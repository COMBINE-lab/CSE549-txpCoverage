CC=g++
CFLAGS=-std=c++11

.PHONY: all clean test

all:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/SingleMapReadCount.cpp -o bin/SingleMapReadCount
	./bin/SingleMapReadCount

clean:
	rm -rf bin/ output/

test:
	mkdir -p bin
	$(CC) $(CFLAGS) test/InputCreator.cpp -o bin/TestInput
	./bin/TestInput
