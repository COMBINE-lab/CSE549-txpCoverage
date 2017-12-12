library(ggplot2)

args <- commandArgs(trailingOnly=TRUE)
inputFile <- c()
outputFile <- c()
if(length(args) == 0) {
	inputFile <- "../CSE523_Project1/output/txpReadCount.tsv"
} else {
	inputFile <- args[1]
}
conn <- file(inputFile, open="r")
line <-readLines(conn)
s_list <- strsplit(line, "\t")
c_vec <- s_list[[1]]
c_vec2 <- c_vec[-1]
i_vec <- as.numeric(c_vec2)

if(length(args) < 2) {
	outputFile <- paste(s_list[[1]], ".jpg", sep="")
} else {
	outputFile <- args[2]
}

png(file = outputFile)
plot(i_vec, type = "l", col = "blue", xlab = "Position", ylab = "Read Count", main = "Trancsript Read Count")
dev.off()
#df <- data.frame(i_vec)
#g<-ggplot(df)
#g

close(conn)
