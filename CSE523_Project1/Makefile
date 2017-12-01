CC=g++
CFLAGS=-std=c++11 -g

.PHONY: all clean test

all: clean multi_map

single_map:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/SingleMapReadCount.cpp -o bin/SingleMapReadCount

multi_map:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/MultiMapReadCount.cpp -o bin/MultiMapReadCount

clean:
	rm -rf bin/ output/

test:
	mkdir -p bin
	$(CC) $(CFLAGS) test/InputCreator.cpp -o bin/TestInput
