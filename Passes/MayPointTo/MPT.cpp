#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/ConstantFolder.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Support/raw_ostream.h"
#include "231DFA.h"
#include <map>
#include <vector>
#include<string>
#include<set>
#include<unordered_map>

#define NULL nullptr
using namespace llvm;
using namespace std;
namespace {
    enum ConstState { Bottom, Const, Top };
    typedef std::pair<ConstState, Constant*> ConstPair;
    typedef std::unordered_map<Value*, ConstPair> ConstPropContent;

    set<Value*> Insts;
    set<Value*> MPT;
    ConstantFolder Folder;
    unordered_map<Function*, std::set<GlobalVariable*>> MOD;
    unordered_map<Value*, string> globalName;


    class ConstPropInfo : public Info {
    public:
       ConstPropInfo(ConstState state = Bottom) {
            for (auto val : Insts) {
                if (state == Top) {
                    setTop(val);
                }
                else {
                    setBottom(val);
                }
            }
        }
        ConstPropContent ConstMap;
        
        void print() {
            
            for (auto& keyval : ConstMap) {
                Value* val = keyval.first;
                if (GlobalVariable* gval = dyn_cast<GlobalVariable>(val)) {
                    ConstState state = keyval.second.first;
                    Constant* value = keyval.second.second;
                    if (state == Const) {
                        errs() << gval->getName() << '=' << *value << '|';
                    }
                    if (state == Bottom) {
                        errs() << gval->getName() << "=⊥|";
                    }
                    if (state == Top) {
                        errs() << gval->getName() << "=⊤|";
                    }
                }
            }
            errs() << "\n";

        }
        



        //override to define join operation in constant propogation.
        static ConstPropInfo* join(ConstPropInfo* info1, ConstPropInfo* info2, ConstPropInfo* result)
        {
            
            for (auto keyval : info1->ConstMap) {

                Value* key = keyval.first;
                auto state1 = info1->getState(key);
                auto state2 = info2->getState(key);

                

                if (state1 == Top ) {
                    result->setTop(key);
                }
                else if (state2 == Top) {
                    result->setTop(key);
                }
                else if (state1 == state2 && state2 == Const) {
                    
                    if (info1->getConst(key)==info2->getConst(key)) {
                        result->setConst(key, info1->getConst(key));
                    }
                    else {
                        result->setTop(key);
                    }
                }
                else if (state1 == Bottom || state2 == Bottom) {
                    if (state1 == state2) {
                        result->setBottom(key);
                    }
                    else if (state1 == Const) {
                        auto value1 = info1->getConst(key);
                        result->setConst(key, value1);
                    }
                    else {
                        auto value2 = info2->getConst(key);
                        result->setConst(key, value2);
                    }
                }
                

            }

            return result;
        }

        //define override operation for equals operation.
        static bool equals(ConstPropInfo* info1, ConstPropInfo* info2) {


            for (auto keyval : info1->ConstMap) {
                auto key = keyval.first;

                auto state1 = info1->getState(key);
                auto state2 = info2->getState(key);

                auto value1 = info1->getConst(key);
                auto value2 = info2->getConst(key);

                if (state1 == state2) {
                    if (state1 == Const) {
                        if (value1 == value2) {
                            continue;
                        }
                        else {
                            return false;
                        }
                    }
                    else {
                        continue;
                    }
                }
                else {
                    return false;
                }
            }
            return true;
        }

        //define inserntion operation in the contant propogation map
        void make_insert(Value*I,ConstPair newPair ) {
            if (ConstMap.find(I) != ConstMap.end()) {
                auto it = ConstMap.find(I);
                it->second = newPair;
            }
            else {
                ConstMap.insert({ I,newPair });
            }
        }

        //Utility functions.
        void setConst(Value* I, Constant* val) {

            ConstPair newPair = make_pair(Const, val);
            make_insert(I, newPair);

        }
        void setTop(Value* I) {

            ConstPair newPair = make_pair(Top, NULL);
            make_insert(I, newPair);
        }

        void setBottom(Value* I) {
            ConstPair newPair = make_pair(Bottom, NULL);
            make_insert(I, newPair);

        }
        Constant* getConst(Value* I) {
            auto it = ConstMap.find(I);
            if (it != ConstMap.end()) {
                return it->second.second;
            }
            return NULL;

        }
        ConstState getState(Value* I) {
            auto it = ConstMap.find(I);
            if (it != ConstMap.end()) {
                return it->second.first;
            }
            return Top;

        }







    };



    //DFA for constant propogation 
    class ConstPropAnalysis : public DataFlowAnalysis<ConstPropInfo, true> {
    public:
        ConstPropAnalysis(ConstPropInfo& bottom, ConstPropInfo& initialState) :
            DataFlowAnalysis<ConstPropInfo, true>::DataFlowAnalysis(bottom, initialState) { }

        ~ConstPropAnalysis() {}

        Constant* MapConst(ConstPropInfo* I, Value* c) {
            Constant* val = dyn_cast<Constant>(c);
            if (!val) {
                return I->getConst(c);
            }
            else return val;
        }
        void flowfunction(Instruction* I,
            std::vector<unsigned>& IncomingEdges,
            std::vector<unsigned>& OutgoingEdges,
            std::vector<ConstPropInfo*>& Infos)
        {


            if (I == nullptr) {
                return;
            }
            auto InstrToIndex = getInstrToIndex();
            auto EdgeToInfo = getEdgeToInfo();
            auto IndexToInstr = getIndexToInstr();

            auto index = InstrToIndex[I];
            auto opname(I->getOpcodeName());





            ConstPropInfo* info = new ConstPropInfo();
            auto* locally_computed_liveness_info = new ConstPropInfo();
            unsigned end = index;


            for (auto start : IncomingEdges) {
                auto edge = make_pair(start, end);
                ConstPropInfo::join(info, EdgeToInfo[edge], info);
            }
            
            //Operation for binary operator
            if (isa<BinaryOperator>(I)) {
                BinaryOperator* bin_op = dyn_cast<BinaryOperator>(I);

                Value* x = I->getOperand(0);
                Value* y = I->getOperand(1);

                Constant* cons_x = MapConst(info, x);
                Constant* cons_y = MapConst(info, y);
                if (cons_x && cons_y) {
                    info->setConst(I, Folder.CreateBinOp(bin_op->getOpcode(), cons_x, cons_y));

                }
                else
                {
                    info->setTop(I);
                }
            }

            //Operations for PHI node
            if (isa<PHINode>(I)) {
                auto phi_node = dyn_cast<PHINode>(I);
                int count = 0;
                for (auto& cur_val : phi_node->incoming_values()) {
                    count += 1;

                }

                //incoming edges into a phi node
                auto incoming1 = phi_node->getIncomingValue(0);
                auto incoming2 = phi_node->getIncomingValue(2);

                Constant* cons_x = dyn_cast<Constant>(incoming1);
                Constant* cons_y = dyn_cast<Constant>(incoming2);
                if (!cons_x) {
                    cons_x = info->getConst(incoming1);
                }
                if (!cons_y) {
                    cons_y = info->getConst(incoming2);
                }
                if (cons_x && cons_y) {
                    info->setConst(I, cons_x);
                }
                else
                {
                    info->setTop(I);

                    uint32_t val = 0;

                    for (auto& keyval : info->ConstMap) {
                        Value* val = keyval.first;

                        ConstState state = keyval.second.first;
                        Constant* value = keyval.second.second;
                        if (auto constant_val = info->getConst(val)) {
                            info->setConst(I, value);

                        }
                    }

                }
            }
            //OPperation for Select Instruction
            if (SelectInst* select_inst = dyn_cast<SelectInst>(I)) {
                Value* cond = I->getOperand(0);
                Value* x = I->getOperand(1);
                Value* y = I->getOperand(2);

                Constant* cons_cond = MapConst(info,cond);
                Constant* cons_x = MapConst(info,x);
                Constant* cons_y = MapConst(info,y);

                if (cons_x && cons_y && cons_cond) {
                    info->setConst(I, Folder.CreateSelect(cons_cond, cons_x, cons_y));
                }
                else if (cons_x && cons_y) {
                    if (cons_x == cons_y) {
                        info->setConst(I, cons_x);
                    }
                    else {
                        info->setTop(I);
                    }
                }
                else {
                    info->setTop(I);
                }

            }
            //Operation for Call Instruction
             if (CallInst* callInst = dyn_cast<CallInst>(I)) {
                

                for (Value* glob : MOD[callInst->getCalledFunction()]) {
                    info->setTop(glob);
                }
            }
             if (LoadInst* load_inst = dyn_cast<LoadInst>(I)) {
                
                Value* load_ptr = load_inst->getPointerOperand();

                auto val = info->getConst(load_ptr);
                if (val != NULL) {
                    info->setConst(I, val);
                }
                else {
                    info->setTop(I);
                }

            }

             //Operation for Store Instruction.
             if (StoreInst* strI = dyn_cast<StoreInst>(I)) {
                auto valOperand = strI->getValueOperand();
                auto pointerOperand = strI->getPointerOperand();

                auto load_ptr = dyn_cast<LoadInst>(pointerOperand);
                Constant* const_val = dyn_cast<Constant>(valOperand);
                if (load_ptr) { 
                    for (auto mpt_itr : MPT) {
                        info->setTop(mpt_itr);
                    }
                }
                else {
                    if (const_val) { 
                        info->setConst(pointerOperand, const_val);
                    }
                    else {
                        auto load_val = dyn_cast<LoadInst>(valOperand);
                        if (load_val) {
                            auto load_val_ptr = load_val->getPointerOperand();
                            if (auto alloc_x = dyn_cast<AllocaInst>(load_val_ptr)) { 
                                StoreInst* str_inst = dyn_cast<StoreInst>(I);
                                auto val = info->getConst(load_val_ptr);
                                if (val != NULL) {
                                    info->setConst(pointerOperand, val);
                                }
                                else {
                                    info->setTop(pointerOperand);
                                }
                            }
                            else {

                                info->setTop(pointerOperand);
                                
                            }

                        }
                        else { 
                            auto val = info->getConst(valOperand);
                            if (val != NULL) {
                                info->setConst(pointerOperand, val);
                            }
                            else {
                                info->setTop(pointerOperand);
                            }

                        }

                    }
                }
            }

            for (size_t i = 0; i < OutgoingEdges.size(); ++i) {
                ConstPropInfo* newInfo = new ConstPropInfo();
                newInfo->ConstMap = info->ConstMap;
                Infos.push_back(newInfo );
            }
            
            delete info;



        }
    };




    // Function Analysi for a Call Graph Pass
    class ConstPropAnalysisPass : public CallGraphSCCPass {
    public:
        static char ID;


        ConstPropAnalysisPass() : CallGraphSCCPass(ID) {}
        // Initialize call graph and start function processing
        bool doInitialization(CallGraph& CG) override {


            for (auto& glob : CG.getModule().getGlobalList()) {

                GlobalVariable* var2 = dyn_cast<GlobalVariable>(&glob);
                Insts.insert(var2);

            }

            //Insttru processing.
            for (Function& F : CG.getModule().functions()) {
                for (Function::iterator bi = F.begin(), e = F.end(); bi != e; ++bi) {
                    BasicBlock* block = &*bi;

                    for (auto ii = block->begin(), ie = block->end(); ii != ie; ++ii) {
                        Instruction* I = &*ii;
                        Insts.insert(I);
                        //Store instruction processing
                        if (StoreInst* strI = dyn_cast<StoreInst>(I)) {
                            
                            auto valOperand = strI->getValueOperand();
                            auto pointerOperand = strI->getPointerOperand();
                           

                            
                            MPT.insert(valOperand);
                            Insts.insert(pointerOperand);
                            
                        }
                        //Call Instruction processing
                        else if (CallInst* callI = dyn_cast<CallInst>(I)) {
                            for (Use& operand : callI->args()) {
                                MPT.insert(operand);
                            }
                        }
                        //Return Instuction processing
                        else if (ReturnInst* retInst = dyn_cast<ReturnInst>(I)) {
                            for (Use& operand : I->operands()) {
                                MPT.insert(operand);
                            }
                        }
                        else {
                            
                        }

                    }
                }
            }

            for (Function& F : CG.getModule().functions()) {
                std::set<GlobalVariable*> func_mod;
                for (Function::iterator bi = F.begin(), e = F.end(); bi != e; ++bi) {
                    BasicBlock* block = &*bi;

                    for (auto ii = block->begin(), ie = block->end(); ii != ie; ++ii) {
                        Instruction* I = &*ii;
                        auto opname(I->getOpcodeName());
                        //Only have to process store instruction
                        if (!strcmp(opname, "store")) {
                            auto store_inst = dyn_cast<StoreInst>(I);
                            auto val_operand = store_inst->getValueOperand();
                            auto ptr_operand = store_inst->getPointerOperand();
                            GlobalVariable* glob_ptr = dyn_cast<GlobalVariable>(ptr_operand);
                            if (glob_ptr) {
                                func_mod.insert(glob_ptr);
                            }
                        }
                        if (!strcmp(opname, "store")) {
                            StoreInst* store_inst = dyn_cast<StoreInst>(I);
                            auto ptr_operand = store_inst->getPointerOperand();
                            LoadInst* load_inst = dyn_cast<LoadInst>(ptr_operand);
                            if (load_inst) { 
                                for (auto variable : MPT) {
                                    GlobalVariable* glob_ptr = dyn_cast<GlobalVariable> (variable);
                                    if (glob_ptr) {
                                        func_mod.insert(glob_ptr);
                                    }
                                }
                            }
                        }
                    }
                }
                std::pair<Function*, std::set<GlobalVariable*>> func_mod_pair(&F, func_mod); // &F
                MOD.insert(func_mod_pair);
                for (auto x : func_mod) { // does not do U (MPT || phi) -
                    
                }
            }
            return false;
        }

        //Functions for running calls on Call Graph
        bool runOnSCC(CallGraphSCC& SCC) override {

            std::set<GlobalVariable*> scc_mod;
            for (CallGraphNode* caller_node : SCC) {
                Function* caller_func = caller_node->getFunction();
                auto caller_mod = MOD[caller_func];
                if (caller_func) {
                    for (auto x : caller_mod) {
                        scc_mod.insert(x);
                       
                    }

                    for (auto pair : *caller_node) {
                        CallGraphNode* callee_node = pair.second;
                        Function* callee_func = callee_node->getFunction();
                        for (auto x : MOD[callee_func]) { 
                            scc_mod.insert(x);
                        }
                    }
                }
            }

            for (CallGraphNode* caller_node : SCC) {
                Function* caller_func = caller_node->getFunction();

                if (caller_func) {
                    MOD[caller_func] = scc_mod;
                }
            }
            return false;
        }

        bool doFinalization(CallGraph& CG) override {
            for (Function& F : CG.getModule().functions()) {
                ConstPropInfo bot = ConstPropInfo(Bottom);
                ConstPropInfo initialState = ConstPropInfo(Top);
                ConstPropAnalysis rda(bot, initialState);

                rda.runWorklistAlgorithm(&F);

                rda.print();
            }
            return false;
        }

    };
}
char ConstPropAnalysisPass::ID = 0;
static RegisterPass<ConstPropAnalysisPass> X("cse231-constprop", "Do Inter on CFG",
    false /* Only looks at CFG */,
    false /* Analysis Pass */);