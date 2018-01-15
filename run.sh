#!/bin/bash

if [ "$2" ]; then
	cd salmon
	if [ "$2" = "full" ]; then
		./bin/salmon quant -i output/intermediate -1 input/ERR188023_1.fastq -2 input/ERR188023_2.fastq -o output/ -p 4 -la --dumpAlignments > output/pos.tsv
	else
		./bin/salmon quant -i output/intermediate -1 input/1.fastq -2 input/2.fastq -o output/ -p 4 -la --dumpAlignments > output/pos3.tsv
	fi
	cd ..
fi
cd CSE523_Project1
make
if [ -z "$1" -o "$1" = "-" ]; then
	./bin/txp_rc ../salmon/output/quant.sf ../salmon/output/pos3.tsv output/txpReadCount.tsv
else
	echo "Txp ID: " "$1"
	./bin/txp_rc ../salmon/output/quant.sf ../salmon/output/pos3.tsv output/txpReadCount.tsv "$1"
	cd ..
	imgName="txp_plot/$1.png"
	Rscript txp_plot/plot.R CSE523_Project1/output/txpReadCount.tsv "$imgName"
fi
