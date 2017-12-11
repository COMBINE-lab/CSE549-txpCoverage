from collections import Counter
import numpy as np
import pandas as pd
import click
import sys
import pysam
from collections import defaultdict

def divideRead(quantDf, algns, qTxpId):
    abundSum = 0.0
    qAbund = 0.0
    for tid in algns:
        abund = quantDf[tid]
        if tid == qTxpId:
            qAbund += abund
        abundSum += abund
    if abundSum == 0:
        return 0.0
    return qAbund/abundSum

@click.command()
@click.option('--sam', help="input sam file")
@click.option('--sf', help="input quant.sf file from salmon")
@click.option('--out', help="output file location")
@click.option('--tid', help="txp Id to generate coverage of")
def main(sam, sf, out, tid):
    quantDf = pd.read_table(sf)
    coverage = defaultdict(float)
    tlen = quantDf[ quantDf['Name'] == tid ]['Length'].values[0]
    quantDf = quantDf.set_index('Name').to_dict()['NumReads']

    with pysam.AlignmentFile(sam) as samFile:
        read = ''
        alns = []
        found = False
        num = 0
        for aln in samFile:
            qname = aln.query_name
            rname = aln.reference_name
            rstart = aln.reference_start
            if read != qname:
                num += 1
                if num%100000 == 0:
                    print "\r Done " + str(num) + " reads.",
                    sys.stdout.flush()

                # finalize previous history
                if found:
                    count = divideRead(quantDf,
                                       alns,
                                       tid)
                    if count != 0:
                        coverage[pos] += count
                found = False

                # add new elements
                read = qname
                alns = []
                pos = -1
                
            if rname == tid:
                pos = rstart
                found = True
            alns.append(rname)

    if -1 in coverage:
            print "error"
            exit(1)

    with open(out, 'w') as wf:
        wf.write(tid)
        for i in range(tlen):
            if i in coverage:
                wf.write("\t"+str(coverage[i]))
            else:
                wf.write("\t0.0")


if __name__ == "__main__":
    main()
