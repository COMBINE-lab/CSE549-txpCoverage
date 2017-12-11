#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <ctime>

using namespace std;

void createTxpMaps(ifstream *inputFile, unordered_map<string, int> *txp_name_map, unordered_map<int, int> *txp_len_map, unordered_map<int, float> *txp_abun_map) {

        int index = 0, txp_len;
        string line, txp_name;
	float txp_abun, txp_eff_len, txp_num_reads;
	getline(*inputFile, line);
        while(getline(*inputFile, line) && !line.empty()) {
                istringstream iss(line);
                iss >> txp_name >> txp_len >> txp_eff_len >> txp_abun >> txp_num_reads;
                (*txp_name_map)[txp_name] = index;
                (*txp_len_map)[index] = txp_len;
		(*txp_abun_map)[index] = txp_abun;
		index++;
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
	string txp_id(name);
	char *temp = &line[comma_pos + 1];
	string t_pos(temp);
	int pos = stoi(t_pos);

	int txp_index = (*txp_name_map)[txp_id];
	(*read_pos_map)[txp_index] = pos;
}

void setReadCount(unordered_map<int, int> *read_pos_map, unordered_map<int, float> *txp_abun_map, float ***txp_count_arr) {

	float total = 0;
	for(auto it = (*read_pos_map).begin(); it != (*read_pos_map).end(); it++) {
		total += (*txp_abun_map)[it->first];
	}
	for(auto it = (*read_pos_map).begin(); it != (*read_pos_map).end(); it++) {
                (*txp_count_arr)[it->first][it->second] += (*txp_abun_map)[it->first]/total;
        }
}

int main(int argc, char* argv[]) {

	// Creating maps for transcripts name and length
	string quantFile = "input/quant.sf";
	if(argc > 1 && argv[1] != NULL) {
		string temp(argv[1]);
		quantFile = temp;
		cout << "Quant File: " << quantFile << endl;
	}
	ifstream  infile;
	infile.open(quantFile);
	if(!infile.is_open()) {
		cout << "Could not open input file: " << quantFile << endl;
		return -1;
	}
	unordered_map<string, int> txp_name_map;
        unordered_map<int, int> txp_len_map;
	unordered_map<int, float> txp_abun_map;
	createTxpMaps(&infile, &txp_name_map, &txp_len_map, &txp_abun_map);
        infile.close();

	//Open pos.csv file for read
	string posFile = "input/pos.csv";
	if(argc > 2 && argv[2] != NULL) {
		string temp(argv[2]);
                posFile = temp;
		cout << "pos file: " << posFile << endl;
        }
	infile.open(posFile);
	if(!infile.is_open()) {
                cout << "Could not open input file: " << posFile << endl;
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
	
	cout << "Total read time :" << float(clock()-start_time)/CLOCKS_PER_SEC << " sec"<< endl;
	start_time = clock();

	//write result to file	
	ofstream outfile;
	string outputFile = "output/txpReadCount.tsv";
	if(argc > 3 && argv[3] != NULL) {
		string temp(argv[3]);
		outputFile = temp;
		cout << "Output file: " << outputFile << endl;
	}
	outfile.open(outputFile, fstream::trunc);
	if(!outfile.is_open()) {
		cout << "Could not open/create output file" << endl;
		return -1;
	}
	
	// Since the output is too big, here we are checking for only 1 trasncript
	// For ~200k tsanscripts, writing to the file takes about 120 seconds and a file of ~5 GB gets created
	int count = 0;
	string txpId = "";
	if(argc > 4 && argv[4] != NULL) {
		string temp(argv[4]);
		txpId = temp;
		cout<< "txp ID: " << txpId << endl;
	}
	for(auto it = txp_name_map.begin(); it != txp_name_map.end(); it++) {
		if(!txpId.empty()) {
			if(txpId.compare(it->first) == 0) {
				outfile << it->first <<'\t';
				//cout << "Txp_len: " << txp_len_map[it->second] << endl;
				for(int i = 0; i < txp_len_map[it->second]; i++) {
					outfile << txp_count_arr[it->second][i] << '\t';
                		}
				count++;
				break;
			}
		} else {
			count++;
			outfile << it->first <<'\t';
			for(int i = 0; i < txp_len_map[it->second]; i++) {
                		outfile << txp_count_arr[it->second][i] << '\t';
			}
			if(count % 10000 == 0)
				cout << count << " transcripts written" << endl;
			outfile << endl;
		}
	}
	cout << "Total transcript count: " << count << endl;
	outfile.close();

//	cout << "Abundance for ENST00000272317.10 : " << txp_abun_map[txp_name_map["ENST00000272317.10"]] << "\nvalue at 743 : " << txp_count_arr[txp_name_map["ENST00000272317.10"]][743] << endl;
//	cout << "Abundance for ENST00000495843.1 : " << txp_abun_map[txp_name_map["ENST00000495843.1"]] << "\nvalue at 1075 : " << txp_count_arr[txp_name_map["ENST00000495843.1"]][1075] << endl;
//	cout << "Abundance for ENST00000404735.1 : " << txp_abun_map[txp_name_map["ENST00000404735.1"]] << "\nvalue at 566 : " << txp_count_arr[txp_name_map["ENST00000404735.1"]][566] << endl;
	

	// delete arrays
	cout << "Deleting 'new'ly created arrays" << endl;
        for(auto it = txp_len_map.begin(); it != txp_len_map.end(); it++) {
                delete [] txp_count_arr[it->first];
        }
	delete [] txp_count_arr;


	cout << "Total write time :" << float(clock()-start_time)/CLOCKS_PER_SEC << " sec" << endl;

    return 0;
}
