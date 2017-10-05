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
    outFile.open("input.txt");

    int t[5];
    t[0] = rand() % MAX + MAX;
    t[1] = rand() % MAX + MAX;
    t[2] = rand() % MAX + MAX;
    t[3] = rand() % MAX + MAX;
    t[4] = rand() % MAX + MAX;

    int linesCount = rand() % MAX;

    for(int i = 0; i < linesCount; i++) {
	int tIndex = rand() % 5;
	outFile << rand() % t[tIndex] << " " << t[tIndex] << " " << tIndex << endl;
    }
    outFile.close();

    return 0;
}
