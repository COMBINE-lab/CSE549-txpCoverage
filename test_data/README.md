# Details for the files in tests.tar.gz

* pos.csv: Read -> pos/transcript mapping with the following schema  
<READ_ID>\n  
<TXP_ID>,\<POSITION\>\n  

* txp_lens.tsv: transcript lengths  
<TXP_NAME>\t\<LENGTH\>\n  

* abundance.tsv: abundance of the transcript in TPM (per million)  
<TXP_NAME>\t\<ABUNDANCE\>\n  

# How to Run Salmon

## 1. Download data

Download reference transcriptome
```
wget ftp://ftp.sanger.ac.uk/pub/gencode/Gencode_human/release_27/gencode.v27.transcripts.fa.gz
```

Download paired end reads
```

```

## 2. Index the reference

Next step is to index the reference using the salmon binary and the above downloaded fasta file

```
<salmon_bin_path> index -t <fa_file_path> -k 31 -i <output_index_path> --gencode -p <number_threads>
```

## 3. Run Salmon quantification

the last step is to run salmon quantification 

```
<salmon_bin_path> quant -i <input_index_path> -1 <first_read_file> -2 <second_read_file> -o <salmon_output_folder> -p <num_threads> -la
```
