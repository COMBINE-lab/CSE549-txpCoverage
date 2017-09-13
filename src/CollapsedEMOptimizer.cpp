#include <atomic>
#include <unordered_map>
#include <vector>

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_for_each.h"
#include "tbb/parallel_reduce.h"
#include "tbb/partitioner.h"
#include "tbb/task_scheduler_init.h"

//#include "fastapprox.h"
#include <boost/math/special_functions/digamma.hpp>

// C++ string formatting library
#include "spdlog/fmt/fmt.h"

#include "Eigen/Dense"
#include "cuckoohash_map.hh"

#include "AlignmentLibrary.hpp"
#include "BootstrapWriter.hpp"
#include "CollapsedEMOptimizer.hpp"
#include "MultinomialSampler.hpp"
#include "ReadExperiment.hpp"
#include "ReadPair.hpp"
#include "SalmonMath.hpp"
#include "Transcript.hpp"
#include "TranscriptGroup.hpp"
#include "UnpairedRead.hpp"

using BlockedIndexRange = tbb::blocked_range<size_t>;

// intelligently chosen value adopted from
// https://github.com/pachterlab/kallisto/blob/master/src/EMAlgorithm.h#L18
constexpr double minEQClassWeight = std::numeric_limits<double>::denorm_min();
constexpr double minWeight = std::numeric_limits<double>::denorm_min();
// A bit more conservative of a minimum as an argument to the digamma function.
constexpr double digammaMin = 1e-10;

double normalize(std::vector<tbb::atomic<double>>& vec) {
  double sum{0.0};
  for (auto& v : vec) {
    sum += v;
  }

  // too small!
  if (sum < minWeight) {
    return sum;
  }

  double invSum = 1.0 / sum;
  for (auto& v : vec) {
    v.store(v.load() * invSum);
  }

  return sum;
}

template <typename VecT>
double truncateCountVector(VecT& alphas, double cutoff) {
  // Truncate tiny expression values
  double alphaSum = 0.0;

  for (size_t i = 0; i < alphas.size(); ++i) {
    if (alphas[i] <= cutoff) {
      alphas[i] = 0.0;
    }
    alphaSum += alphas[i];
  }
  return alphaSum;
}

template <typename VecT>
double truncateCountVector(VecT& alphas, std::vector<double>& cutoff) {
  // Truncate tiny expression values
  double alphaSum = 0.0;

  for (size_t i = 0; i < alphas.size(); ++i) {
    if (alphas[i] <= cutoff[i]) {
      alphas[i] = 0.0;
    }
    alphaSum += alphas[i];
  }
  return alphaSum;
}

/**
 *  Populate the prior parameters for the VBEM
 *  Note: effLens *must* be valid before calling this function.
 */
std::vector<double> populatePriorAlphas_(
                                         std::vector<Transcript>& transcripts, // transcripts
                                         Eigen::VectorXd& effLens, // current effective length estimate
                                         double priorValue,        // the per-nucleotide prior value to use
                                         bool perTranscriptPrior   // true if prior is per-txp, else per-nucleotide
                                         ) {
    // start out with the per-txp prior
    std::vector<double> priorAlphas(transcripts.size(), priorValue);

    // If the prior is per-nucleotide (default, then we need a potentially different
    // value for each transcript based on its length).
    if (!perTranscriptPrior) {
        for (size_t i = 0; i < transcripts.size(); ++i) {
            priorAlphas[i] = priorValue * effLens(i); 
        }
    }
    return priorAlphas;
}


/**
 * Single-threaded EM-update routine for use in bootstrapping
 */
template <typename VecT>
void EMUpdate_(std::vector<std::vector<uint32_t>>& txpGroupLabels,
               std::vector<std::vector<double>>& txpGroupCombinedWeights,
               std::vector<uint64_t>& txpGroupCounts,
               std::vector<Transcript>& transcripts, const VecT& alphaIn,
               VecT& alphaOut) {

  assert(alphaIn.size() == alphaOut.size());

  size_t numEqClasses = txpGroupLabels.size();
  for (size_t eqID = 0; eqID < numEqClasses; ++eqID) {
    uint64_t count = txpGroupCounts[eqID];
    // for each transcript in this class
    const std::vector<uint32_t>& txps = txpGroupLabels[eqID];
    const auto& auxs = txpGroupCombinedWeights[eqID];

    double denom = 0.0;
    size_t groupSize = txps.size();
    // If this is a single-transcript group,
    // then it gets the full count.  Otherwise,
    // update according to our VBEM rule.
    if (BOOST_LIKELY(groupSize > 1)) {
      for (size_t i = 0; i < groupSize; ++i) {
        auto tid = txps[i];
        auto aux = auxs[i];
        double v = alphaIn[tid] * aux;
        denom += v;
      }

      if (denom <= ::minEQClassWeight) {
        // tgroup.setValid(false);
      } else {
        double invDenom = count / denom;
        for (size_t i = 0; i < groupSize; ++i) {
          auto tid = txps[i];
          auto aux = auxs[i];
          double v = alphaIn[tid] * aux;
          if (!std::isnan(v)) {
            salmon::utils::incLoop(alphaOut[tid], v * invDenom);
          }
        }
      }
    } else {
      salmon::utils::incLoop(alphaOut[txps.front()], count);
    }
  }
}

/**
 * Single-threaded VBEM-update routine for use in bootstrapping
 */
template <typename VecT>
void VBEMUpdate_(std::vector<std::vector<uint32_t>>& txpGroupLabels,
                 std::vector<std::vector<double>>& txpGroupCombinedWeights,
                 std::vector<uint64_t>& txpGroupCounts,
                 std::vector<Transcript>& transcripts, std::vector<double>& priorAlphas,
                 double totLen, const VecT& alphaIn, VecT& alphaOut,
                 VecT& expTheta) {

  assert(alphaIn.size() == alphaOut.size());
  size_t M = alphaIn.size();
  size_t numEQClasses = txpGroupLabels.size();
  double alphaSum = {0.0};
  for (size_t i = 0; i < M; ++i) {
    alphaSum +=  alphaIn[i] + priorAlphas[i];
  }

  double logNorm = boost::math::digamma(alphaSum);

  //double prior = priorAlpha;

  for (size_t i = 0; i < M; ++i) {
      auto ap = alphaIn[i] + priorAlphas[i];
    if (ap > ::digammaMin) {
      expTheta[i] = std::exp(boost::math::digamma(ap) - logNorm);
    } else {
      expTheta[i] = 0.0;
    }
    alphaOut[i] = 0.0;//priorAlphas[i];
  }

  for (size_t eqID = 0; eqID < numEQClasses; ++eqID) {
    uint64_t count = txpGroupCounts[eqID];
    const std::vector<uint32_t>& txps = txpGroupLabels[eqID];
    const auto& auxs = txpGroupCombinedWeights[eqID];

    double denom = 0.0;
    size_t groupSize = txps.size();
    // If this is a single-transcript group,
    // then it gets the full count.  Otherwise,
    // update according to our VBEM rule.
    if (BOOST_LIKELY(groupSize > 1)) {
      for (size_t i = 0; i < groupSize; ++i) {
        auto tid = txps[i];
        auto aux = auxs[i];
        if (expTheta[tid] > 0.0) {
          double v = expTheta[tid] * aux;
          denom += v;
        }
      }
      if (denom <= ::minEQClassWeight) {
        // tgroup.setValid(false);
      } else {
        double invDenom = count / denom;
        for (size_t i = 0; i < groupSize; ++i) {
          auto tid = txps[i];
          auto aux = auxs[i];
          if (expTheta[tid] > 0.0) {
            double v = expTheta[tid] * aux;
            salmon::utils::incLoop(alphaOut[tid], v * invDenom);
          }
        }
      }

    } else {
      salmon::utils::incLoop(alphaOut[txps.front()], count);
    }
  }
}

/*
 * Use the "standard" EM algorithm over equivalence
 * classes to estimate the latent variables (alphaOut)
 * given the current estimates (alphaIn).
 */
void EMUpdate_(std::vector<std::pair<const TranscriptGroup, TGValue>>& eqVec,
               std::vector<Transcript>& transcripts,
               const CollapsedEMOptimizer::VecType& alphaIn,
               CollapsedEMOptimizer::VecType& alphaOut) {

  assert(alphaIn.size() == alphaOut.size());

  tbb::parallel_for(
      BlockedIndexRange(size_t(0), size_t(eqVec.size())),
      [&eqVec, &alphaIn, &alphaOut](const BlockedIndexRange& range) -> void {
        for (auto eqID : boost::irange(range.begin(), range.end())) {
          auto& kv = eqVec[eqID];

          uint64_t count = kv.second.count;
          // for each transcript in this class
          const TranscriptGroup& tgroup = kv.first;
          if (tgroup.valid) {
            const std::vector<uint32_t>& txps = tgroup.txps;
            const auto& auxs = kv.second.combinedWeights;

            double denom = 0.0;
            size_t groupSize = txps.size();
            // If this is a single-transcript group,
            // then it gets the full count.  Otherwise,
            // update according to our VBEM rule.
            if (BOOST_LIKELY(groupSize > 1)) {
              for (size_t i = 0; i < groupSize; ++i) {
                auto tid = txps[i];
                auto aux = auxs[i];
                double v = alphaIn[tid] * aux;
                denom += v;
              }

              if (denom <= ::minEQClassWeight) {
                // tgroup.setValid(false);
              } else {
                double invDenom = count / denom;
                for (size_t i = 0; i < groupSize; ++i) {
                  auto tid = txps[i];
                  auto aux = auxs[i];
                  double v = alphaIn[tid] * aux;
                  if (!std::isnan(v)) {
                    salmon::utils::incLoop(alphaOut[tid], v * invDenom);
                  }
                }
              }
            } else {
              salmon::utils::incLoop(alphaOut[txps.front()], count);
            }
          }
        }
      });
}

/*
 * Use the Variational Bayesian EM algorithm over equivalence
 * classes to estimate the latent variables (alphaOut)
 * given the current estimates (alphaIn).
 */
void VBEMUpdate_(std::vector<std::pair<const TranscriptGroup, TGValue>>& eqVec,
                 std::vector<Transcript>& transcripts, std::vector<double>& priorAlphas,
                 double totLen, const CollapsedEMOptimizer::VecType& alphaIn,
                 CollapsedEMOptimizer::VecType& alphaOut,
                 CollapsedEMOptimizer::VecType& expTheta) {

  assert(alphaIn.size() == alphaOut.size());
  size_t M = alphaIn.size();
  double alphaSum = {0.0};
  for (size_t i = 0; i < M; ++i) {
      alphaSum +=  alphaIn[i] + priorAlphas[i];
  }

  double logNorm = boost::math::digamma(alphaSum);

  tbb::parallel_for(BlockedIndexRange(size_t(0), size_t(transcripts.size())),
                    [logNorm, totLen, &priorAlphas, &alphaIn, &alphaOut,
                     &expTheta](const BlockedIndexRange& range) -> void {

                      //double prior = priorAlpha;

                      for (auto i : boost::irange(range.begin(), range.end())) {
                          auto ap = alphaIn[i].load() + priorAlphas[i];
                          if (ap > ::digammaMin) {
                              expTheta[i] = std::exp(boost::math::digamma(ap) - logNorm);
                          } else {
                              expTheta[i] = 0.0;
                          }
                          //alphaOut[i] = prior * transcripts[i].RefLength;
                          alphaOut[i] = 0.0;
                      }
                    });

  tbb::parallel_for(
      BlockedIndexRange(size_t(0), size_t(eqVec.size())),
      [&eqVec, &alphaIn, &alphaOut,
       &expTheta](const BlockedIndexRange& range) -> void {
        for (auto eqID : boost::irange(range.begin(), range.end())) {
          auto& kv = eqVec[eqID];

          uint64_t count = kv.second.count;
          // for each transcript in this class
          const TranscriptGroup& tgroup = kv.first;
          if (tgroup.valid) {
            const std::vector<uint32_t>& txps = tgroup.txps;
            const auto& auxs = kv.second.combinedWeights;

            double denom = 0.0;
            size_t groupSize = txps.size();
            // If this is a single-transcript group,
            // then it gets the full count.  Otherwise,
            // update according to our VBEM rule.
            if (BOOST_LIKELY(groupSize > 1)) {
              for (size_t i = 0; i < groupSize; ++i) {
                auto tid = txps[i];
                auto aux = auxs[i];
                if (expTheta[tid] > 0.0) {
                  double v = expTheta[tid] * aux;
                  denom += v;
                }
              }
              if (denom <= ::minEQClassWeight) {
                // tgroup.setValid(false);
              } else {
                double invDenom = count / denom;
                for (size_t i = 0; i < groupSize; ++i) {
                  auto tid = txps[i];
                  auto aux = auxs[i];
                  if (expTheta[tid] > 0.0) {
                    double v = expTheta[tid] * aux;
                    salmon::utils::incLoop(alphaOut[tid], v * invDenom);
                  }
                }
              }

            } else {
              salmon::utils::incLoop(alphaOut[txps.front()], count);
            }
          }
        }
      });
}

template <typename VecT>
size_t markDegenerateClasses(
    std::vector<std::pair<const TranscriptGroup, TGValue>>& eqVec,
    VecT& alphaIn, Eigen::VectorXd& effLens, std::vector<bool>& available, 
    std::shared_ptr<spdlog::logger> jointLog, bool verbose = false) {

  size_t numDropped{0};
  size_t idx{0};
  for (auto& kv : eqVec) {
    uint64_t count = kv.second.count;
    // for each transcript in this class
    const TranscriptGroup& tgroup = kv.first;
    const std::vector<uint32_t>& txps = tgroup.txps;
    const auto& auxs = kv.second.combinedWeights;

    double denom = 0.0;
    for (size_t i = 0; i < txps.size(); ++i) {
      auto tid = txps[i];
      auto aux = auxs[i];
      double v = alphaIn[tid] * aux;
      if (!std::isnan(v)) {
        denom += v;
      } else {
        std::cerr << "val is NAN; alpha( " << tid << " ) = " << alphaIn[tid]
                  << ", aux = " << aux << "\n";
      }
    }
    if (denom <= minEQClassWeight) {
      fmt::MemoryWriter errstream;

      errstream << "\nDropping weighted eq class\n";
      errstream << "============================\n";

      errstream << "denom = 0, count = " << count << "\n";
      errstream << "class = { ";
      for (auto e : txps) {
        errstream << e << " ";
      }
      errstream << "}\n";
      errstream << "alphas = { ";
      for (auto e : txps) {
        errstream << alphaIn[e] << " ";
      }
      errstream << "}\n";
      errstream << "weights = { ";
      for (auto e : auxs) {
        errstream << e << " ";
      }
      errstream << "}\n";
      errstream << "============================\n\n";

      bool verbose{false};
      if (verbose) {
        jointLog->info(errstream.str());
      }
      ++numDropped;
      kv.first.setValid(false);
    } else {
      for (size_t i = 0; i < txps.size(); ++i) {
        auto tid = txps[i];
        available[tid] = true;
      }
    }
  }
  return numDropped;
}

CollapsedEMOptimizer::CollapsedEMOptimizer() {}

bool doBootstrap(
    std::vector<std::vector<uint32_t>>& txpGroups,
    std::vector<std::vector<double>>& txpGroupCombinedWeights,
    std::vector<Transcript>& transcripts, Eigen::VectorXd& effLens,
    const std::vector<double>& sampleWeights, uint64_t totalNumFrags,
    uint64_t numMappedFrags, double uniformTxpWeight,
    std::atomic<uint32_t>& bsNum, SalmonOpts& sopt,
    std::vector<double>& priorAlphas,
    std::function<bool(const std::vector<double>&)>& writeBootstrap,
    double relDiffTolerance, uint32_t maxIter) {

  // An EM termination criterion, adopted from Bray et al. 2016
  uint32_t minIter = 50;

  // Determine up front if we're going to use scaled counts.
  bool useScaledCounts = !(sopt.useQuasi or sopt.allowOrphans);
  bool useVBEM{sopt.useVBOpt};
  size_t numClasses = txpGroups.size();
  CollapsedEMOptimizer::SerialVecType alphas(transcripts.size(), 0.0);
  CollapsedEMOptimizer::SerialVecType alphasPrime(transcripts.size(), 0.0);
  CollapsedEMOptimizer::SerialVecType expTheta(transcripts.size(), 0.0);
  std::vector<uint64_t> sampCounts(numClasses, 0);

  uint32_t numBootstraps = sopt.numBootstraps;
  bool perTranscriptPrior{sopt.perTranscriptPrior};

  auto& jointLog = sopt.jointLog;

  std::random_device rd;
  std::mt19937 gen(rd());
  //MultinomialSampler msamp(rd);
  std::discrete_distribution<uint64_t> csamp(sampleWeights.begin(), sampleWeights.end());
  while (bsNum++ < numBootstraps) {
    csamp.reset();

    for (size_t sc = 0; sc < sampCounts.size(); ++sc) {
      sampCounts[sc] = 0;
    }
    for (size_t fn = 0; fn < totalNumFrags; ++fn) {
      ++sampCounts[csamp(gen)];
    }
    // Do a new bootstrap
    //msamp(sampCounts.begin(), totalNumFrags, numClasses, sampleWeights.begin());

    double totalLen{0.0};
    for (size_t i = 0; i < transcripts.size(); ++i) {
      alphas[i] =
          transcripts[i].getActive() ? uniformTxpWeight * totalNumFrags : 0.0;
      totalLen += effLens(i);
    }

    bool converged{false};
    double maxRelDiff = -std::numeric_limits<double>::max();
    size_t itNum = 0;

    // If we use VBEM, we'll need the prior parameters
    //double priorAlpha = 1.00;

    // EM termination criteria, adopted from Bray et al. 2016
    double minAlpha = 1e-8;
    double alphaCheckCutoff = 1e-2;
    double cutoff = minAlpha;

    while (itNum < minIter or (itNum < maxIter and !converged)) {

      if (useVBEM) {
        VBEMUpdate_(txpGroups, txpGroupCombinedWeights, sampCounts, transcripts,
                    priorAlphas, totalLen, alphas, alphasPrime, expTheta);
      } else {
        EMUpdate_(txpGroups, txpGroupCombinedWeights, sampCounts, transcripts,
                  alphas, alphasPrime);
      }

      converged = true;
      maxRelDiff = -std::numeric_limits<double>::max();
      for (size_t i = 0; i < transcripts.size(); ++i) {
        if (alphasPrime[i] > alphaCheckCutoff) {
          double relDiff =
              std::abs(alphas[i] - alphasPrime[i]) / alphasPrime[i];
          maxRelDiff = (relDiff > maxRelDiff) ? relDiff : maxRelDiff;
          if (relDiff > relDiffTolerance) {
            converged = false;
          }
        }
        alphas[i] = alphasPrime[i];
        alphasPrime[i] = 0.0;
      }

      ++itNum;
    }

    // Truncate tiny expression values
    double alphaSum = 0.0;
    if (useVBEM and !perTranscriptPrior) {
        std::vector<double> cutoffs(transcripts.size(), 0.0);
        for (size_t i = 0; i < transcripts.size(); ++i) {
            cutoffs[i] = minAlpha;
        }
        //alphaSum = truncateCountVector(alphas, cutoffs);
        alphaSum = truncateCountVector(alphas, cutoffs);
    } else {
        // Truncate tiny expression values
        alphaSum = truncateCountVector(alphas, cutoff);
    }


    if (alphaSum < minWeight) {
      jointLog->error("Total alpha weight was too small! "
                      "Make sure you ran salmon correclty.");
      return false;
    }

    if (useScaledCounts) {
      double mappedFragsDouble = static_cast<double>(numMappedFrags);
      double alphaSum = 0.0;
      for (auto a : alphas) {
        alphaSum += a;
      }
      if (alphaSum > ::minWeight) {
        double scaleFrac = 1.0 / alphaSum;
        // scaleFrac converts alpha to nucleotide fraction,
        // and multiplying by numMappedFrags scales by the total
        // number of mapped fragments to provide an estimated count.
        for (auto& a : alphas) {
          a = mappedFragsDouble * (a * scaleFrac);
        }
      } else { // This shouldn't happen!
        sopt.jointLog->error(
            "Bootstrap had insufficient number of fragments!"
            "Something is probably wrong; please check that you "
            "have run salmon correctly and report this to GitHub.");
      }
    }
    writeBootstrap(alphas);
  }
  return true;
}

template <typename ExpT>
bool CollapsedEMOptimizer::gatherBootstraps(
    ExpT& readExp, SalmonOpts& sopt,
    std::function<bool(const std::vector<double>&)>& writeBootstrap,
    double relDiffTolerance, uint32_t maxIter) {

  std::vector<Transcript>& transcripts = readExp.transcripts();
  std::vector<bool> available(transcripts.size(), false);
  using VecT = CollapsedEMOptimizer::SerialVecType;
  // With atomics
  VecT alphas(transcripts.size(), 0.0);
  VecT alphasPrime(transcripts.size(), 0.0);
  VecT expTheta(transcripts.size());
  Eigen::VectorXd effLens(transcripts.size());

  bool scaleCounts = (!sopt.useQuasi and !sopt.allowOrphans);

  uint64_t numMappedFrags =
      scaleCounts ? readExp.upperBoundHits() : readExp.numMappedFragments();

  uint32_t numBootstraps = sopt.numBootstraps;

  std::vector<std::pair<const TranscriptGroup, TGValue>>& eqVec =
      readExp.equivalenceClassBuilder().eqVec();

  std::unordered_set<uint32_t> activeTranscriptIDs;
  for (auto& kv : eqVec) {
    auto& tg = kv.first;
    for (auto& t : tg.txps) {
      transcripts[t].setActive();
      activeTranscriptIDs.insert(t);
    }
  }

  bool useVBEM{sopt.useVBOpt};
  bool perTranscriptPrior{sopt.perTranscriptPrior};
  double priorValue{sopt.vbPrior};

  auto jointLog = sopt.jointLog;

  jointLog->info("Will draw {} bootstrap samples", numBootstraps);
  jointLog->info("Optimizing over {} equivalence classes", eqVec.size());

  double totalNumFrags{static_cast<double>(numMappedFrags)};
  double totalLen{0.0};

  if (activeTranscriptIDs.size() == 0) {
    jointLog->error("It seems that no transcripts are expressed; something is "
                    "likely wrong!");
    std::exit(1);
  }

  double scale = 1.0 / activeTranscriptIDs.size();
  for (size_t i = 0; i < transcripts.size(); ++i) {
    // double m = transcripts[i].mass(false);
    alphas[i] = transcripts[i].getActive() ? scale * totalNumFrags : 0.0;
    effLens(i) = (sopt.noEffectiveLengthCorrection)
                     ? transcripts[i].RefLength
                     : transcripts[i].EffectiveLength;
    totalLen += effLens(i);
  }

  
  // If we use VBEM, we'll need the prior parameters
  std::vector<double> priorAlphas = populatePriorAlphas_(transcripts, effLens, priorValue, perTranscriptPrior);

  auto numRemoved =
    markDegenerateClasses(eqVec, alphas, effLens, available, sopt.jointLog);
  sopt.jointLog->info("Marked {} weighted equivalence classes as degenerate",
                      numRemoved);

  size_t itNum{0};

  // EM termination criterion, adopted from Bray et al. 2016
  double minAlpha = 1e-8;
  double cutoff = minAlpha;

  // Since we will use the same weights and transcript groups for each
  // of the bootstrap samples (only the count vector will change), it
  // makes sense to keep only one copy of these.
  using TGroupLabelT = std::vector<uint32_t>;
  using TGroupWeightVec = std::vector<double>;
  std::vector<TGroupLabelT> txpGroups;
  std::vector<TGroupWeightVec> txpGroupCombinedWeights;
  std::vector<uint64_t> origCounts;
  uint64_t totalCount{0};

  for (auto& kv : eqVec) {
    uint64_t count = kv.second.count;
    // for each transcript in this class
    const TranscriptGroup& tgroup = kv.first;
    if (tgroup.valid) {
      const std::vector<uint32_t>& txps = tgroup.txps;
      const auto& auxs = kv.second.combinedWeights;
      txpGroups.push_back(txps);
      // Convert to non-atomic
      txpGroupCombinedWeights.emplace_back(auxs.begin(), auxs.end());
      origCounts.push_back(count);
      totalCount += count;
    }
  }

  double floatCount = totalCount;
  std::vector<double> samplingWeights(txpGroups.size(), 0.0);
  for (size_t i = 0; i < origCounts.size(); ++i) {
    samplingWeights[i] = origCounts[i] / floatCount;
  }

  size_t numWorkerThreads{1};
  if (sopt.numThreads > 1 and numBootstraps > 1) {
    numWorkerThreads = std::min(sopt.numThreads - 1, numBootstraps - 1);
  }

  std::atomic<uint32_t> bsCounter{0};
  std::vector<std::thread> workerThreads;
  for (size_t tn = 0; tn < numWorkerThreads; ++tn) {
    workerThreads.emplace_back(
        doBootstrap, std::ref(txpGroups), std::ref(txpGroupCombinedWeights),
        std::ref(transcripts), std::ref(effLens), std::ref(samplingWeights),
        totalCount, numMappedFrags, scale, std::ref(bsCounter), std::ref(sopt),
	std::ref(priorAlphas), std::ref(writeBootstrap), relDiffTolerance, maxIter);
  }

  for (auto& t : workerThreads) {
    t.join();
  }
  return true;
}

void updateEqClassWeights(
    std::vector<std::pair<const TranscriptGroup, TGValue>>& eqVec,
    Eigen::VectorXd& effLens) {
  tbb::parallel_for(
      BlockedIndexRange(size_t(0), size_t(eqVec.size())),
      [&eqVec, &effLens](const BlockedIndexRange& range) -> void {
        // For each index in the equivalence class vector
        for (auto eqID : boost::irange(range.begin(), range.end())) {
          // The vector entry
          auto& kv = eqVec[eqID];
          // The label of the equivalence class
          const TranscriptGroup& k = kv.first;
          // The size of the label
          size_t classSize = k.txps.size();
          // The weights of the label
          TGValue& v = kv.second;

          // Iterate over each weight and set it equal to
          // 1 / effLen of the corresponding transcript
          double wsum{0.0};
          for (size_t i = 0; i < classSize; ++i) {
            auto tid = k.txps[i];
            auto probStartPos = 1.0 / effLens(tid);
            v.combinedWeights[i] =
                kv.second.count * (v.weights[i] * probStartPos);
            wsum += v.combinedWeights[i];
          }
          double wnorm = 1.0 / wsum;
          for (size_t i = 0; i < classSize; ++i) {
            v.combinedWeights[i] *= wnorm;
          }
        }
      });
}

template <typename ExpT>
bool CollapsedEMOptimizer::optimize(ExpT& readExp, SalmonOpts& sopt,
                                    double relDiffTolerance, uint32_t maxIter) {

  tbb::task_scheduler_init tbbScheduler(sopt.numThreads);
  std::vector<Transcript>& transcripts = readExp.transcripts();
  std::vector<bool> available(transcripts.size(), false);

  // An EM termination criterion, adopted from Bray et al. 2016
  uint32_t minIter = 50;
  bool seqBiasCorrect = sopt.biasCorrect;
  bool gcBiasCorrect = sopt.gcBiasCorrect;
  bool posBiasCorrect = sopt.posBiasCorrect;
  bool doBiasCorrect = seqBiasCorrect or gcBiasCorrect or posBiasCorrect;
  bool metaGenomeMode = sopt.meta;
  bool altInitMode = sopt.alternativeInitMode;

  using VecT = CollapsedEMOptimizer::VecType;
  // With atomics
  VecType alphas(transcripts.size(), 0.0);
  VecType alphasPrime(transcripts.size(), 0.0);
  VecType expTheta(transcripts.size());

  Eigen::VectorXd effLens(transcripts.size());

  std::vector<std::pair<const TranscriptGroup, TGValue>>& eqVec =
      readExp.equivalenceClassBuilder().eqVec();

  bool noRichEq = sopt.noRichEqClasses;
  bool useFSPD{sopt.useFSPD};

  bool useVBEM{sopt.useVBOpt};
  bool perTranscriptPrior{sopt.perTranscriptPrior};
  double priorValue{sopt.vbPrior};
  
  auto jointLog = sopt.jointLog;

  auto& fragStartDists = readExp.fragmentStartPositionDistributions();
  double totalNumFrags{static_cast<double>(readExp.numMappedFragments())};
  double totalLen{0.0};

  // If effective length correction isn't turned off, then use effective
  // lengths rather than reference lengths.
  bool useEffectiveLengths = !sopt.noEffectiveLengthCorrection;

  int64_t numActive{0};
  double totalWeight{0.0};

  for (size_t i = 0; i < transcripts.size(); ++i) {
    auto& txp = transcripts[i];
    alphas[i] = txp.projectedCounts;


    effLens(i) = useEffectiveLengths
                     ? std::exp(txp.getCachedLogEffectiveLength())
                     : txp.RefLength;
    if (sopt.noLengthCorrection) { effLens(i) = 100.0; }
    txp.EffectiveLength = effLens(i);

    double uniqueCount = static_cast<double>(txp.uniqueCount() + 0.5);
    auto wi = uniqueCount * 1e-3 * effLens(i);
    alphasPrime[i] = wi;
    totalWeight += wi; 
    ++numActive;
    totalLen += effLens(i);
  }

  // If we use VBEM, we'll need the prior parameters
  std::vector<double> priorAlphas = populatePriorAlphas_(transcripts, effLens, priorValue, perTranscriptPrior);

  // Based on the number of observed reads, use
  // a linear combination of the online estimates
  // and the uniform distribution.
  double uniformPrior = totalWeight / static_cast<double>(numActive);
  // double fracObserved = 1.0;
  double fracObserved = std::min(1.0, totalWeight / sopt.numRequiredFragments);
  // Above, we placed the uniformative (uniform) initalization into the alphasPrime
  // variables.  If that's what the user requested, then copy those over to the alphas
  if (sopt.initUniform) { 
    for (size_t i = 0; i < alphas.size(); ++i) {
      alphas[i] = alphasPrime[i];
      alphasPrime[i] = 1.0;
    } 
  } else { // otherwise, initalize with a linear combination of the true and uniform alphas 
      for (size_t i = 0; i < alphas.size(); ++i) {
        auto uniAbund = (metaGenomeMode or altInitMode) ? alphasPrime[i].load() : uniformPrior;
        alphas[i] = (alphas[i] * fracObserved) + (uniAbund * (1.0 - fracObserved));
        alphasPrime[i] = 1.0;
      }
  }

  // If the user requested *not* to use "rich" equivalence classes,
  // then wipe out all of the weight information here and simply replace
  // the weights with the effective length terms (here, the *inverse* of
  // the effective length).  Otherwise, multiply the existing weight terms
  // by the effective length term.
  tbb::parallel_for(
      BlockedIndexRange(size_t(0), size_t(eqVec.size())),
      [&eqVec, &effLens, noRichEq](const BlockedIndexRange& range) -> void {
        // For each index in the equivalence class vector
        for (auto eqID : boost::irange(range.begin(), range.end())) {
          // The vector entry
          auto& kv = eqVec[eqID];
          // The label of the equivalence class
          const TranscriptGroup& k = kv.first;
          // The size of the label
          size_t classSize = k.txps.size();
          // The weights of the label
          TGValue& v = kv.second;

          // Iterate over each weight and set it
          double wsum{0.0};

          for (size_t i = 0; i < classSize; ++i) {
            auto tid = k.txps[i];
            double el = effLens(tid);
            if (el <= 1.0) {
              el = 1.0;
            }
            if (noRichEq) {
              // Keep length factor separate for the time being
              v.weights[i] = 1.0;
            }
            // meaningful values.
            auto probStartPos = 1.0 / el;

            // combined weight
            v.combinedWeights.push_back(
                v.count * v.weights[i] * probStartPos);
            wsum += v.combinedWeights.back();
          }

          double wnorm = 1.0 / wsum;
          for (size_t i = 0; i < classSize; ++i) {
            v.combinedWeights[i] = v.combinedWeights[i] * wnorm;
          }
        }
      });

  auto numRemoved =
    markDegenerateClasses(eqVec, alphas, effLens, available, sopt.jointLog);
  sopt.jointLog->info("Marked {} weighted equivalence classes as degenerate",
                      numRemoved);

  size_t itNum{0};

  // EM termination criteria, adopted from Bray et al. 2016
  double minAlpha = 1e-8;
  double alphaCheckCutoff = 1e-2;
  double cutoff = minAlpha;

  // Iterations in which we will allow re-computing the effective lengths
  // if bias-correction is enabled.
  // std::vector<uint32_t> recomputeIt{100, 500, 1000};
  //minIter = 100;

  bool converged{false};
  double maxRelDiff = -std::numeric_limits<double>::max();
  bool needBias = doBiasCorrect;
  size_t targetIt{10};
  /* -- v0.8.x
  double alphaSum = 0.0;
  */

  while (itNum < minIter or (itNum < maxIter and !converged) or needBias) {
    if (needBias and (itNum > targetIt or converged)) {

      jointLog->info("iteration {}, adjusting effective lengths to account for biases", itNum);
      effLens = salmon::utils::updateEffectiveLengths(sopt, readExp, effLens,
                                                      alphas, available, true);
      // if we're doing the VB optimization, update the priors
      if (useVBEM) {
          priorAlphas = populatePriorAlphas_(transcripts, effLens, priorValue, perTranscriptPrior);
      }

      // Check for strangeness with the lengths.
      for (size_t i = 0; i < effLens.size(); ++i) {
        if (effLens(i) <= 0.0) {
          jointLog->warn("Transcript {} had length {}", i, effLens(i));
        }
      }
      updateEqClassWeights(eqVec, effLens);
      needBias = false;
    }

    if (useVBEM) {
      VBEMUpdate_(eqVec, transcripts, priorAlphas, totalLen, alphas, alphasPrime,
                  expTheta);
    } else {
      EMUpdate_(eqVec, transcripts, alphas, alphasPrime);
    }

    converged = true;
    maxRelDiff = -std::numeric_limits<double>::max();
    for (size_t i = 0; i < transcripts.size(); ++i) {
      if (alphasPrime[i] > alphaCheckCutoff) {
        double relDiff = std::abs(alphas[i] - alphasPrime[i]) / alphasPrime[i];
        maxRelDiff = (relDiff > maxRelDiff) ? relDiff : maxRelDiff;
        if (relDiff > relDiffTolerance) {
          converged = false;
        }
      }
      alphas[i] = alphasPrime[i];
      alphasPrime[i] = 0.0;
    }

    /* -- v0.8.x
    if (converged and itNum > minIter and !needBias) {
      if (useVBEM and !perTranscriptPrior) {
        std::vector<double> cutoffs(transcripts.size(), 0.0);
        for (size_t i = 0; i < transcripts.size(); ++i) {
          cutoffs[i] = minAlpha;
        }
        alphaSum = truncateCountVector(alphas, cutoffs);
      } else {
        // Truncate tiny expression values
        alphaSum = truncateCountVector(alphas, cutoff);
      }
      if (useVBEM) {
        VBEMUpdate_(eqVec, transcripts, priorAlphas, totalLen, alphas, alphasPrime,
                    expTheta);
      } else {
        EMUpdate_(eqVec, transcripts, alphas, alphasPrime);
      }
      for (size_t i = 0; i < transcripts.size(); ++i) {
        alphas[i] = alphasPrime[i];
        alphasPrime[i] = 0.0;
      }
    }
    */

    if (itNum % 100 == 0) {
      jointLog->info("iteration = {} | max rel diff. = {}", itNum, maxRelDiff);
    }

    ++itNum;
  }

  /* -- v0.8.x
  if (alphaSum < minWeight) {
    jointLog->error("Total alpha weight was too small! "
                    "Make sure you ran salmon correclty.");
    return false;
  }
  */

  // Reset the original bias correction options
  sopt.gcBiasCorrect = gcBiasCorrect;
  sopt.biasCorrect = seqBiasCorrect;

  jointLog->info("iteration = {} | max rel diff. = {}", itNum, maxRelDiff);

  double alphaSum = 0.0;
  if (useVBEM and !perTranscriptPrior) {
      std::vector<double> cutoffs(transcripts.size(), 0.0);
      for (size_t i = 0; i < transcripts.size(); ++i) {
          cutoffs[i] = minAlpha;
      }
      alphaSum = truncateCountVector(alphas, cutoffs);
  } else {
      // Truncate tiny expression values
    alphaSum = truncateCountVector(alphas, cutoff);
  }

  if (alphaSum < minWeight) {
    jointLog->error("Total alpha weight was too small! "
                    "Make sure you ran salmon correclty.");
    return false;
  }
  

  // Set the mass of each transcript using the
  // computed alphas.
  for (size_t i = 0; i < transcripts.size(); ++i) {
    // Set the mass to the normalized (after truncation)
    // relative abundance
    // If we changed the effective lengths, copy them over here
    if (doBiasCorrect) {
      transcripts[i].EffectiveLength = effLens(i);
    }
    transcripts[i].setSharedCount(alphas[i]);
    transcripts[i].setMass(alphas[i] / alphaSum);
  }
  return true;
}

template bool CollapsedEMOptimizer::optimize<ReadExperiment>(
    ReadExperiment& readExp, SalmonOpts& sopt, double relDiffTolerance,
    uint32_t maxIter);

template bool CollapsedEMOptimizer::optimize<AlignmentLibrary<UnpairedRead>>(
    AlignmentLibrary<UnpairedRead>& readExp, SalmonOpts& sopt,
    double relDiffTolerance, uint32_t maxIter);

template bool CollapsedEMOptimizer::optimize<AlignmentLibrary<ReadPair>>(
    AlignmentLibrary<ReadPair>& readExp, SalmonOpts& sopt,
    double relDiffTolerance, uint32_t maxIter);

template bool CollapsedEMOptimizer::gatherBootstraps<ReadExperiment>(
    ReadExperiment& readExp, SalmonOpts& sopt,
    std::function<bool(const std::vector<double>&)>& writeBootstrap,
    double relDiffTolerance, uint32_t maxIter);

template bool
CollapsedEMOptimizer::gatherBootstraps<AlignmentLibrary<UnpairedRead>>(
    AlignmentLibrary<UnpairedRead>& readExp, SalmonOpts& sopt,
    std::function<bool(const std::vector<double>&)>& writeBootstrap,
    double relDiffTolerance, uint32_t maxIter);

template bool
CollapsedEMOptimizer::gatherBootstraps<AlignmentLibrary<ReadPair>>(
    AlignmentLibrary<ReadPair>& readExp, SalmonOpts& sopt,
    std::function<bool(const std::vector<double>&)>& writeBootstrap,
    double relDiffTolerance, uint32_t maxIter);

// Unused / old
