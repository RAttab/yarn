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
#include <llvm/Yarn/YarnLoopInfo.h>
#include <llvm/Value.h>
#include <llvm/User.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Dominators.h>
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
  L(l), Dependencies(), Pointers(), Invariants(),
  PtrInstrPoints(), ValueInstrPoints(), EntryValues()
{
  processLoop();
}

YarnLoop::~YarnLoop() {
  VectorUtil<LoopValue>::free(Dependencies);
  VectorUtil<LoopPointer>::free(Pointers);
}


void YarnLoop::processLoop () {

  ValueMap exitingValueMap;
  PointerInstSet loadSet;
  PointerInstSet storeSet;

  for (Loop::iterator bb = L->block_begin(), bbEnd = L->block_end(); bb != bbEnd; ++bb) {
    bool isHeader = *i == L->getHeader();

    for (BasicBlock::iterator i = (*bb)->begin(), iEnd = (*bb)->end(); i != iEnd; ++i) {

      if (isHeader && PhiNode phi = dyn_cast<PHINode>(*i)) {
	processHeaderPHINode(phi, exitingValueMap);
      }

      else if(StoreInst* si = dyn_cast<StoreInst>(*i)) {
	Value* ptr = si->getPointerOperand();
	storeSet.insert(ptr);
	loadSet.erase(ptr);
      }

      else if (LoadInst* li = dyn_cast<LoadInst>(*i)) {
	Value* ptr = li->getPointerOperand();
	if (storeSet.find(ptr) == storeSet.end()) {
	  loadSet.insert(ptr);
	}
      }

      else if (Instruction* inst = dyn_cast<User>(*i)){
	processInvariants(inst);
      }      

    }
  }

  processPointers(loadSet, storeSet);

  const BBList& exitBlocks = L->getExitBlocks();
  if (exitBlocks.size() == 1) {
    const BasicBlock* bb = exitBlocks.front();
    for (BasicBlock::iterator i = bb->begin(), iEnd = bb->end(); i != iEnd; ++i) {
      
      if (PHINode* phi = dyn_cast<PHINode>(*i)) {
	processFooterPHINode(phi, exitingValueMap);
      }
    }
    
  }
  else {
    assert(exitBlocks.size() > 1 && 
	   "Simplify Loop should ensure only a single exit blocks.");
  }

  processValuePoints();
  processPtrPoints();

  processEntryValues();

}

// The SimplifyLoop pass ensures that we have only one back-edge in the loop and only
// one incomming edge into the loop. This means that the header of the loop will contain
// PHI nodes with all the dependencies used before the loop. We can also use the PHI
// nodes to get the exiting values which can later be used by the processFooterPHINode.
void YarnLoop::processHeaderPHINode (PHINode* PHI, ValueMap& ExitingValueMap) {
  BasicBlock* loopPred = L->getPredecessor();

  LoopValue* lv = new LoopValue();
  lv->HeaderNode = PHI;

  assert(lv->getNumIncomingValues == 2 &&
	 "SimplifyLoop should ensure only one exiting and one entry value.");

  Value* exitingValue;

  for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
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
void YarnLoop::processFooterPHINode(PHINode* PHI, ValueMap& ExitingValueMap) {

  LoopValue* lv;

  for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
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
      AliasResult ar = AA->alias((*it).first, Ptr);
      
      if (strict && ar == MustAlias) {
	return (*it).second;
      }
      else if (!strict && ar) {
	return (*it).second;
      }
    }

    return NULL;
  }

}; // Anonymous namespace


/// \todo This will pick up strictly local load and stores that may not alias with
///   another loop iteration. So a pointer that manipulates a piece of memory that was
///   allocated and freed during the same iteration, will be instrumented.
///   Not sure how to find and filter out these pointers.
void YarnLoop::processPointers (PointerInstSet& LoadSet, PointerInstSet& StoreSet) {

  PointerMap storeMap;

  for (PointerInstSet::iterator it = StoreSet.begin(), itEnd = StoreSet.end(); 
       it != iEnd; ++it)
  {
    LoopPointer* lp = isKnownAlias(AA, storeMap, *it, true);
    if (!lp) {
      lp = new LoopPointer();
      Pointers.push_back(*it);
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
      Pointers.push_back(*it);
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

  for (PointerList::iterator it = Pointers.begin(), itEnd = Points.end(); 
       it != itEnd; ++it)
  {
    using LoopPointer::AliasList;
    const AliasList& aliases = (*it)->getAliasList();

    for (AliasList::iterator aliasIt = aliases.begin(), aliasEndIt = aliases.end();
	 aliasIt != aliasEndIt; ++aliasIt)
    {
      Value* alias = *it;
      if (L->contains(alias->getParent())) {
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

}; // Anonymous namespace


/// \todo This is currently not optimal.
///   If the two earliest stores are done on two parallel branches then we should
///   return the two load instructions and not their dominator.
BBPosList YarnLoop::findLoadPos (const Value* Value) const {
  // Find the common dominator for all the load operations.
  BasicBlock* loadBB = NULL;
  for (Value::const_use_iterator it = Value->begin(), endIt = Value->end();
       it != endIt; ++it)
  {
    BasicBlock* bb = (*it)->getParent();
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
  BBPos pos = &(*loadBB->back());
  for (Value::const_use_iterator it = Value->begin(), endIt = Value->end();
       it != endIt; ++it)
  {
    BBInstPredicate pred(*it);
    BasicBlock::iterator findIt = std::find_if(loadBB->begin(), loadBB->end(), pred);
    if (findIt != loadBB->end()) {
      pos = &(*findIt);
      break;
    }
    
  }  

  BBPosList posList;
  posList.push_back(pos);
  return posList;
}

namespace {

  typedef Instruction* InstructionList;

  void findWriteInstructions (PHINode* PHI, InstructionList& OutList) {

    for (int i = 0; i < PHI->getNumIncomingValues(); ++i) {

      BasicBlock* incBB = PHI->getIncomingBasicBlock(i);
      Value* incValue = PHI->getIncommingValue(i);

      if (PHINode* node = dyn_cast<PHINode>(incValue)) {
	// Avoid the infinit loop.
	if (PHI->getParent() == L->getHeader()) {
	  continue;
	}
	findWriteInstructions(node, OutList);
      }

      else if (Instruction* inst = dyn_cast<Instruction>(incValue)) {
	OutList.push_back(inst);
      }

    }

  }

}; // Anonymous namespace

BBPosList YarnLoop::findStorePos (const Value* Value) const {
  Instructionlist storeList;

  // Gather the list of all the non-phi store instruction for the value.
  if (PHINode* phi = dyn_cast<PHINode>(value)) {
    findWriteInstructions(phi, storeList);
  }
  else if (Instruction* inst = dyn_cast<Instruction*>(Value)) {
    storeList.push_back(Value);
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
  BBPos pos = &(*storeBB->getFristNonPHI());
  for (InstructionList::iterator it = storeList.begin(), endIt = storeList.end();
       it != endIt; ++it)
  {
    BBInstPredicate pred(*it);
    BasicBlock::iterator findIt = std::find_if(storeBB->begin(), storeBB->end(), pred);
    if (findIt != storeBB->end()) {
      pos = &(*findIt);
      break;
    }
    
  }  

  BBPosList posList;
  posList.push_back(pos);
  return posList;

}


// Ugliest for loops EVAR...
void YarnLoop::processPtrPoints () {
  using LoopPointer::AliasList;

  for (PointerList::iterator it = Pointers.begin(), itEnd = Pointers.end();
       it != itEnd; ++it)
  {

    const AliasList& aliasList = (*it)->getAliasList();
    for (AliasList::const_iterator aliasIt = aliasList.begin(), 
	   aliasEndIt = aliasList.end();
	 aliasIt != aliasEndIt; ++aliasIt)
    {

      Value* alias = *aliasIt;
      for (Value::const_use_iterator useIt =  alias->use_begin(), 
	     useEndIt = alias->use_end();
	   useIt != useEndIt; ++useIt)
      {
	User* user = *useIt;
	// Make sure we're still in the loop.
	if (!L->contains(user->getParent())) {
	  continue;
	}

	PointerInsertPoint* pip = NULL;

	if (StoreInst* si = dyn_cast<StoreInst>(user)) {
	  pip = new PointerInsertPoint(InstrStore, si);
	}
	else if (LoadInst* li = dyn_cast<LoadInst>(user)) {
	  pip = new PointerInsertPoint(InstrLoad, li);
	}
	else {
	  continue;
	}
	
	PtrInstrPoints->push_back(pip);

      } // use iteration
    } // alias iteration
  } // pointer interation

}

void YarnLoop::processValuePoints () {

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
	ValueInsertPoint* vip = new ValueInsertPoint(InstrLoad, loadVal, *posIt);
	ValueInstrPoint.push_back(vip);
      }
    }

    // Process the stores.
    {
      Value* writeVal = (*it)->getExitValue();
      BBPosList posList = findWritePos(writeVal);
      assert(posList.size() > 0 &&
	     "There should be at least one store point.");

      for (BBPosList::iterator posIt = posList.begin(), posEndIt = posList.end();
	   posIt != posEndIt; ++posIt)
      {
	ValueInsertPoint* vip = new ValueInsertPoint(InstrWrite, writeVal, *posIt);
	ValueInstrPoint.push_back(vip);
      }
    }

  }
}    


void YarnLoop::print (raw_ostream &OS) const {

}



//===----------------------------------------------------------------------===//
/// YarnLoopInfo

char YarnLoopInfo::ID = 0;


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

  return false;
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
bool YarnLoopInfo::keepLoop (YarnLoop* YL) {
  int depCount = 0;
  depCount += YL->getPtrInstrPoints().size();
  depCount += YL->getValueInstrPoints().size();

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



//===----------------------------------------------------------------------===//
/// Pass Registration

INITIALIZE_PASS(YarnLoopInfo, "yarn-loopinfo",
                "Yarn loop analysis.",
                true, true);
