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
	outputFile <- paste(c_vec[1], ".png", sep="")
} else {
	outputFile <- args[2]
}

png(file = outputFile, width=1000, height=600, unit="px")

plot(i_vec, type = "l", col = "blue", lty = 1, axes=FALSE, ann=FALSE)

xLabCount = as.integer(length(i_vec)/20)
yLabCount = as.integer(max(i_vec)/8)
axis(1, at = xLabCount * 0:length(i_vec))
axis(2, las = 1, at = yLabCount * 0:max(i_vec))
title(main="Transcript Read Coverage", font.main=4)
title(xlab="Locus")
title(ylab="Coverage")
legend("topright", c(c_vec[1]), cex = 1.0, col = c("blue"), lty = 1)
box(bty="L")

dev.off()
#df <- data.frame(i_vec)
#g<-ggplot(df)
#g

close(conn)
