library(ggplot2)

fileName <- "CSE523_Project1/output/txpReadCount.tsv"
conn <- file(fileName,open="r")
line <-readLines(conn)
s_list <- strsplit(line, "\t")
c_vec <- s_list[[1]]
c_vec2 <- c_vec[-1]
i_vec <- as.numeric(c_vec2)

png(file = "txp_plot.jpg")
plot(i_vec, type = "l", col = "blue", xlab = "Position", ylab = "Read Count", main = "Trancsript Read Count")
dev.off()
#df <- data.frame(i_vec)
#g<-ggplot(df)
#g

close(conn)
