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

void createTxpAbunMap(ifstream *inputFile, unordered_map<string, int> *txp_name_map, unordered_map<int, float> *txp_abun_map) {

        int index = -1;
	float txp_abun;
        string line, txp_name;

        while(getline(*inputFile, line)) {
                if(index++ == -1)
                        continue;

                istringstream iss(line);
                iss >> txp_name >> txp_abun;
                (*txp_abun_map)[(*txp_name_map)[txp_name]] = txp_abun;
        }
}

void add_positions(string line, unordered_map<int, int> *read_pos_map, unordered_map<string, int> *txp_name_map) {
	istringstream iss(line);
	int comma_pos;
	char *name = &line[0];
	for(comma_pos = 0; line[comma_pos] != ',' && line[comma_pos] != '\0'; comma_pos++) {
		;
	}
	// If wrong line return immediately
	if(comma_pos == line.size() - 1) {
	cout << "Comma not found; wrong line parsed!" << endl;
	return;
	}
	line[comma_pos] = '\0';
	char *temp = &line[comma_pos + 1];
	int pos = stoi(temp, nullptr, 10);

	int txp_rank = (*txp_name_map)[name];
	(*read_pos_map)[txp_rank] = pos;
}

void setReadCount(unordered_map<int, int> *read_pos_map, unordered_map<int, float> *txp_abun_map, float ***txp_count_arr) {

	float total = 0;
	for(auto it = (*read_pos_map).begin(); it != (*read_pos_map).end(); it++) {
		total += (*txp_abun_map)[it->first];
	}
	for(auto it = (*read_pos_map).begin(); it != (*read_pos_map).end(); it++) {
                (*txp_count_arr)[it->first][it->second] = (*txp_abun_map)[it->first]/total;
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

	// Creating maps for transcripts name and length
        infile.open("data/abundance.tsv");
        if(!infile.is_open()) {
                cout << "Could not open input file" << endl;
                return -1;
        }
	unordered_map<int, float> txp_abun_map;
	createTxpAbunMap(&infile, &txp_name_map, &txp_abun_map);
        infile.close();

	//Open pos.csv file for read
	infile.open("data/pos.csv");
	if(!infile.is_open()) {
                cout << "Could not open input file" << endl;
                return -1;
        }
	
	// create arrays for keeping count
	float **txp_count_arr = new float*[txp_len_map.size()]();
	for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
		txp_count_arr[it->first] = new float[it->second]();
	}

	cout << "Starting timer" << endl;
	clock_t start_time = clock();

	// Read pos.csv
	string read, read_prev, pos_line;
	unordered_map<int, int> read_pos_map;
	int read_count = 0;
	while(getline(infile, read) && getline(infile, pos_line)) {
		if(!read_count) {
			read_prev = read;
		}
		if(read.compare(read_prev)) {
                        setReadCount(&read_pos_map, &txp_abun_map, &txp_count_arr);
                        read_count = 0;
                        read_pos_map.clear();
		}
		add_positions(pos_line, &read_pos_map, &txp_name_map);
		read_count++;
		read_prev = read;
	}
	infile.close();
	
	cout << "Total read time :" << float(clock()-start_time)/CLOCKS_PER_SEC << endl;
	start_time = clock();

	//write result to file	
	ofstream outfile;
	outfile.open("output/MultiMapReadCount.txt", fstream::trunc);
	if(!outfile.is_open()) {
		cout << "Could not open/create output file" << endl;
		return -1;
	}
	
	// Since the output is too big, here we are checking for only 1 trasncript
	// For the complete data set, writing to the file takes about 25 seconds and a file of ~500 MB gets created
	for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
		for(int i = 0; i < it->second; i++) {
                	outfile << txp_count_arr[it->first][i] << ' ';
		}
		outfile << endl;
	}
	outfile.close();

//	cout << "Abundance for ENST00000272317.10 : " << txp_abun_map[txp_name_map["ENST00000272317.10"]] << "\nvalue at 743 : " << txp_count_arr[txp_name_map["ENST00000272317.10"]][743] << endl;
//	cout << "Abundance for ENST00000495843.1 : " << txp_abun_map[txp_name_map["ENST00000495843.1"]] << "\nvalue at 1075 : " << txp_count_arr[txp_name_map["ENST00000495843.1"]][1075] << endl;
//	cout << "Abundance for ENST00000404735.1 : " << txp_abun_map[txp_name_map["ENST00000404735.1"]] << "\nvalue at 566 : " << txp_count_arr[txp_name_map["ENST00000404735.1"]][566] << endl;
	
	// delete arrays
        for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
                delete [] txp_count_arr[it->first];
        }
	delete [] txp_count_arr;

	cout << "Total write time :" << float(clock()-start_time)/CLOCKS_PER_SEC << endl;

    return 0;
}
