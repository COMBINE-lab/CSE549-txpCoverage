
#index
../alevin/bin/salmon/bin/salmon index -t ../alevin/data/human/txptome/rsem.txp.fa -k 31 -i ./index/ -p 20

#quant
../alevin/bin/salmon/bin/salmon quant -la -r ../alevin/data/10x/human/4k/pbmc4k_S1_L001_R2_001.fastq.gz -i ./index/ -o ./out/ -p 20 -z > out.sam

#lens
samtools view -H out.sam > test_data/txp_lens.tsv

#pos
bioawk -c sam '{print $qname "\n" $rname","$pos}' out.sam > test_data/pos.csv

#abundance
awk '{print $1,$4}' quant.sf > abundance.tsv
