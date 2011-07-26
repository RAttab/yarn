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


//===----------------------------------------------------------------------===//
/// LoopPointer

void LoopPointer::print (raw_ostream &OS) const {
}


//===----------------------------------------------------------------------===//
/// LoopValue

void LoopValue::print (raw_ostream &OS) const {
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
  PointerInstrs(), ValueInstrs(), EntryValues()
{
  processLoop();
}

YarnLoop::~YarnLoop() {
  VectorUtil<LoopValue>::free(Dependencies);
  VectorUtil<LoopPointer>::free(Pointers);
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
    

  processValueInstrs();
  processPointerInstrs();

  processEntryValues();

}

// The SimplifyLoop pass ensures that we have only one back-edge in the loop and only
// one incomming edge into the loop. This means that the header of the loop will contain
// PHI nodes with all the dependencies used before the loop. We can also use the PHI
// nodes to get the exiting values which can later be used by the processFooterPHINode.
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
      lv->ExitingValue = incValue;
    }
  }

  assert((lv->EntryValue != NULL && lv->ExitingValue != NULL) &&
	 "SimplifyLoop should ensure only one exiting and one entry value.");

  Dependencies.push_back(lv);
  ExitingValueMap[lv->ExitingValue] = lv;

}

// The LCSSA and the SimplifyLoop pass ensures that we have a unique footer for the loop
// which contains PHI nodes of dependencies that are used after the loop.
// This functions looks at these and, if possible, matches them to the values found in
// processHeaderPHINode.
void YarnLoop::processFooterPHINode(PHINode* PHI, ValueMap& ExitingValueMap) {

  LoopValue* lv;
  
  int bbIndex = PHI->getBasicBlockIndex(L->getLoopLatch());
  if (bbIndex < 0) {
    return;
  }
  Value* incValue = PHI->getIncomingValue(bbIndex);
    
  // Do we already know the value from the header?
  ValueMap::iterator it = ExitingValueMap.find(incValue);
  if (it != ExitingValueMap.end()) {
    lv = it->second;
  }
  
  // If we didn't match anything from the header then 
  // this is a new exit-only value.
  if (!lv) {
    lv = new LoopValue();
    Dependencies.push_back(lv);
  }

  assert(lv->FooterNode == NULL &&
	 "SimplifyLoop should ensure only one exit value.");
  lv->FooterNode = PHI;  
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
      Invariants.push_back(*it);
    }
    
  }  

}



// An invariant means that 
void YarnLoop::processInvariants (Instruction* Inst) {

  for (unsigned int i = 0, iEnd = Inst->getNumOperands(); i < iEnd; ++i) {
    Value* operand = Inst->getOperand(i);
    if (L->isLoopInvariant(operand)) {
      Invariants.push_back(operand);
    }
  }

}


void YarnLoop::processEntryValues () {
  for (ValueList::iterator it = Dependencies.begin(), itEnd = Dependencies.end();
       it != itEnd; ++it)
  {
    EntryValues.push_back(std::make_pair((*it)->getEntryValue(), false));
  }

  for (PointerList::iterator it = Pointers.begin(), itEnd = Pointers.end(); 
       it != itEnd; ++it)
  {
    typedef LoopPointer::AliasList AliasList;
    AliasList& aliases = (*it)->getAliasList();
    for (AliasList::iterator aliasIt = aliases.begin(), aliasEndIt = aliases.end();
	 aliasIt != aliasEndIt; ++aliasIt)
    {
      Value* alias = *aliasIt;
      bool outsideLoop = false;
      if (isa<Argument>(alias)) {
	outsideLoop = true;
      }
      else if(Instruction* inst = dyn_cast<Instruction>(alias)) {
	outsideLoop = L->contains(inst->getParent());
      }
      if (outsideLoop) {
	EntryValues.push_back(std::make_pair(alias, true));
      }
    }
  }

  for (InvariantList::iterator it = Invariants.begin(), itEnd = Invariants.end();
       it != itEnd; ++it) 
  {
    EntryValues.push_back(std::make_pair(*it, true));
  }
  
}



namespace {
  
  class BBInstPredicate : public std::unary_function<bool, Instruction&> {

    Instruction* ToFind;

  public :

    BBInstPredicate (Instruction* toFind) : ToFind(toFind) {}
    bool operator() (const Instruction& inst) {
      return &inst == ToFind;
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
  for (Value::use_iterator it = V->use_begin(), endIt = V->use_end();
       it != endIt; ++it)
  {
    Instruction* inst = dyn_cast<Instruction>(*it);
    assert(inst && "Sanity check.");
    if (!isInLoop(L, inst)) {
      continue;
    }

    BBInstPredicate pred(inst);
    BasicBlock::iterator findIt = std::find_if(loadBB->begin(), loadBB->end(), pred);
    if (findIt != loadBB->end()) {
      posList.push_back(&(*findIt));
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

  // If the common dominator contains a load instruction, 
  //    return the pos right after it.
  // otherwise return the first valid position in the BB.
  BBPosList posList;
  for (InstructionList::iterator it = storeList.begin(), endIt = storeList.end();
       it != endIt; ++it)
  {
    BBInstPredicate pred(*it);
    BasicBlock::iterator findIt = std::find_if(storeBB->begin(), storeBB->end(), pred);
    if (findIt != storeBB->end()) {
      posList.push_back(&(*findIt));
      return posList;
    }
    
  }  

  posList.push_back(storeBB);
  return posList;

}


// Ugliest for loops EVAR...
void YarnLoop::processPointerInstrs () {
  typedef LoopPointer::AliasList AliasList;

  for (PointerList::iterator it = Pointers.begin(), itEnd = Pointers.end();
       it != itEnd; ++it)
  {

    AliasList& aliasList = (*it)->getAliasList();
    for (AliasList::iterator aliasIt = aliasList.begin(), 
	   aliasEndIt = aliasList.end();
	 aliasIt != aliasEndIt; ++aliasIt)
    {

      Value* alias = *aliasIt;
      for (Value::use_iterator useIt =  alias->use_begin(), 
	     useEndIt = alias->use_end();
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
  } // pointer interation

}

void YarnLoop::processValueInstrs () {

  unsigned index = 0;

  for (ValueList::iterator it = Dependencies.begin(), itEnd = Dependencies.end();
       it != itEnd; ++it)
  {
    // Process the loads.
    {
      Value* loadVal = (*it)->getEntryValue();
      BBPosList posList = findLoadPos(loadVal);
      assert(posList.size() == 1 &&
	     "We don't yet support multiple load points"
	     " and there should be at least one load point.");

      for (BBPosList::iterator posIt = posList.begin(), posEndIt = posList.end();
	   posIt != posEndIt; ++posIt)
      {
	ValueInstr* vip = new ValueInstr(InstrLoad, loadVal, *posIt, index);
	ValueInstrs.push_back(vip);
      }
    }

    // Process the stores.
    {
      Value* writeVal = (*it)->getExitingValue();
      BBPosList posList = findStorePos(writeVal);
      assert(posList.size() > 0 &&
	     "There should be at least one store point.");

      for (BBPosList::iterator posIt = posList.begin(), posEndIt = posList.end();
	   posIt != posEndIt; ++posIt)
      {
	ValueInstr* vip = new ValueInstr(InstrStore, writeVal, *posIt, index);
	ValueInstrs.push_back(vip);
      }
    }

    index++;

  }
}    


void YarnLoop::print (raw_ostream &OS) const {

}



//===----------------------------------------------------------------------===//
/// YarnLoopInfo

char YarnLoopInfo::ID = 0;


void YarnLoopInfo::getAnalysisUsage (AnalysisUsage &AU) const {
  errs() << "START YarnLoopInfo::getAnalysisUsage\n";

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

  errs() << "END   YarnLoopInfo::getAnalysisUsage\n";

}

bool YarnLoopInfo::runOnFunction (Function& F) {

  errs() << "START YarnLoopInfo::runOnFunction(F=" << F.getName() <<")\n";


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

  errs() << "END   YarnLoopInfo::runOnFunction(F=" << F.getName() <<")\n";

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
}



//===----------------------------------------------------------------------===//
/// Pass Registration

INITIALIZE_PASS(YarnLoopInfo, "yarn-loopinfo",
                "Yarn loop analysis.",
                true, true);
