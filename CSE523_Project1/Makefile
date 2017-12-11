CC=g++
CFLAGS=-std=c++11 -g

.PHONY: all clean test

all: clean txp_rc

single_map:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/SingleMapReadCount.cpp -o bin/singlemapreadcount

multi_map:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/MultiMapReadCount.cpp -o bin/multimapreadcount

txp_rc:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/TxpReadCount.cpp -o bin/txp_rc

clean:
	rm -rf bin/ output/

test:
	mkdir -p bin
	$(CC) $(CFLAGS) test/InputCreator.cpp -o bin/TestInput
