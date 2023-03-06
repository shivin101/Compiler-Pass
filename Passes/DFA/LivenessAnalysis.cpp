#include "231DFA.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <deque>
#include <map>
#include <utility>
#include <vector>
#include <string>
#include "231DFA.h"
#include <set>
#include <algorithm>

using namespace llvm;
using namespace std;

namespace {
    class LivenessInfo : public Info {
    public:

        LivenessInfo() {}
        LivenessInfo(set<unsigned> s) {
            defs = s;
        }

        void print() {
            for (auto def : defs) {
                errs() << def << "|";
            }
            errs() << "\n";
        }


        static bool equals(LivenessInfo* info1, LivenessInfo* info2) {
            return info1->defs == info2->defs;
        }

        static LivenessInfo* join(LivenessInfo* info1, LivenessInfo* info2, LivenessInfo* result) {
            if(!equals(result,info1))
                result->defs.insert(info1->defs.begin(), info1->defs.end());
            result->defs.insert(info2->defs.begin(), info2->defs.end());

            return result;
        }
        

        

        set<unsigned> defs;
    };

    //Performs DFA analysis for Liveness check.
    class LivenessAnalysis : public DataFlowAnalysis<LivenessInfo, false> {
    public:
        LivenessAnalysis(LivenessInfo& bottom, LivenessInfo& initialState) :
            DataFlowAnalysis<LivenessInfo, false>::DataFlowAnalysis(bottom, initialState) { }

        ~LivenessAnalysis() {}
        
        void flowfunction(Instruction* I,
            std::vector<unsigned>& IncomingEdges,
            std::vector<unsigned>& OutgoingEdges,
            std::vector<LivenessInfo*> & Infos)
        {
            vector<LivenessInfo*> temp = Infos;
            if (I == nullptr) {
                return;
            }
            auto InstrToIndex = getInstrToIndex();
            auto EdgeToInfo = getEdgeToInfo();
            auto IndexToInstr = getIndexToInstr();
            
            auto index = InstrToIndex[I];


            //Infos.resize(OutgoingEdges.size());
            auto opname(I->getOpcodeName());
            
            set <unsigned> operands;
            for (unsigned i = 0; i < I->getNumOperands(); ++i) {
				Instruction *instr = (Instruction *) I->getOperand(i);

				if (InstrToIndex.count(instr) != 0)
					operands.insert(InstrToIndex[instr]);
			}

            LivenessInfo* info_in = new LivenessInfo();
            auto* locally_computed_liveness_info = new LivenessInfo();
            unsigned end = index;

            

            for (auto start : IncomingEdges) {
                auto edge = make_pair(start, end);
                LivenessInfo::join(info_in, EdgeToInfo[edge], info_in);
            }
            

            // unsigned category = 10;

            set<string> cat1 = { "alloca", "load", "select", "getelementptr","icmp","fcmp" };
            //Do not need to process category 2
            set<string> cat2 = { "br","switch", "store" };
            //For phi nodes
            set<string> cat3 = { "phi" };

            if (I->isBinaryOp() || cat1.find(opname) != cat1.end()) {
                
                LivenessInfo::join(new LivenessInfo(operands), info_in, info_in);
                info_in->defs.erase(index);
                
                for (int i = 0, size = OutgoingEdges.size(); i < size; i++) {
                    LivenessInfo* newInfo = new LivenessInfo();
                    newInfo->defs = info_in->defs;
                    Infos.push_back(newInfo);
                }
            }
            else if(cat2.find(opname) != cat2.end()){
                //join_operands(I, locally_computed_liveness_info, InstrToIndex);
                LivenessInfo::join(new LivenessInfo(operands), info_in, info_in);
                
                for (int i = 0, size = OutgoingEdges.size(); i < size; i++) {
                    LivenessInfo* newInfo = new LivenessInfo();
                    newInfo->defs = info_in->defs;
                    Infos.push_back(newInfo);
                }
            }
            else if (cat3.find(opname) != cat3.end()) {
                unsigned nonPhi;
                Instruction* curr = I;
                while (curr != nullptr && curr->getOpcodeName() == "phi")
                {
                    auto idx = InstrToIndex[curr];
                    info_in->defs.erase(idx);
                    curr = curr->getNextNode();
                    nonPhi = idx;
                }
                // Handle each output respectively
				
            }
            
            


           



            
            delete info_in;
        }
    };

    //Function pass for Liveness Analysis
    struct LivenessAnalysisPass : public FunctionPass {
        static char ID;

        LivenessAnalysisPass() : FunctionPass(ID) {}

        bool runOnFunction(Function& F) override {

            LivenessInfo bot;
            LivenessInfo initialState;
            LivenessAnalysis rda(bot, initialState);
            
            rda.runWorklistAlgorithm(&F);
            
            rda.print();
            return false;
        }
    };
}

char LivenessAnalysisPass::ID = 0;
static RegisterPass<LivenessAnalysisPass> X("cse231-liveness", "Code for part 3", false, false);
