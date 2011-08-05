//===- YarnLoop.cpp - Example code from "Writing an LLVM Pass" ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the FreeBSD License.
// See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Yarnc loop analysis pass.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "yarn-loopinfo"
#include "YarnUtil.h"
#include <llvm/Yarn/YarnLoopInfo.h>
#include <llvm/Value.h>
#include <llvm/User.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/Statistic.h>
#include <algorithm>
using namespace llvm;
using namespace yarn;


//===----------------------------------------------------------------------===//
/// Typedefs

typedef std::vector<BasicBlock*> BBList;


//===----------------------------------------------------------------------===//
/// LoopPointer

STATISTIC(LoopsKept, "Loops retained for intrumentation");
STATISTIC(LoopValues, "Dependency values found in loop.");
STATISTIC(LoopPointers, "Dependency pointers found in loop.");
STATISTIC(LoopInvariants, "Invariants found in loop.");


#define PRINT_VAL(OS, LVL, VAL)				\
  do {							\
    if (!VAL) break;					\
    OS << LVL << (#VAL) << " = ";			\
    (VAL)->print(OS);					\
    OS << " --- ";					\
    (VAL)->getType()->print(OS);			\
    OS << "\n";						\
  } while(false);

//===----------------------------------------------------------------------===//
/// LoopPointer

void LoopPointer::print (raw_ostream &OS) const {
  std::string LVL = "\t\t";
  const std::string LVL2 = "\t\t\t";
  OS << LVL << "LoopPointer - Aliasses:\n";
  
  for (size_t i = 0; i < Aliases.size(); ++i) {
    PRINT_VAL(OS, LVL2, Aliases[i]);
  }
  
}


//===----------------------------------------------------------------------===//
/// LoopValue

void LoopValue::print (raw_ostream &OS) const {
  const std::string LVL = "\t\t";
  const std::string LVL2 = "\t\t\t";
  const std::string LVL3 = "\t\t\t\t";

  OS << LVL << "LoopValue:\n";
  
  PRINT_VAL(OS, LVL2, HeaderNode);
  PRINT_VAL(OS, LVL2, FooterNode);
  PRINT_VAL(OS, LVL2, EntryValue);
  PRINT_VAL(OS, LVL2, IterationValue);

  OS << LVL2 << "ExitingValues:\n";
  for (size_t i = 0; i < ExitingValues.size(); ++i) {
    PRINT_VAL(OS, LVL3, ExitingValues[i]);
  }

}

//===----------------------------------------------------------------------===//
/// PointerInstr

void PointerInstr::print (raw_ostream &OS) const {
  const std::string LVL = "\t\t";
  const std::string LVL2 = "\t\t\t";

  OS << LVL << "PointerInstr:\n";
  
  OS << LVL2 << "Type = " << (Type == InstrLoad ? "Load" : "Store") << "\n";
  PRINT_VAL(OS, LVL2, I);
}

//===----------------------------------------------------------------------===//
/// ValueInstr

void ValueInstr::print (raw_ostream &OS) const {
  const std::string LVL = "\t\t";
  const std::string LVL2 = "\t\t\t";

  OS << LVL << "ValueInstr:\n";

  OS << LVL2 << "Type = " << (Type == InstrLoad ? "Load" : "Store") << "\n";
  PRINT_VAL(OS, LVL2, V);
  PRINT_VAL(OS, LVL2, Pos);
}

//===----------------------------------------------------------------------===//
/// ArrayEntry

void ArrayEntry::print (raw_ostream &OS) const {
  const std::string LVL = "\t\t";
  const std::string LVL2 = "\t\t\t";

  OS << LVL << "ArrayEntry:\n";
  
  PRINT_VAL(OS, LVL2, EntryValue);
  PRINT_VAL(OS, LVL2, ExitNode);
  OS << LVL2 << "IsInvariant = " << IsInvariant << "\n";
  PRINT_VAL(OS, LVL2, Pointer);
  PRINT_VAL(OS, LVL2, NewValue);
}




//===----------------------------------------------------------------------===//
/// YarnLoop

YarnLoop::YarnLoop(Function* f,
		   Loop* l, 
		   LoopInfo* li,
		   AliasAnalysis* aa, 
		   DominatorTree* dt,
		   PostDominatorTree* pdt) 
:
  LI(li), AA(aa), DT(dt), PDT(pdt),
  F(f), L(l), Dependencies(), Pointers(), Invariants(),
  PointerInstrs(), ValueInstrs(), ArrayEntries()
{
  processLoop();
}

YarnLoop::~YarnLoop() {
  VectorUtil<LoopValue>::free(Dependencies);
  VectorUtil<LoopPointer>::free(Pointers);
  VectorUtil<PointerInstr>::free(PointerInstrs);
  VectorUtil<ValueInstr>::free(ValueInstrs);
  VectorUtil<ArrayEntry>::free(ArrayEntries);
}


LoopValue* YarnLoop::getDependencyForValue (Value* val) {
  for (ValueList::iterator it = Dependencies.begin(), itEnd = Dependencies.end();
       it != itEnd; ++it)
  {
    LoopValue* lv = *it;
    if (lv->getEntryValue() == val || lv->getExitValue() == val) {
      return lv;
    }
  }

  return NULL;
}


void YarnLoop::processLoop () {

  ValueMap exitingValueMap;
  PointerInstSet loadSet;
  PointerInstSet storeSet;

  for (Loop::block_iterator bb = L->block_begin(), bbEnd = L->block_end(); 
       bb != bbEnd; ++bb) 
  {
    bool isHeader = *bb == L->getHeader();

    for (BasicBlock::iterator i = (*bb)->begin(), iEnd = (*bb)->end(); i != iEnd; ++i) {
      Instruction* inst = &(*i);

      PHINode* phi = dyn_cast<PHINode>(inst);
      if (isHeader && phi) {
	processHeaderPHINode(phi, exitingValueMap);
      }

      else if(StoreInst* si = dyn_cast<StoreInst>(inst)) {
	Value* ptr = si->getPointerOperand();
	storeSet.insert(ptr);
	loadSet.erase(ptr);
      }

      else if (LoadInst* li = dyn_cast<LoadInst>(inst)) {
	Value* ptr = li->getPointerOperand();
	if (storeSet.find(ptr) == storeSet.end()) {
	  loadSet.insert(ptr);
	}
      }

      else {
	processInvariants(inst);
      }      

    }
  }

  processPointers(loadSet, storeSet);

  BasicBlock* exitBlock = L->getUniqueExitBlock();
  assert(exitBlock && "SimplifyLoop should ensure a single exit block.");
  for (BasicBlock::iterator i = exitBlock->begin(), iEnd = exitBlock->end(); 
       i != iEnd; ++i) 
  {  
    if (PHINode* phi = dyn_cast<PHINode>(&(*i))) {
      processFooterPHINode(phi, exitingValueMap);
    }
  }
  
  processArrayEntries();

}

// The SimplifyLoop pass ensures that we have only one back-edge in the loop and only
// one incomming edge into the loop. This means that the header of the loop will contain
// PHI nodes with all the dependencies used before the loop. We also reccord any
// exiting values associated with these so that they can later be used by 
// processFooterPHINode. Note that the exiting values here might be incomplete so we have
// to wait.
void YarnLoop::processHeaderPHINode (PHINode* PHI, ValueMap& ExitingValueMap) {
  BasicBlock* loopPred = L->getLoopPredecessor();

  LoopValue* lv = new LoopValue();
  lv->HeaderNode = PHI;

  for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
    BasicBlock* incBB = PHI->getIncomingBlock(i);
    Value* incValue = PHI->getIncomingValue(i);

    if (loopPred == incBB) {
      lv->EntryValue = incValue;
    }
    else {
      lv->IterationValue = incValue;
    }
  }

  assert((lv->EntryValue != NULL && lv->IterationValue != NULL) &&
	 "SimplifyLoop should ensure only one exiting and one entry value.");

  Dependencies.push_back(lv);
  ExitingValueMap[lv->IterationValue] = lv;

}

// The LCSSA and the SimplifyLoop pass ensures that we have a unique footer for the loop
// which contains PHI nodes of dependencies that are used after the loop.
// This functions looks at these to find the exit value and the exiting values for a 
// LoopValue.
void YarnLoop::processFooterPHINode(PHINode* PHI, ValueMap& ExitingValueMap) {

  LoopValue* lv = NULL;
  LoopValue::ValueList exitingValList;

  for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
    if (!isInLoop(L, PHI->getIncomingValue(i))) {
      continue;
    }

    Value* exitingValue = PHI->getIncomingValue(i);
    exitingValList.push_back(exitingValue);
    
    // Do we already know the value from the header?
    ValueMap::iterator it = ExitingValueMap.find(exitingValue);
    if (it != ExitingValueMap.end()) {
      assert(!lv && "Can't belong to two LoopValues.");
      lv = it->second;
    }
  }

  // If we didn't match anything from the header then 
  // this is a new exit-only value.
  if (!lv) {
    lv = new LoopValue();
    Dependencies.push_back(lv);
  }

  assert(lv->FooterNode == NULL &&
	 "LCSSA should ensure only one exit value.");
  lv->FooterNode = PHI;
  lv->ExitingValues.insert(lv->ExitingValues.end(), 
			   exitingValList.begin(), 
			   exitingValList.end());
}


namespace {

  typedef std::map<Value*, LoopPointer*> PointerMap;

  LoopPointer* isKnownAlias (AliasAnalysis* AA, 
			     const PointerMap& StoreMap, 
			     Value* Ptr,
			     bool Strict) 
  {

    for(PointerMap::const_iterator it = StoreMap.begin(), endIt = StoreMap.end(); 
	it != endIt; ++it)
    {
      AliasAnalysis::AliasResult ar = AA->alias((*it).first, Ptr);
      
      if (Strict && ar == AliasAnalysis::MustAlias) {
	return (*it).second;
      }
      else if (!Strict && ar) {
	return (*it).second;
      }
    }

    return NULL;
  }

} // Anonymous namespace


/// \todo This will pick up strictly local load and stores that may not alias with
///   another loop iteration. So a pointer that manipulates a piece of memory that was
///   allocated and freed during the same iteration, will be instrumented.
///   Not sure how to find and filter out these pointers.
void YarnLoop::processPointers (PointerInstSet& LoadSet, PointerInstSet& StoreSet) {

  PointerMap storeMap;

  for (PointerInstSet::iterator it = StoreSet.begin(), itEnd = StoreSet.end(); 
       it != itEnd; ++it)
  {
    LoopPointer* lp = isKnownAlias(AA, storeMap, *it, true);
    if (!lp) {
      lp = new LoopPointer();
      Pointers.push_back(lp);
    }

    lp->Aliases.push_back(*it);
    storeMap[*it] = lp;
  }

  PointerInstSet::iterator it = LoadSet.begin();
  PointerInstSet::iterator itEnd = LoadSet.end(); 
  while (it != itEnd) {
    
    // If we're a strict alias then just add ourself to our alias.
    LoopPointer* lp = isKnownAlias(AA, storeMap, *it, true);
    if (lp) {
      lp->Aliases.push_back(*it);
      continue;
    }

    // If we might alias then we still need to instrument.
    if (isKnownAlias(AA, storeMap, *it, false) != NULL) {
      lp = new LoopPointer();
      lp->Aliases.push_back(*it);
      Pointers.push_back(lp);
      continue;
    }

    // We only read the pointer so check if we're an invariant.
    if (L->isLoopInvariant(*it)) {
      Invariants.insert(*it);
    }
    
  }  

}



// An invariant means that 
void YarnLoop::processInvariants (Instruction* Inst) {

  for (unsigned int i = 0, iEnd = Inst->getNumOperands(); i < iEnd; ++i) {
    Value* operand = Inst->getOperand(i);
    if (!isa<Instruction>(operand) && 
	!isa<Argument>(operand) && 
	!isa<Operator>(operand)) 
    {
      continue;
    }
    if (!L->isLoopInvariant(operand)) {
      continue;
    }
    Invariants.insert(operand);
  }

}


void YarnLoop::processArrayEntries () {
  unsigned Index = 0;
  for (ValueList::iterator it = Dependencies.begin(), itEnd = Dependencies.end();
       it != itEnd; ++it)
  {
    LoopValue* lv = *it;

    ArrayEntry* ae = new ArrayEntry(lv->getEntryValue(), lv->getFooterNode(), false);
    ArrayEntries.push_back(ae);
    processValueInstrs(lv, Index);
    Index++;
  }

  for (PointerList::iterator it = Pointers.begin(), itEnd = Pointers.end(); 
       it != itEnd; ++it)
  {
    LoopPointer* lp = *it;

    typedef LoopPointer::AliasList AliasList;
    AliasList& aliases = lp->getAliasList();
    for (AliasList::iterator aliasIt = aliases.begin(), aliasEndIt = aliases.end();
	 aliasIt != aliasEndIt; ++aliasIt)
    {
      Value* alias = *aliasIt;
      if (!isInLoop(L, alias)) {
	ArrayEntry* ae = new ArrayEntry(alias, NULL, true);
	ArrayEntries.push_back(ae);
      }
    }

    processPointerInstrs(lp);
  }

  for (InvariantList::iterator it = Invariants.begin(), itEnd = Invariants.end();
       it != itEnd; ++it) 
  {
    ArrayEntry* ae = new ArrayEntry(*it, NULL, true);
    ArrayEntries.push_back(ae);
  }
  
}



namespace {

  template<typename T>
  class FindValuePredicate : public std::unary_function<bool, T*> {

    T* ToFind;

  public :

    FindValuePredicate (T* toFind) : ToFind(toFind) {}
    bool operator() (const T* user) {
      return ToFind == user;
    }

  };

} // Anonymous namespace


/// \todo This is currently not optimal.
///   If the two earliest stores are done on two parallel branches then we should
///   return the two load instructions and not their dominator.
YarnLoop::BBPosList YarnLoop::findLoadPos (Value* V) const {
  // Find the common dominator for all the load operations.
  BasicBlock* loadBB = NULL;
  for (Value::use_iterator it = V->use_begin(), endIt = V->use_end();
       it != endIt; ++it)
  {
    Instruction* inst = dyn_cast<Instruction>(*it);
    assert(inst && "Sanity check.");
    
    if (!isInLoop(L, inst)) {
      continue;
    }

    BasicBlock* bb = inst->getParent();
    if (loadBB == NULL) {
      loadBB = bb;
      continue;
    }

    loadBB = DT->findNearestCommonDominator(loadBB, bb);
    assert(loadBB && 
	   "LoopSimplify ensures that there's always a common dominator.");
  }

  // If the common dominator contains a load instruction, 
  //    return the pos right before it.
  // otherwise return the last position in the BB.
  BBPosList posList;
  for (BasicBlock::iterator it = loadBB->begin(), itEnd = loadBB->end();
       it != itEnd; ++it)
  {
    Instruction* inst = &(*it);

    FindValuePredicate<User> pred(inst);
    Value::use_iterator findIt = std::find_if(V->use_begin(), V->use_end(), pred);
    if (findIt != V->use_end()) {
      posList.push_back(inst);
      return posList;
    }
  }

  posList.push_back(loadBB);
  return posList;
}

namespace {

  typedef std::vector<Instruction*> InstructionList;

  void findWriteInstructions (PHINode* PHI, InstructionList& OutList) {

    for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
      Value* incValue = PHI->getIncomingValue(i);

      if (PHINode* node = dyn_cast<PHINode>(incValue)) {
	findWriteInstructions(node, OutList);
      }
      else if (Instruction* inst = dyn_cast<Instruction>(incValue)) {
	OutList.push_back(inst);
      }

    }
  }

} // Anonymous namespace

YarnLoop::BBPosList YarnLoop::findStorePos (Value* V) const {
  assert(V && "V == NULL");

  InstructionList storeList;

  // Gather the list of all the non-phi store instruction for the value.
  if (PHINode* phi = dyn_cast<PHINode>(V)) {
    findWriteInstructions(phi, storeList);
  }
  else if (Instruction* inst = dyn_cast<Instruction>(V)) {
    storeList.push_back(inst);
  }
  else {
    assert(false && "findStore requires an instruction as an input.");
  }
  
  // Find the common dominator for all the load operations.
  BasicBlock* storeBB = NULL;
  for (InstructionList::iterator it = storeList.begin(), endIt = storeList.end();
       it != endIt; ++it)
  {
    BasicBlock* bb = (*it)->getParent();
    if (storeBB == NULL) {
      storeBB = bb;
      continue;
    }

    storeBB = PDT->findNearestCommonDominator(storeBB, bb);
    assert(storeBB && 
	   "LoopSimplify ensures that there's always a common dominator.");
  }

  // If the common dominator contains a store instruction, 
  //    return the pos right after it.
  // otherwise return the first valid position in the BB.
  BBPosList posList;
  BasicBlock::iterator it = storeBB->end();
  do {
    it--;
    
    Instruction* inst = &(*it);

    FindValuePredicate<Instruction> pred(inst);
    InstructionList::iterator findIt = 
      std::find_if(storeList.begin(), storeList.end(), pred);

    if (findIt != storeList.end()) {
      posList.push_back(inst);
      return posList;
    }
  } while (it != storeBB->begin());

  posList.push_back(storeBB);
  return posList;

}


// Ugliest for loops EVAR...
void YarnLoop::processPointerInstrs (LoopPointer* lp) {
  typedef LoopPointer::AliasList AliasList;

  AliasList& aliasList = lp->getAliasList();
  for (AliasList::iterator aliasIt = aliasList.begin(), aliasEndIt = aliasList.end();
       aliasIt != aliasEndIt; ++aliasIt)
  {

    Value* alias = *aliasIt;
    for (Value::use_iterator useIt =  alias->use_begin(), useEndIt = alias->use_end();
	 useIt != useEndIt; ++useIt)
    {
      User* user = *useIt;
      // Make sure we're still in the loop.
      if (!isInLoop(L, user)) {
	continue;
      }

      PointerInstr* pip = NULL;

      if (StoreInst* si = dyn_cast<StoreInst>(user)) {
	pip = new PointerInstr(InstrStore, si);
      }
      else if (LoadInst* li = dyn_cast<LoadInst>(user)) {
	pip = new PointerInstr(InstrLoad, li);
      }
      else {
	continue;
      }
	
      PointerInstrs.push_back(pip);

    } // use iteration
  } // alias iteration
}
 

 void YarnLoop::processValueInstrs (LoopValue* lv, unsigned index) {

   std::set<Value*> itValues;

  if (!lv->isExitOnly()) {

    // Load of the iteration values.
    {
      Value* startItVal = lv->getStartIterationValue();
      BBPosList posList = findLoadPos(startItVal);
      assert(posList.size() == 1 &&
	     "We don't yet support multiple load points"
	     " and there should be at least one load point.");

      for (BBPosList::iterator posIt = posList.begin(), posEndIt = posList.end();
	   posIt != posEndIt; ++posIt)
      {
	ValueInstr* vip = new ValueInstr(InstrLoad, startItVal, *posIt, index);
	ValueInstrs.push_back(vip);
      }
    }

    // Store of the iteration values.
    {
      Value* endItVal = lv->getEndIterationValue();
      itValues.insert(endItVal);

      BBPosList posList = findStorePos(endItVal);

      for (BBPosList::iterator posIt = posList.begin(), posEndIt = posList.end();
	   posIt != posEndIt; ++posIt)
      {
	ValueInstr* vip = new ValueInstr(InstrStore, endItVal, *posIt, index);
	ValueInstrs.push_back(vip);
      }      
    }
    
  }

  // Store of the exiting values.
  typedef LoopValue::ValueList VL;
  const VL& exitingValues = lv->getExitingValues();
  for (VL::const_iterator it = exitingValues.begin(), itEnd = exitingValues.end();
       it != itEnd; ++it) 
  {
    Value* exitingVal = *it;
    
    // If we already instrumented it, don't do it twice.
    if (itValues.find(exitingVal) != itValues.end()) {
      continue;
    }

    BBPosList posList = findStorePos(exitingVal);
    
    for (BBPosList::iterator posIt = posList.begin(), posEndIt = posList.end();
	 posIt != posEndIt; ++posIt)
    {
      ValueInstr* vip = new ValueInstr(InstrStore, exitingVal, *posIt, index);
      ValueInstrs.push_back(vip);
    }
  }

}    


void YarnLoop::print (raw_ostream &OS) const {
  const std::string LVL = "\t";
  const std::string LVL2 = "\t\t";
  const std::string LVL3 = "\t\t\t";

  OS << LVL << "YarnLoop(" << F->getName() << "):\n";
  OS << LVL
     << "dep=" << Dependencies.size() << ", "
     << "ptr=" << Pointers.size() << ", "
     << "inv=" << Invariants.size() << ", "
     << "pin=" << PointerInstrs.size() << ", "
     << "vin=" << ValueInstrs.size() << ", "
     << "aes=" << ArrayEntries.size() << "\n";


  for (unsigned i = 0; i < Dependencies.size(); ++i)
    Dependencies[i]->print(OS);
  for (unsigned i = 0; i < Pointers.size(); ++i)
    Pointers[i]->print(OS);

  OS << LVL2 << "Invariants:\n";
  for (InvariantList::iterator it = Invariants.begin(), itEnd = Invariants.end();
       it != itEnd; ++it)
    PRINT_VAL(OS, LVL3, *it);

  for (unsigned i = 0; i < PointerInstrs.size(); ++i) 
    PointerInstrs[i]->print(OS);
  for (unsigned i = 0; i < ValueInstrs.size(); ++i) 
    ValueInstrs[i]->print(OS);
  for (unsigned i = 0; i < ArrayEntries.size(); ++i) 
    ArrayEntries[i]->print(OS);

}



//===----------------------------------------------------------------------===//
/// YarnLoopInfo

char YarnLoopInfo::ID = 0;


void YarnLoopInfo::getAnalysisUsage (AnalysisUsage &AU) const {
  AU.setPreservesAll();

  /* LLVM Won't schedul these two for whatever reason...
     if we try to do it from the Instrumentation pass instead it'll schedul them
     after this pass which is not any better.
     Current work around is to do it from the command line instead...

  AU.addRequiredID(LCSSAID);
  AU.addRequiredID(LoopSimplifyID);
  */

  AU.addRequired<LoopInfo>();
  AU.addRequired<PostDominatorTree>();
  AU.addRequired<DominatorTree>();
  AU.addRequiredTransitive<AliasAnalysis>();

}

bool YarnLoopInfo::runOnFunction (Function& F) {

  LI = &getAnalysis<LoopInfo>();
  AA = &getAnalysis<AliasAnalysis>();
  DominatorTree* dt = &getAnalysis<DominatorTree>();
  PostDominatorTree* pdt = &getAnalysis<PostDominatorTree>();
    
  for (LoopInfo::iterator it = LI->begin(), itEnd = LI->end(); 
       it != itEnd; ++it) 
  {

    Loop* loop = *it;
    if (!checkLoop(loop, dt)) {
      continue;
    }

    YarnLoop* yLoop = new YarnLoop(&F, loop, LI, AA, dt, pdt);
    if (keepLoop(yLoop)) {
      Loops.push_back(yLoop);
      
      LoopsKept++;
      LoopValues += yLoop->getDependencies().size();
      LoopPointers += yLoop->getPointers().size();
      LoopInvariants += yLoop->getInvariants().size();
       
    }
    else {
      delete yLoop;
    }

  }

  return false;
}


/// Because we can't currently instrument functions we're not working on,
/// ignore any loop with non-trivial function calls.
bool YarnLoopInfo::checkLoop (Loop* L, DominatorTree* DT) {
  assert(L->isLoopSimplifyForm());
  assert(L->isLCSSAForm(*DT));

  for (Loop::block_iterator bbIt = L->block_begin(), bbEndIt = L->block_end(); 
       bbIt != bbEndIt; ++bbIt) 
  {
    for (BasicBlock::iterator it = (*bbIt)->begin(), itEnd = (*bbIt)->end(); 
	 it != itEnd; ++it) 
    {
      if (CallInst* ci = dyn_cast<CallInst>(&(*it))) {
	if (!AA->doesNotAccessMemory(ci)) {
	  return false;
	}
      }

    }
  }

  // \todo Type checking. Probably has to be done after the analysis though.
  return true;
}


/// Heuristic to determine whether it's worth instrumenting the loop.    
/// \todo Could implement a way better heuristic
///       - Check for inner loops.
bool YarnLoopInfo::keepLoop (YarnLoop* YL) {
  int depCount = 0;
  depCount += YL->getPointerInstrs().size();
  depCount += YL->getValueInstrs().size();

  int loopSize = 0;
  Loop* l = YL->getLoop();
  for (Loop::block_iterator bbIt = l->block_begin(), bbEndIt = l->block_end(); 
       bbIt != bbEndIt; ++bbIt) 
  {
    loopSize += (*bbIt)->size();
  }

  double ratio = depCount / loopSize;
  return ratio < 0.25;
}


void YarnLoopInfo::releaseMemory () {
  VectorUtil<YarnLoop>::free(Loops);
}


void YarnLoopInfo::print (raw_ostream &O, const Module *M) const {
  O << "YarnLoopInfo (" << Loops.size() << "):\n";
  for (size_t i = 0; i < Loops.size(); ++i) {
    Loops[i]->print(O);
  }
}



//===----------------------------------------------------------------------===//
/// Pass Registration

INITIALIZE_PASS(YarnLoopInfo, "yarn-loopinfo",
                "Yarn loop analysis.",
                true, true);
