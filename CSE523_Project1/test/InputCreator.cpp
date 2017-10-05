//
// Created by akshat on 9/28/2017.
//
#include <iostream>
#include <fstream>
#include <cstdlib>

#define MAX 10000

using namespace std;

int main() {
    
    ofstream outFile;
    outFile.open("data/input.txt", fstream::trunc);

    int t[5];
    t[0] = rand() % MAX;
    t[1] = rand() % MAX + MAX/2;
    t[2] = rand() % MAX;
    t[3] = rand() % MAX;
    t[4] = rand() % MAX + MAX/2;

    int linesCount = 10000000;

    for(int i = 0; i < linesCount; i++) {
	int tIndex = rand() % 5;
	outFile << rand() % t[tIndex] << " " << t[tIndex] << " " << tIndex << endl;
    }
    outFile.close();

    return 0;
}
