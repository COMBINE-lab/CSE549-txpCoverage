CC=g++
CFLAGS=-std=c++11

.PHONY: all clean test

all: single_map multi_map

single_map:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/SingleMapReadCount.cpp -o bin/SingleMapReadCount
	./bin/SingleMapReadCount

multi_map:
	mkdir -p bin output
	$(CC) $(CFLAGS) src/MultiMapReadCount.cpp -o bin/MultiMapReadCount
	./bin/MultiMapReadCount

clean:
	rm -rf bin/ output/

test:
	mkdir -p bin
	$(CC) $(CFLAGS) test/InputCreator.cpp -o bin/TestInput
	./bin/TestInput
