#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <ctime>

using namespace std;

void createTxpMaps(ifstream *inputFile, unordered_map<string, int> *txp_index_map, unordered_map<string, int> *txp_len_map, unordered_map<string, float> *txp_abun_map) {

        int txp_len;
	int index = 0;
        string line, txp_name;
	float txp_abun, txp_eff_len, txp_num_reads;
	getline(*inputFile, line);
        while(getline(*inputFile, line) && !line.empty()) {
                istringstream iss(line);
                iss >> txp_name >> txp_len >> txp_eff_len >> txp_abun >> txp_num_reads;
		(*txp_index_map)[txp_name] = index++;
                (*txp_len_map)[txp_name] = txp_len;
		(*txp_abun_map)[txp_name] = txp_num_reads;
        }
}

/*int add_positions(string line, unordered_map<string, int> *read_pos_map) {
	istringstream iss(line);
	string name = "";
	string t_pos = "";
	getline(iss, name, ',');
	getline(iss, t_pos);
	if(t_pos.empty() || name.empty())
		return -1;
	stringstream ss(t_pos);
	int pos = 0;
	ss >> pos;
	(*read_pos_map)[name] = pos;
	return 0;
}*/

void setReadCount(unordered_map<string, int> *read_pos_map, unordered_map<string, float> *txp_abun_map, unordered_map<string, int> *txp_index_map, float ***txp_count_arr) {

	float total = 0;
	for(auto it = (*read_pos_map).begin(); it != (*read_pos_map).end(); it++) {
		total += (*txp_abun_map)[it->first];
	}
	for(auto it = (*read_pos_map).begin(); it != (*read_pos_map).end(); it++) {
                (*txp_count_arr)[(*txp_index_map)[it->first]][it->second] += ((*txp_abun_map)[it->first])/total;
        }
}

int main(int argc, char* argv[]) {

	// Creating maps for transcripts name and length
	string quantFile = "input/quant.sf";
	if(argc > 1 && argv[1] != NULL) {
		string temp(argv[1]);
		quantFile = temp;
	}
	ifstream  infile;
	infile.open(quantFile);
	if(!infile.is_open()) {
		cerr << "Could not open input file: " << quantFile << endl;
		return -1;
	}
	unordered_map<string, int> txp_index_map;
        unordered_map<string, int> txp_len_map;
	unordered_map<string, float> txp_abun_map;
	createTxpMaps(&infile, &txp_index_map, &txp_len_map, &txp_abun_map);
        infile.close();

	// Open pos.csv file for read
	string posFile = "input/pos.csv";
	if(argc > 2 && argv[2] != NULL) {
		string temp(argv[2]);
                posFile = temp;
        }
	infile.open(posFile);
	if(!infile.is_open()) {
                cerr << "Could not open input file: " << posFile << endl;
                return -1;
        }
	
	// create arrays for keeping count
	float **txp_count_arr = new float*[txp_len_map.size()]();
	for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
		txp_count_arr[txp_index_map[it->first]] = new float[it->second]();
	}

	cerr << "Starting timer" << endl;
	clock_t start_time = clock();

	// Read pos.csv
	string read, read_prev, txp_id, line;
	int pos, matePos;
	unordered_map<string, int> read_pos_map;
	int read_count = 0;
	int line_count = 1;
	while(getline(infile, line) && !line.empty()) {
		istringstream iss(line);
		iss >> read >> txp_id >> pos >> matePos;
		if(read.empty()) {
			cerr << "Read is empty. Line: " << line << endl;
			continue;
		}
		if(txp_id.empty()) {
			cerr << "Txp_id is empty. Line: " << line << endl;
			continue;
		}
		if(pos > txp_len_map[txp_id]) {
			cerr << "wrong pos value. Line: " << line << endl;
			continue;
		}
		if(matePos < pos) {
			pos = matePos;
		}
		if(!read_count) {
			read_prev = read;
		}
		if(read.compare(read_prev)) {
                        setReadCount(&read_pos_map, &txp_abun_map, &txp_index_map, &txp_count_arr);
                        read_count = 0;
                        read_pos_map.clear();
			line_count++;
			if(line_count % 100000 == 0) {
				cerr << "\rReads processed: " << line_count;
			}
		}
		read_pos_map[txp_id] = pos;
		read_count++;
		read_prev = read;
	}
	cerr << "Total reads processed: " << line_count << endl;
	infile.close();
	
	cerr << "Total read time :" << float(clock()-start_time)/CLOCKS_PER_SEC << " sec"<< endl;
	start_time = clock();

	// write result to file	
	ofstream outfile;
	string outputFile = "output/txpReadCount.tsv";
	if(argc > 3 && argv[3] != NULL) {
		string temp(argv[3]);
		outputFile = temp;
	}
	outfile.open(outputFile, fstream::trunc);
	if(!outfile.is_open()) {
		cerr << "Could not open/create output file" << endl;
		return -1;
	}
	
	// Since the output is too big, here we are checking for only 1 trasncript
	// For ~200k tsanscripts, writing to the file takes about 120 seconds
	int count = 0;
	string txpId = "";
	if(argc > 4 && argv[4] != NULL) {
		string temp(argv[4]);
		txpId = temp;
	}
	for(auto it = txp_index_map.begin(); it != txp_index_map.end(); it++) {
		if(!txpId.empty()) {
			if(txpId.compare(it->first) == 0) {
				outfile << it->first <<'\t';
				for(int i = 1; i < txp_len_map[it->first]; i++) {
					outfile << txp_count_arr[it->second][i] << '\t';
                		}
				count++;
				break;
			}
		} else {
			count++;
			outfile << it->first <<'\t';
			for(int i = 0; i < txp_len_map[it->first]; i++) {
                		outfile << txp_count_arr[it->second][i];
				if(i < txp_len_map[it->first] -1) {
					outfile << '\t';
				}
			}
			if(count % 10000 == 0)
				cerr << count << " transcripts written" << '\r';
			outfile << endl;
		}
	}
	outfile << endl;
	cerr << "Total transcript count: " << count << endl;
	outfile.close();

	// delete arrays
	cerr << "Deleting 'new'ly created arrays" << endl;
	for(auto it = txp_index_map.begin(); it != txp_index_map.end(); it++) {
		delete [] txp_count_arr[it->second];
	}
	delete [] txp_count_arr;

	cerr << "Total write time :" << float(clock()-start_time)/CLOCKS_PER_SEC << " sec" << endl;

    return 0;
}
