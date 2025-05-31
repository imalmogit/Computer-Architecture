/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */


#include <iostream>
#include "bp_api.h"
#include <cmath>
#include <vector>
#include <cstdio>


#define NOT_USING_SHARE 0
#define USING_SHARE_LSB 1
#define USING_SHARE_MID 2

using namespace std;

// Global variables	
enum FSM_ST {
	SNT = 0,
	WNT = 1,
	WT = 2,
	ST = 3
};
unsigned btbSize;
unsigned historySize;
unsigned tagSize;
unsigned fsmState;
bool isGlobalHist;
bool isGlobalTable;
int Shared; 
vector<FSM_ST> globalFsm;
FSM_ST initFsmState; 
unsigned globalHistory = 0; 
SIM_stats simStats;


// functions to get the index from the PC
unsigned getIndex(uint32_t pc) {
	return (pc >> 2) & (btbSize - 1);
}

// function to get the tag from the PC
unsigned getTag(uint32_t pc) {
	return (pc >> (int)(log2(btbSize) + 2)) & ((1 << tagSize) - 1);
}

// each unit in the BTB table has these fields: tag (branch IP), history pattern, local_fsm, PC target address, and valid bit
class btbUnit{	

	public:
	unsigned tag; 
	unsigned history;
	vector<FSM_ST> localFsm; 
	uint32_t target; 
	bool isValid=false;

	// constructor for class btbUnit
	btbUnit(uint32_t pc = 255, uint32_t targetPc = 0, bool isValid = false) {
		tag = getTag(pc);
		target = targetPc;
		history = 0;
		this->isValid = isValid;
		if (!isGlobalTable){
			localFsm.resize(1 << historySize, initFsmState); // initialize local FSM
		}
	}

	// destructor for class btbUnit
	// clear the local FSM
	~btbUnit() {
		while(!localFsm.empty()) {
			localFsm.pop_back(); // clear the local FSM
		}
	}

};

vector<btbUnit> btbTable; // BTB table

int BP_init(unsigned btb_size, unsigned history_size, unsigned tag_size, unsigned fsm_state,
			bool is_globalHist, bool is_globalTable, int is_shared){
			//btbSize not power of 2 && not in range
			if ((btb_size & (btb_size - 1)) != 0 || btb_size < 0 || btb_size > 32) {
				fprintf(stderr, "Error in input file: btbSize must be a power of 2\n");
				return -1;
			}
			//historySize not in range
			if (history_size < 1 || history_size > 8) {
				fprintf(stderr, "Error in input file: historySize must be in range [1, 8]\n");
				return -1;
			}
			//tagSize not in range
			if (tag_size < 0 || tag_size > (30 - log2(btb_size))) {
				fprintf(stderr, "Error in input file: tagSize must be in range [0, 3-log2(btbSize)]\n");
				return -1;
			}
			//fsmState not in range	
			if (fsm_state < SNT || fsm_state > ST) {
				fprintf(stderr, "Error in input file: fsmState must be in range [0, 3]\n");
				return -1;
			}

			btbSize = btb_size;
			historySize = history_size;
			tagSize = tag_size;
			fsmState = fsm_state;
			isGlobalHist = is_globalHist;
			isGlobalTable = is_globalTable;
			Shared = is_shared;
			initFsmState = (FSM_ST)fsmState;
			globalHistory = 0; 
			btbTable.resize(btbSize, btbUnit()); // resize the BTB table to the given size & initialize it
			if(isGlobalTable){
				globalFsm.resize(1 << historySize, initFsmState); // initialize global FSM
			}

			simStats.flush_num = 0; // initialize the number of flushes
			simStats.br_num = 0; // initialize the number of branch instructions
			
			size_t entrySize = (1 << historySize) * 2; // size of each entry in the BTB table
			simStats.size = btbSize * (tagSize + 31); // 31 = 32(target address)-2(offset)+1(valid bit)  
			if (isGlobalTable) {
				simStats.size += entrySize; // add the size of the global FSM
			} else simStats.size += entrySize * btbSize; // add the size of the local FSM

			if (isGlobalHist) {
				simStats.size += historySize; // add the size of the global history
			} else simStats.size += historySize * btbSize; // add the size of the local history


	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
	unsigned index = getIndex(pc);
	unsigned tag = getTag(pc);

	unsigned fixedHis;
	int shareBits = 0;
	if (Shared == USING_SHARE_LSB) {
		shareBits = 2;
	} else if (Shared == USING_SHARE_MID) {
		shareBits = 16;
	}
	if ((btbTable[index].isValid) && (btbTable[index].tag == tag)) {
		if (isGlobalTable) { // global table
			if (isGlobalHist) { // global table & global history
				if (shareBits == 0) {
					fixedHis = globalHistory;
				} else fixedHis = globalHistory ^ ((pc & (((1 << historySize) - 1) << shareBits)) >> shareBits); // get the fixed history
			} else { // global table & local history
				if (shareBits == 0) {
					fixedHis = btbTable[index].history;
				} else fixedHis = btbTable[index].history ^ ((pc & (((1 << historySize) - 1) << shareBits)) >> shareBits); // get the fixed history
			}
			if (globalFsm[fixedHis] == ST || globalFsm[fixedHis] == WT) { // check the FSM state
				*dst = btbTable[index].target; // get the target address
				return true;
			} else {
				*dst = pc + 4; // not taken
				return false;
			}
		} else { // local table
			if (isGlobalHist) { // local table & global history
				fixedHis = globalHistory;
			} else fixedHis = btbTable[index].history; // local table & local history
			if(btbTable[index].localFsm[fixedHis] == ST || btbTable[index].localFsm[fixedHis] == WT) { // check the FSM state
				*dst = btbTable[index].target; // get the target address
				return true;
			} else {
				*dst = pc + 4; // not taken
				return false;
			}
		}
	} else { 
			*dst = pc + 4; // not taken
	}
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	simStats.br_num++; // update the number of branch instructions
	unsigned index = getIndex(pc);
	unsigned tag = getTag(pc);

	
	if ((taken && (targetPc != pred_dst)) || (!taken && (pc + 4 != pred_dst))) {
		simStats.flush_num++; 
		//printf("taken: %d, targetPc: %x, pred_dst: %x pc + 4 : %x \n", taken, targetPc, pred_dst, pc+4);
	}
	
	if (!btbTable[index].isValid || btbTable[index].tag != tag){
		btbTable[index] = btbUnit(pc, targetPc, true); // create a new entry in the BTB table
	}

	unsigned fixedHis;
	int shareBits = 0;
	if (Shared == USING_SHARE_LSB) {
		shareBits = 2;
	} else if (Shared == USING_SHARE_MID) {
		shareBits = 16;
	}


	
	if (btbTable[index].isValid && btbTable[index].tag == tag) {
		if (isGlobalTable) { // global table
			if (isGlobalHist) { // global table & global history
				if (shareBits == 0) {
					fixedHis = globalHistory;
				} else {
					fixedHis = globalHistory ^ ((pc & (((1 << historySize) - 1) << shareBits)) >> shareBits); // get the fixed history
				}	
				globalHistory = ((globalHistory << 1) | (taken ? 1 : 0)) & ((1 << historySize) - 1); // update the global history
			} else { // global table & local history
				if (shareBits == 0) {
					fixedHis = btbTable[index].history;
				} else {
					fixedHis = btbTable[index].history ^ ((pc & (((1 << historySize) - 1) << shareBits)) >> shareBits); // get the fixed history
				}
				btbTable[index].history = ((btbTable[index].history << 1) | (taken ? 1 : 0)) & ((1 << historySize) - 1); // update the local history
			}
			btbTable[index].target = targetPc; // update the target address
			if (taken) {
				if (globalFsm[fixedHis] == SNT) {
					globalFsm[fixedHis] = WNT; // update the FSM state
				} else if (globalFsm[fixedHis] == WNT) {
					globalFsm[fixedHis] = WT; // update the FSM state
				} else if (globalFsm[fixedHis] == WT) {
					globalFsm[fixedHis] = ST; // update the FSM state
				}
			} else {
				if (globalFsm[fixedHis] == ST) {
					globalFsm[fixedHis] = WT; // update the FSM state
				} else if (globalFsm[fixedHis] == WT) {
					globalFsm[fixedHis] = WNT; // update the FSM state
				} else if (globalFsm[fixedHis] == WNT) {
					globalFsm[fixedHis] = SNT; // update the FSM state
				}
			}
			
			} else { // local table
				if (isGlobalHist) { // local table & global history
					fixedHis = globalHistory;
					globalHistory = ((globalHistory << 1) | (taken ? 1 : 0)) & ((1 << historySize) - 1); // update the global history
				} else { // local table & local history
					fixedHis = btbTable[index].history;
					btbTable[index].history = ((btbTable[index].history << 1) | (taken ? 1 : 0)) & ((1 << historySize) - 1); // update the local history
				}
				btbTable[index].target = targetPc; // update the target address
				if (taken) {
					if (btbTable[index].localFsm[fixedHis] == SNT) {
						btbTable[index].localFsm[fixedHis] = WNT; // update the FSM state
					} else if (btbTable[index].localFsm[fixedHis] == WNT) {
						btbTable[index].localFsm[fixedHis] = WT; // update the FSM state
					} else if (btbTable[index].localFsm[fixedHis] == WT) {
						btbTable[index].localFsm[fixedHis] = ST; // update the FSM state
					}
				} else {
					if (btbTable[index].localFsm[fixedHis] == ST) {
						btbTable[index].localFsm[fixedHis] = WT; // update the FSM state
					} else if (btbTable[index].localFsm[fixedHis] == WT) {
						btbTable[index].localFsm[fixedHis] = WNT; // update the FSM state
					} else if (btbTable[index].localFsm[fixedHis] == WNT) {
						btbTable[index].localFsm[fixedHis] = SNT; // update the FSM state
					}
				}

			}
	}
	btbTable[index].isValid = true; // set the valid bit
}

void BP_GetStats(SIM_stats *curStats){
	curStats->flush_num = simStats.flush_num; // get the number of flushes
	curStats->br_num = simStats.br_num; // get the number of updates
	curStats->size = simStats.size; // get the size of the BTB table

	if (isGlobalTable) {
		while (!globalFsm.empty()) {
			globalFsm.pop_back(); // clear the global FSM
		}		
	} 

}

