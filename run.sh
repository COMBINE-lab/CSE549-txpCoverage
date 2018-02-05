#!/bin/bash

if [ -z "$1" -o "$1" = "-" ]; then
	exit 1
fi

echo "Gene ID: " "$1"

if [ "$2" ]; then
	cd salmon
	if [ "$2" = "full" ]; then
		./bin/salmon quant -i output/intermediate -1 input/ERR188023_1.fastq -2 input/ERR188023_2.fastq -o output/ -p 4 -la --dumpAlignments > output/pos.tsv
	else
		./bin/salmon quant -i output/intermediate -1 input/1.fastq -2 input/2.fastq -o output/ -p 4 -la --dumpAlignments > output/pos.tsv
	fi
	cd ..
fi

cd CSE523_Project1
make
./bin/txp_rc input/gene2txp.tsv ../salmon/output/quant.sf ../salmon/output/pos.tsv output/txpReadCount.tsv "$1"
exit 0
