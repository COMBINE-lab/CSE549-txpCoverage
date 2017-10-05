#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <map>

using namespace std;

void add(unordered_map<int, int> *read_map, int index) {
	
	if((*read_map).find(index) == (*read_map).end()) {
		(*read_map)[index] = 1;
	} else {
		(*read_map).at(index) += 1;
	}
}

int main() {

	ifstream  infile;	
	infile.open("data/input.txt");
	if(!infile.is_open()) {
		cout << "Could not open input file" << endl;
		return -1;
	}
	string line;
	
	ofstream outfile;
	outfile.open("output/output.txt", fstream::trunc);
	if(!outfile.is_open()) {
		cout << "Could not open output file" << endl;
		return -1;
	}
	
	unordered_map<int, int> *tr_map = new unordered_map<int, int>[6];
	int *tr_len = new int[6];
	
	while(getline(infile, line)) {
		istringstream iss(line);
		int index, length, tnum;
		iss >> index >> length >> tnum;

		tr_len[tnum] = length;

		add(&tr_map[tnum], index);
	}
	int i = 0;
	int count = 0;
	while(tr_len[i] > 0) {
		unordered_map<int, int> read_map = tr_map[i];
		for(int j = 0; j < tr_len[i]; j++) {
			int output = 0;
			if(read_map.find(j) == read_map.end()) {
				//do nothing
			} else {
				count++;
				output = read_map.at(j);
			}
			outfile << output << " ";
		}
		outfile << endl;
		i++;
	}
	outfile << "count = " << count << endl;
	outfile.close();
	infile.close();
    return 0;
}
