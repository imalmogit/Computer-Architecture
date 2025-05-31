#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdint.h>
#include <cmath>

using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;
using std::vector;
using namespace std;



class Block {
	public:
		uint32_t tag;
		uint32_t set;
		uint32_t address;
		int lru;
		bool dirty;
		bool valid; // 1 for valid, 0 for invalid

		Block (uint32_t tag_i = 0, uint32_t set_i = 0,
				uint32_t address_i = 0, int lru_i = 0, bool dirty_i = false,
				bool valid_i = false) {
					this->tag = tag_i;
					this->set = set_i;
					this->address = address_i;
					this->lru = lru_i;
					this->dirty = dirty_i;
					this->valid = valid_i;
			
		}
		~Block() {
			// Destructor
			// No dynamic memory allocation, so nothing to free
		}
};

class Way {
	public:
		vector<Block> blocks;
		uint32_t setSize; // number of bits for set
		//uint32_t offsetSize; // number of bits for offset
		//int tagSize; // number of bits for tag
		int wayNum; 

		Way(uint32_t setSize_i = 0, /*uint32_t offsetSize_i = 0, int tagSize_i = 0,*/
				int wayNum_i = 0) {
					this->setSize = setSize_i;
					//this->offsetSize = offsetSize_i;
					//this->tagSize = tagSize_i;
					this->wayNum = wayNum_i;
					for (int i = 0; i < (1 << this->setSize); i++) { // insert block for each set
						this->blocks.push_back(Block());
					}
		}
		~Way() {

		}
	

};

class CacheLevel {
	public:
		uint32_t cacheSize; // power of 2
		uint32_t cacheAssoc; // power of 2
		uint32_t cacheCyc;
		uint32_t setSize;
		uint32_t blockSize; // power of 2
		uint32_t hit;
		uint32_t miss;
		vector<Way> ways;
		bool writeAlloc; // Write allocate.no allocate policy


		CacheLevel(uint32_t cacheSize_i, uint32_t cacheAssoc_i,
			uint32_t cacheCyc_i, uint32_t blockSize_i,
			bool writeAlloc_i, uint32_t hit_i, uint32_t miss_i) {

			this->cacheSize = cacheSize_i;
			this->cacheAssoc = cacheAssoc_i;
			this->cacheCyc = cacheCyc_i;
			this->blockSize = blockSize_i;
			this->writeAlloc = writeAlloc_i;
			this->hit = hit_i;
			this->miss = miss_i;

					
			uint32_t cacheSizeBytes = (1 << cacheSize);
			uint32_t blockSizeBytes = (1 << blockSize);
			uint32_t associativity = (1 << cacheAssoc);
			uint32_t numSets = cacheSizeBytes / (blockSizeBytes * associativity);

			int localSetSize = log2(numSets);
			this->setSize = localSetSize;

			for (int i = 0; i < associativity; i++) {
   				 Way w(this->setSize, i);
   				 ways.push_back(w);
			}

		}

		~CacheLevel() {
	
		}
		
		// function to get set
		uint32_t getSet(uint32_t address) {
			return (address >> this->blockSize) & ((1 << this->setSize) - 1);
		}

		// function to get tag
		uint32_t getTag(uint32_t address) {
			return address >> (this->setSize + this->blockSize);
		}

		// function to manage the LRU policy in the cache level
		void updateLRU(int set, int wayNum) {
    		int currentLRU = this->ways.at(wayNum).blocks.at(set).lru;
			int numWays = (1 << this->cacheAssoc);
			this->ways.at(wayNum).blocks.at(set).lru = numWays - 1;

			for (int i = 0; i < (1 << this->cacheAssoc); i++) {
				if (i != wayNum && this->ways.at(i).blocks.at(set).lru > currentLRU
					&& this->ways.at(i).blocks.at(set).valid) {
					this->ways.at(i).blocks.at(set).lru--;
				}
			}

		}

		// function to check HIT/MISS in the cache level
		bool checkHit(uint32_t address, char op) {
			uint32_t tag = this->getTag(address);
			uint32_t set = this->getSet(address);
			
			for (int i = 0; i < (1 << this->cacheAssoc); i++) {
				Block &block = this->ways.at(i).blocks.at(set);
				if (block.valid && block.tag == tag) {
					updateLRU(set, i);
					if (op == 'w') {
						block.dirty = true;
					}
					return true;
				}
			}
			return false;
		}

		// function to evict the block in the cache level 
		void evict(uint32_t address, char op, CacheLevel &nextLevel, int nextLevelNum) {

			uint32_t tag = this->getTag(address);
			uint32_t set = this->getSet(address);
			int wayNum = evictBlock(set);
			
			Block &block = this->ways.at(wayNum).blocks.at(set);
			if (block.valid && block.dirty && nextLevelNum == 2) {
				nextLevel.dirtyBit(block.address);
			}

			block.tag = tag;
			block.valid = true;
			block.address = address;
			block.dirty = (op == 'w' && nextLevelNum);

			this->updateLRU(set, wayNum);
			return;	
		}

		// function to snoop the block in the previous cache level
		void snoop(uint32_t address, char op, CacheLevel &prevLevel, int prevLevelNum) {
	
			uint32_t tag = this->getTag(address);
			uint32_t set = this->getSet(address);
			int wayNum = this->evictBlock(set);
			
			Block &block = this->ways.at(wayNum).blocks.at(set);
			if (block.valid && prevLevelNum == 1) {
				prevLevel.invalidate(block.address);
			}
			
			block.tag = tag;
			block.valid = true;
			block.address = address;
			block.dirty = false;
			this->updateLRU(set, wayNum);
			return;
		}
			
		// function to check if the wanted set is empty, returns way's number to be evicted
		int evictBlock(uint32_t set) {
			for (int i = 0; i < (1 << this->cacheAssoc); i++) {
				if ((!this->ways.at(i).blocks.at(set).valid) 
					|| (this->ways.at(i).blocks.at(set).lru == 0)) {
					return i;
				}
			}
			return 0;
		}

		// function to update durty bit when writing to block
		void dirtyBit(uint32_t address) {

			uint32_t tag = this->getTag(address);
			uint32_t set = this->getSet(address);
			for (int i = 0; i < (1 << this->cacheAssoc); i++) {
				Block &block = this->ways.at(i).blocks.at(set);
				if (block.valid && block.tag == tag) {
					block.dirty = true;
					updateLRU(set, i);
				}
			}
		}

		// function to invalidate the block in the cache level
		void invalidate(uint32_t address) {
	
			uint32_t tag = this->getTag(address);
			uint32_t set = this->getSet(address);
			for (int i = 0; i < (1 << this->cacheAssoc); i++) {
				Block &block = this->ways.at(i).blocks.at(set);
				if (block.valid && block.tag == tag) {
					block.valid = false;
					//this->ways.at(i).blocks.at(set).dirty = false;
				}
			}
		}

		// function to get the miss rate
		double getMissRate() {
			if (this->hit + this->miss == 0) {
				return 0;
			}
			return (double)this->miss / (double)(this->hit + this->miss);
		}


};

class Cache {
	public:
		CacheLevel L1;
		CacheLevel L2;
		uint32_t memCyc;
		uint32_t L1cyc;
		uint32_t L2cyc;
		uint32_t totTime;
		uint32_t totReq;
		bool writeAlloc;
		

		Cache(uint32_t memCyc_i, uint32_t L1cyc_i, uint32_t L2cyc_i,
          uint32_t L1Size_i, uint32_t L1Assoc_i,
          uint32_t L2Size_i, uint32_t L2Assoc_i,
          uint32_t blockSize_i, bool writeAlloc_i)
        : L1(L1Size_i, L1Assoc_i, L1cyc_i, blockSize_i, writeAlloc_i, 0, 0),
          L2(L2Size_i, L2Assoc_i, L2cyc_i, blockSize_i, writeAlloc_i, 0, 0){
			this->memCyc = memCyc_i;
			this->L1cyc = L1cyc_i;
			this->L2cyc = L2cyc_i;
			this->totTime = 0;
			this->totReq = 0;
			this->writeAlloc = writeAlloc_i;
			this->L1 = CacheLevel(L1Size_i, L1Assoc_i, L1cyc_i, blockSize_i, writeAlloc_i, 0, 0);
			this->L2 = CacheLevel(L2Size_i, L2Assoc_i, L2cyc_i, blockSize_i, writeAlloc_i, 0, 0);
			
		}

		~Cache() {
			
		}
		
		
		// function to manage the request to the cache
		void access(uint32_t address, char op) {
			this->totReq++;
			if (L1.checkHit(address, op)) {
				// hit in L1
				this->L1.hit++;
				this->totTime += this->L1cyc;
			} else if (L2.checkHit(address, op)) {
					// miss in L1, hit in L2
					this->L1.miss++;
					this->L2.hit++;
					this->totTime += (this->L1cyc + this->L2cyc);
					if (((op == 'w') && this->writeAlloc) || (op == 'r')) {
						this->L1.evict(address, op, this->L2, 2);
					} 
			} else {
					// miss in both L1 and L2
					this->L1.miss++;
					this->L2.miss++;
					this->totTime += this->L1cyc + this->L2cyc + this->memCyc;
					if (((op == 'w') && this->writeAlloc) || (op == 'r')){
						this->L2.snoop(address, op, this->L1, 1);
						this->L1.evict(address, op, this->L2, 2);
					}
				
			}

		}
		



		// function to get the average access time
		double getAvgAccTime() {
			if (this->totReq == 0) {
				return 0;
			}
			return (double) this->totTime / (double)this->totReq;
		}

};

int main(int argc, char **argv) {

	if (argc < 19) {
		cerr << "Not enough arguments" << endl;
		return 0;
	}

	// Get input arguments

	// File
	// Assuming it is the first argument
	char* fileString = argv[1];
	ifstream file(fileString); //input file stream
	string line;

	if (!file || !file.good()) {
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}

	unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

	for (int i = 2; i < 19; i += 2) {
		string s(argv[i]);
		if (s == "--mem-cyc") {
			MemCyc = atoi(argv[i + 1]);
		} else if (s == "--bsize") {
			BSize = atoi(argv[i + 1]);
		} else if (s == "--l1-size") {
			L1Size = atoi(argv[i + 1]);
		} else if (s == "--l2-size") {
			L2Size = atoi(argv[i + 1]);
		} else if (s == "--l1-cyc") {
			L1Cyc = atoi(argv[i + 1]);
		} else if (s == "--l2-cyc") {
			L2Cyc = atoi(argv[i + 1]);
		} else if (s == "--l1-assoc") {
			L1Assoc = atoi(argv[i + 1]);
		} else if (s == "--l2-assoc") {
			L2Assoc = atoi(argv[i + 1]);
		} else if (s == "--wr-alloc") {
			WrAlloc = atoi(argv[i + 1]);
		} else {
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}


	Cache cache(MemCyc, L1Cyc, L2Cyc, L1Size, L1Assoc, L2Size, L2Assoc,
			BSize, WrAlloc);

	while (getline(file, line)) {

		stringstream ss(line);
		string address;
		char operation = 0; // read (R) or write (W)
		if (!(ss >> operation >> address)) {
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}

		// DEBUG - remove this line
		//cout << "operation: " << operation;

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		// DEBUG - remove this line
		//cout << ", address (hex)" << cutAddress;

		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);

		cache.access(num, operation);

		// DEBUG - remove this line
		//cout << " (dec) " << num << endl;

	}

	file.close(); 
	double L1MissRate = (double)cache.L1.getMissRate();
	double L2MissRate = (double)cache.L2.getMissRate();
	double avgAccTime = (double)cache.getAvgAccTime();

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}
