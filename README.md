# Execute All
 Execute run.sh to run all binaries together.
run.sh takes 2 arguments:
1. Argument 1 is either "-" or trancsript ID can be provided through it. 
   By defualt, result for all transcripts will we produced.
2. Argument 2 is for running salmon. If its value is "full", salmon will run for the complete dataset.
   If argument 2 is any other string, salmon will run for a smaller dataset.
   If argument 2 is not present, salmon will not run.

So, to execute salmon partially, assuming 1.fastq and 2.fastq are smaller datasets, following command will run salmon for smaller dataset for a given "txp-id":

./run.sh "txp-id" partial

To run salmon for complete dataset and create read count for all transcripts, use:

./run.sh - full

To create read count for "txp-id" without running salmon, execute:

./run.sh "txp-id"

