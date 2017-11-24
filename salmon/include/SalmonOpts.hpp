#ifndef SALMON_OPTS_HPP
#define SALMON_OPTS_HPP

#include <boost/filesystem.hpp>

// Logger includes
#include "spdlog/spdlog.h"

#include <fstream>
#include <ostream>
#include <memory> // for shared_ptr


enum class SalmonQuantMode { MAP = 1, ALIGN = 2 };

/**
  * A structure to hold some common options used
  * by Salmon so that we don't have to pass them
  * all around as separate arguments.
  */
struct SalmonOpts {
    // The options below are adopted from the mem_opt_t structure of BWA
    /*
    int maxOccurences; // maximum number of allowable occurences of (S)MEM
    int minSeedOccurences; // try to split a seed into smaller seeds if it occurs
                           // fewer than this many times.
    int minSeedLen; // A seed must be at least this long.
    float splitFactor; // Split a seed if it's longer than splitFactor * minSeedLen.
    int flag; // Used by bwa
    bool maxMEMIntervals; // If true, don't split (S)MEMs into MEMs
    */

  SalmonOpts() : quantMode(SalmonQuantMode::MAP), alternativeInitMode(false),
                   allowOrphans(false), splitSpanningSeeds(false),
                   noFragLengthDist(false), noEffectiveLengthCorrection(false),
                   useReadCompat(false), maxReadOccs(200), extraSeedPass(false),
                   mappingCacheMemoryLimit(5000000), useQuasi(false), dumpAlignments(false) {}

  SalmonQuantMode quantMode; // How quantification is done

    bool alternativeInitMode; // Weigh unique reads more heavily when initialzing the optimization.

    bool allowOrphans; // Consider orphaned reads when performing lightweight alignemnt.

    std::string auxDir; // The directory where auxiliary files will be written.

  std::string runStartTime; // String representation of the date / time at which the run began.

  std::string runStopTime; // String representation of the date / time at which the run ended.

    bool consistentHits;  // Enforce consistency of hits gathered during quasi-mapping.

    bool dumpEq; 	     // Dump the equivalence classes and counts to file

    bool dumpEqWeights; 	     // Dump the equivalence classes rich weights 

    bool fasterMapping; // [Developer]: Disables some extra checks during quasi-mapping. This may make mapping a 
                        // little bit faster at the potential cost of returning too many mappings (i.e. some sub-optimal mappings) 
                        // for certain reads. Only use this option if you know what it does (enables NIP-skipping)

    bool gencodeRef; // The reference is expected to be from Gencode.

    double quasiCoverage; // [Experimental]: Default of 0.  The coverage by MMPs required for a read to be considered mapped.

    bool splitSpanningSeeds; // Attempt to split seeds that span multiple transcripts.

    bool noFragLengthDist ; // Don't give a fragment assignment a likelihood based on an emperically
	                    // observed fragment length distribution.

    bool noEffectiveLengthCorrection; // Don't take the fragment length distribution into
                                      // account when computing the probability that a
                                     // fragment was generated from a transcript.

    bool noLengthCorrection; // Don't account for transcript length at all during abundance estimation.

    bool noBiasLengthThreshold; // Don't require that the recomputed effective length for a target
                                // be above a threshold before applying it.
    bool useBiasLengthThreshold; // Don't require that the recomputed effective length for a target
                                // be above a threshold before applying it.

    bool rankEqClasses; // Keep a separate equivalence class for each ordering (by conditional probability) 
                        // of the the transcript labels 
    
    bool useFSPD; // Learn a non-uniform start distribution

    bool noSeqBiasModel; // Don't learn and use a sequence-specific bias model.

    bool noRichEqClasses; // Don't use rich equivalence classes --- forget the
                          // aggregate weights for each transcript to each
                          // equivalence class of fragments.

    bool noFragLenFactor; // Don't account for the fragment length term in the
                          // likelihood.

    bool useReadCompat; // Give a fragment assignment a likelihood based on the compatibility
                        // between the manner in which it mapped and the expected read
                        // library format.

    double incompatPrior; // The prior probability that an alignment that disagrees with
                          // the provided library type could correspond to the true
                          // fragment origin.

    bool ignoreIncompat; // If incompatPrior is 0, this flag is set to true and we completely 
                         // ignore incompatible fragments.

    bool useErrorModel; // Learn and apply the error model when computing the likelihood
                        // of a given alignment.

    uint32_t numErrorBins; // Number of bins into which each read is divided
                           // when learning and applying the error model.

    double forgettingFactor; // The forgetting factor used to determine the
                             // learning schedule in the online inference algorithm.

    uint32_t numBurninFrags; // Number of mapped fragments required for burn-in

    uint32_t numPreBurninFrags; // Number of mapped fragments that are evaluated before
    				// applying the auxiliary models.

    uint32_t maxReadOccs; // Discard reads  mapping to more than this many places.

    uint32_t maxExpectedReadLen; // Maximum expected length of an observed read.

    uint64_t numRequiredFragments; //
  uint64_t minRequiredFrags;
    bool reduceGCMemory;  // Use a memory-efficient (rank-based) data structure for computing fragment GC content
  //uint32_t gcSampFactor; // The factor by which to down-sample the GC distribution of transcripts
    uint32_t pdfSampFactor; // The factor by which to down-sample the fragment length pmf when
                            // evaluating gc-bias for effective length correction.

    bool strictIntersect; // Use strict rather than fuzzy intersection in quasi-mapping
    bool useMassBanking; // DEPRECATED

    bool sensitive; // Perform splitting of long SMEMs into MEMs

    bool extraSeedPass; // Perform extra pass trying to find seeds to cover the read

    bool disableMappingCache; // Don't write mapping results to temporary mapping cache file
    
    bool meta; // Set other options to be optimized for metagenomic data

  boost::filesystem::path outputDirectory; // Quant output directory

  boost::filesystem::path paramsDirectory; // Parameters directory

    boost::filesystem::path indexDirectory; // Index directory

    boost::filesystem::path geneMapPath; // Gene map path 
    
    bool quiet; // Be quiet during quantification.

    bool useVBOpt; // Use Variational Bayesian EM instead of "regular" EM in the batch passes

    bool useQuasi; // Are we using the quasi-mapping based index or not.
    
    // For writing quasi-mappings
    std::string qmFileName;
    std::ofstream qmFile;
    std::unique_ptr<std::ostream> qmStream{nullptr};
    std::shared_ptr<spdlog::logger> qmLog{nullptr};

    bool dumpAlignments; // Dump alignment groups

    // For writing quasi-mapped alignments
    std::string daFileName;
    std::ofstream daFile;
    std::unique_ptr<std::ostream> daStream{nullptr};
    std::shared_ptr<spdlog::logger> daLog{nullptr};


  std::unique_ptr<std::ofstream> unmappedFile{nullptr};
    bool writeUnmappedNames; // write the names of unmapped reads
    std::shared_ptr<spdlog::logger> unmappedLog{nullptr};
    
    std::unique_ptr<std::ofstream> orphanLinkFile{nullptr};
    bool writeOrphanLinks; // write the names of unmapped reads
    std::shared_ptr<spdlog::logger> orphanLinkLog{nullptr};

    bool sampleOutput; // Sample alignments according to posterior estimates of transcript abundance.
    bool sampleUnaligned; // Pass along un-aligned reads in the sampling.

    uint32_t numGibbsSamples; // Number of rounds of Gibbs sampling to perform
    uint32_t numBootstraps; // Number of bootstrap samples to draw
    uint32_t thinningFactor; // Gibbs chain thinning factor
    bool dontExtrapolateCounts{false}; // In gibbs sampling, use direct counts from re-allocation in eq classes, don't extrapolate from txp-fraction
    bool initUniform{false}; // initialize offline optimization parameters uniformly, rather than with online estimates.
    bool alnMode{false};     // true if we're in alignment based mode, false otherwise
    bool biasCorrect{false}; // Perform sequence-specific bias correction
    bool gcBiasCorrect{false}; // Perform gc-fragment bias correction
    bool posBiasCorrect{false}; // Perform positional bias correction
    size_t numConditionalGCBins{3};
    size_t numFragGCBins{20};
    std::atomic<int32_t> numBiasSamples{1000000}; // The number of fragment mappings to consider when building
						  // the sequence-specific "foreground" distribution.

    // Related to the prior of the VBEM algorithm
    double vbPrior{1e-3};
    bool perTranscriptPrior{false};
    // Related to the fragment length distribution
    size_t fragLenDistMax;
    size_t fragLenDistPriorMean;
    size_t fragLenDistPriorSD;

    // Related to the logger
    std::shared_ptr<spdlog::logger> jointLog{nullptr};
    std::shared_ptr<spdlog::logger> fileLog{nullptr};

    // Related to caching and threading
    uint32_t mappingCacheMemoryLimit;
    uint32_t numThreads;
    uint32_t numQuantThreads;
    uint32_t numParseThreads;
};

#endif // SALMON_OPTS_HPP
