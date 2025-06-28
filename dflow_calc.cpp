/* 046267 Computer Architecture - HW #3 */
/* Implementation (skeleton)  for the dataflow statistics calculator */

#include "dflow_calc.h"

#include <iostream>
#include <vector>

using namespace std;


class ProgInstraction {
    public:
        InstInfo instInfo;
        unsigned int instIndex;
        int latency;
        int depth;
        int src1DepInst;
        int src2DepInst;

        ProgInstraction(InstInfo info, unsigned int index = 0, int latency = 0, int depth = 0, int src1DepInst = -1, int src2DepInst = -1){
            this->instInfo = info;
            this->instIndex = index;
            this->latency = latency;
            this->depth = depth;
            this->src1DepInst = src1DepInst;
            this->src2DepInst = src2DepInst;
        }

        ~ProgInstraction() {
          
        }
};

typedef vector<ProgInstraction> ProgInstList;


ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts) {
    //return PROG_CTX_NULL;

    ProgInstList *progInstList = new ProgInstList();
    if (progInstList == nullptr) {
        return PROG_CTX_NULL; // Memory allocation failed
    }

    //initialize the program instruction list
    for (unsigned int i = 0; i < numOfInsts; ++i) {
        InstInfo info = progTrace[i];
        unsigned int opcode = info.opcode;
        
        ProgInstraction inst(info, i , opsLatency[opcode]);
        //progInstList.push_back(inst);

        //check for dependencis in previous instructions
        for (unsigned int j = 0; j < i; ++j) {

            if (progInstList->at(j).instInfo.dstIdx == (int)info.src1Idx) {
                inst.src1DepInst = j;
            }
            if (progInstList->at(j).instInfo.dstIdx == (int)info.src2Idx) {
                inst.src2DepInst = j;
            }
        }

        unsigned int instDepth1 = (inst.src1DepInst != -1) ? progInstList->at(inst.src1DepInst).depth + progInstList->at(inst.src1DepInst).latency : 0;
        unsigned int instDepth2 = (inst.src2DepInst != -1) ? progInstList->at(inst.src2DepInst).depth + progInstList->at(inst.src2DepInst).latency : 0;
        
        //depth of instruction is the longest from the two dpendencies 
        unsigned int maxDepth = max(instDepth1, instDepth2);
        inst.depth = maxDepth;
        
        progInstList->push_back(inst);
    }

    return (ProgCtx)progInstList; 
    
}

void freeProgCtx(ProgCtx ctx) {
    delete (ProgInstList *)ctx;
    
}

int getInstDepth(ProgCtx ctx, unsigned int theInst) {

    vector <ProgInstraction> *prog = (vector <ProgInstraction> *)ctx;
    if (theInst < prog->size()) {
        return prog->at(theInst).depth;
    }
    return -1;
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst) {

    vector <ProgInstraction> *prog = (vector <ProgInstraction> *)ctx;
    if (theInst < prog->size()) {
        *src1DepInst = prog->at(theInst).src1DepInst;
        *src2DepInst = prog->at(theInst).src2DepInst;
        return 0;
    }
    return -1;
}

int getProgDepth(ProgCtx ctx) {
    vector <ProgInstraction> *prog = (vector <ProgInstraction> *)ctx;

    int maxDepth = 0, tempDepth = 0;
    for (unsigned int i = 0; i < prog->size(); ++i) {
        tempDepth = prog->at(i).depth + prog->at(i).latency;
        if (tempDepth > maxDepth) {
            maxDepth = tempDepth;
        }
    }

    return maxDepth;

}


