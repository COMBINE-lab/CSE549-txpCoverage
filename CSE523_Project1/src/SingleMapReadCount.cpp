#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <ctime>

using namespace std;

void createTxpMap(ifstream *inputFile, unordered_map<string, int> *txp_name_map, unordered_map<int, int> *txp_len_map) {

        int index = -1, txp_len;
        string line, txp_name;

        while(getline(*inputFile, line)) {
                if(index++ == -1)
                        continue;

                istringstream iss(line);
                iss >> txp_name >> txp_len;
                (*txp_name_map)[txp_name] = index;
                (*txp_len_map)[index] = txp_len;
        }
}

int main() {

	ifstream  infile;
	
	// Creating maps for transcripts name and length
	infile.open("data/txp_lens.tsv");
	if(!infile.is_open()) {
		cout << "Could not open input file" << endl;
		return -1;
	}
	unordered_map<string, int> txp_name_map;
        unordered_map<int, int> txp_len_map;
	createTxpMap(&infile, &txp_name_map, &txp_len_map);
	infile.close();

	//Open pos.csv file for read
	infile.open("data/pos.csv");
	if(!infile.is_open()) {
                cout << "Could not open input file" << endl;
                return -1;
        }
	
	// create arrays for keeping count
	int **txp_count_arr = new int*[txp_len_map.size()]();
	for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
		txp_count_arr[it->first] = new int[it->second]();
	}

	cout << "Starting timer" << endl;
	clock_t start_time = clock();

	// Read each alternate line of pos.csv
	int  line_count = -1;
	string line;
	while(getline(infile, line)) {
		line_count++;
		if(!(line_count % 2)) {
			continue;
		}

		istringstream iss(line);
		int comma_pos;
		char *name = &line[0];
		for(comma_pos = 0; line[comma_pos] != ',' && line[comma_pos] != '\0'; comma_pos++) {
			;
		}
		// If wrong line return immediately
		if(comma_pos == line.size() - 1) {
			cout << "Comma not found; wrong line parsed!" << endl;
			return -1;
		}
		line[comma_pos] = '\0';
		char *temp = &line[comma_pos + 1];
		int pos = stoi(temp, nullptr, 10);
		txp_count_arr[txp_name_map[name]][pos] += 1;
	}
	infile.close();
	
	cout << "Total read time :" << float(clock()-start_time)/CLOCKS_PER_SEC << endl;
	start_time = clock();

	//write result to file	
	ofstream outfile;
	outfile.open("output/SingleMapReadCount.txt", fstream::trunc);
	if(!outfile.is_open()) {
		cout << "Could not open/create output file" << endl;
		return -1;
	}
	
	for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
		for(int i = 0; i < it->second; i++) {
                	outfile << txp_count_arr[it->first][i] << ' ';
		}
		outfile << endl;
	}
	outfile.close();
	
	// delete arrays
        for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
                delete [] txp_count_arr[it->first];
        }
	delete [] txp_count_arr;

	// For the complete data set, writing to the file takes about 25 seconds and a file of ~500 MB gets created
	cout << "Total write time :" << float(clock()-start_time)/CLOCKS_PER_SEC << endl;

    return 0;
}
