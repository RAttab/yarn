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
#include "llvm/Yarn/YarnLoopInfo.h"
#include <llvm/Value.h>
#include <llvm/User.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/Statistic.h>
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
  OS << "Pointer={" << 
    "name=" << Pointer->getName() << 
    "flag=" << (IsRead ? 'r' : '') << (IsWrite ? 'w' : '') <<
    "}";
}


//===----------------------------------------------------------------------===//
/// LoopValue

void LoopValue::print (raw_ostream &OS) const {
  OS << "Value={" << 
    "name=" << Node->getName() << 
    "entry=" << EntryValue->getName() <<
    "exit=" << ExitValue->getName() <<
    "}";
}



//===----------------------------------------------------------------------===//
/// YarnLoop

YarnLoop::YarnLoop(Loop* l, 
	 LoopInfo* li,
	 AliasAnalysis* aa, 
	 DominatorTree* dt,
	 PostDominatorTree* pdt) 
:
  LI(li), AA(aa), DT(dt), PDT(pdt),
  L(l), Dependencies(), Pointers(), Invariants()
{
  processLoop();
}

YarnLoop::~YarnLoop() {
  VectorUtil<LoopValue>::free(Dependencies);
  VectorUtil<LoopPointer>::free(Pointers);
}


void YarnLoop::processLoop () {

  for (Loop::iterator bb = L->block_begin(), bbEnd = L->block_end(); bb != bbEnd; ++bb) {
    bool isHeader = *i == L->getHeader();

    for (BasicBlock::iterator i = (*bb)->begin(), iEnd = (*bb)->end(); i != iEnd; ++i) {

      if (isHeader && PhiNode phi = dyn_cast<PHINode>(*i)) {
	processHeaderPHINode(phi);
      }
      else if(StoreInst* si = dyn_cast<StoreInst>(*i)) {
	processStore(si);
      }
      else if (LoadInst* li = dyn_cast<LoadInst>(*i)) {
	processLoad(li);
      }
      else if (User* u = dyn_cast<User>(*i)){
	processInvariants(u);
      }
      

    }
  }


  const BBList& exitBlocks = L->getExitBlocks();
  if (exitBlocks.size() == 1) {
    const BasicBlock* bb = exitBlocks.front();
    for (BasicBlock::iterator i = bb->begin(), iEnd = bb->end(); i != iEnd; ++i) {
      
      if (PHINode* phi = dyn_cast<PHINode>(*i)) {
	processFooterPHINode(phi);
      }
    }
    
  }
  else {
    assert(exitBlocks.size() > 1 && 
	   "Simplify Loop should ensure only a single exit blocks.");
  }
}

// The SimplifyLoop pass ensures that we have only one back-edge in the loop and only
// one incomming edge into the loop. This means that the header of the loop will contain
// PHI nodes with all the dependencies used before the loop. We can also use the PHI
// nodes to get the exiting values which can later be used by the processFooterPHINode.
void YarnLoop::processHeaderPHINode (PHINode* PHI) {
  BasicBlock* loopPred = L->getPredecessor();

  LoopValue* lv = new LoopValue();
  lv->HeaderNode = PHI;

  assert(lv->getNumIncomingValues == 2 &&
	 "SimplifyLoop should ensure only one exiting and one entry value.");

  Value* exitingValue;

  for (int i = 0; i < PHI->getNumIncomingValues(); ++i) {
    BasicBlock* incBB = PHI->getIncomingBasicBlock(i);
    Value* incValue = PHI->getIncommingValue(i);

    if (loopPred == incBB) {
      lv->EntryValue = incValue;
    }
    else {
      exitingValue = incValue;
    }
  }

  assert((lv->EntryValue != NULL && exitingValue != NULL) &&
	 "SimplifyLoop should ensure only one exiting and one entry value.");

  Dependencies.push_back(lv);
  DependenciesMap[exitingValue] = lv;

}

// The LCSSA and the SimplifyLoop pass ensures that we have a unique footer for the loop
// which contains PHI nodes of dependencies that are used after the loop.
// This functions looks at these and, if possible, matches them to the values found in
// processHeaderPHINode.
void YarnLoop::processFooterPHINode(PHINode* PHI) {

  LoopValue* lv;

  for (int i = 0; i < PHI->getNumIncomingValues(); ++i) {
    BasicBlock* incBB = PHI->getIncomingBasicBlock(i);
    Value* incValue = PHI->getIncommingValue(i);

    // Do we already know the value from the header?
    ValueMap::iterator it = ExitingValueMap.find(incValue);
    if (it != ExitingValueMap.end()) {
      lv = it->second;
      break;
    }

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


void YarnLoop::processPointer (Instruction* I) {

}

void YarnLoop::processInvariants (User* U) {

}


BBPos YarnLoop::findLoadPos (const Value* value) const {

}

BBPosList YarnLoop::findStorePos (const Value* value) const {

}

void YarnLoop::print (raw_ostream &OS) const {

}


//===----------------------------------------------------------------------===//
/// LoopDepVisitor




//===----------------------------------------------------------------------===//
/// YarnLoopInfo

void YarnLoopInfo::getAnalysisUsage (AnalysisUsage &AU) const {
  AU.setPreservesAll();
    
  AU.addRequiredID(LCSSAID);
  AU.addRequiredID(LoopSimplifyID);

  AU.addRequired<LoopInfo>();
  AU.addRequired<PostDominatorTree>();
  AU.addRequired<DominatorTree>();
  AU.addRequiredTransitive<AliasAnalysis>();
}

bool YarnLoopInfo::runOnFunction (Function& F) {

  LI = getAnalysis<LoopInfo>();
  AA = &getAnalysis<AliasAnalysis>();
  DominatorTree* dt = &getAnalysis<DominatorTree>();
  PostDominatorTree* pdt = &getAnalysis<PostDominatorTree>();
    
  for (LoopInfo::iterator i = LI.begin(), end = LI.end(); i != end; ++i) {

    Loop* loop = &(*i);
    if (!checkLoop(loop)) {
      continue;
    }

    YarnLoop* yLoop = new YarnLoop(loop, LI, AA, dt, pdt);
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

}


/// Because we can't currently instrument functions we're not working on,
/// ignore any loop with non-trivial function calls.
/// \todo We could easily support functions that only modify it's arguments.
bool YarnLoopInfo::checkLoop (Loop* L) {
  for (Loop::iterator bb = L->block_begin(), bbEnd = L->block_end(); bb != bbEnd; ++bb) {
    for (BasicBlock::iterator i = (*bb)->begin(), iEnd = (*bb)->end(); i != iEnd; ++i) {

      if (CallInst* ci = dyn_cast<CallInst>(*i)) {
	if (!AA->doesNotAccessMemory(ci)) {
	  return false;
	}
      }

    }
  }

}


/// Heuristic to determine whether it's worth instrumenting the loop.    
/// \todo Could implement a way better heuristic
///       - Check for inner loops.
///       - Cound the number of load and stores (number of deps is inacurate for ptrs).
bool YarnLoopInfo::keepLoop (YarnLoop* YL) {
  int depCount = 0;
  depCount += YL->getDependencies();
  depCount += YL->getPointers();

  int loopSize = 0;
  Loop* l = YL->getLoop();
  for (Loop::iterator bb = l->block_begin(), bbEnd = l->block_end(); bb != bbEnd; ++bb) {
    loopSize += bb->size();
  }

  double ratio = depCount / loopSize;
  return ration < 0.25;
}


void YarnLoopInfo::releaseMemory () {
  VectorUtil<YarnLoop*>::free(Loops);
}


void YarnLoopInfo::print (raw_ostream &O, const Module *M) const {
  O << "YarnLoopInfo(" << M->getName() << "): " << 
    Loops.size() << " Loops found.\n";

  for (iterator i = Loops.begin(), end = Loops.end(); i != end; ++i) {
    (*i)->print(O);
  }

}



char Yarnc::ID = 0;
INITIALIZE_PASS(Yarnc, "yarn-loopinfo",
                "Yarn loop analysis.",
                false, false);
