//===- YarnUtil.h - Example code from "Writing an LLVM Pass" --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the FreeBSD License.
// See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Utility header for the lib folder.
//
//===----------------------------------------------------------------------===//

#ifndef YARN_UTIL_H
#define YARN_UTIL_H

#include <llvm/Instruction.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>
#include <stack>

namespace yarn {

//===----------------------------------------------------------------------===//
/// Utility class for std::vector
///
  template<typename T>
  struct VectorUtil {
    static void free (std::vector<T*>& v) {
      while(!v.empty()) {
	T* ptr = v.back();
	v.pop_back();
	delete ptr;
      }
    }
  };


//===----------------------------------------------------------------------===//
/// Random loop utility
///

  inline bool isInLoop (llvm::Loop* l, llvm::Value* v) {
    if (llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(v)) {
      return l->contains(inst);
    }
    else if (llvm::BasicBlock* bb = llvm::dyn_cast<llvm::BasicBlock>(v)) {
      return l->contains(bb);
    }
    return false;
  }


  // un-inline this....
  inline void replaceUsesInBlock(llvm::BasicBlock* bb, 
				 llvm::Value* from, 
				 llvm::Value* to) 
  {
    if (!from->isUsedInBasicBlock(bb)) {
      return;
    }


    bool removedSomething;
    do {
      removedSomething = false;

      for (llvm::Value::use_iterator it = from->use_begin(), itEnd = from->use_end();
	   it != itEnd; ++it)
      {

	llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(*it);
	if (!inst) {
	  continue;
	}

	if (inst->getParent() == bb) {
	  inst->replaceUsesOfWith(from, to);

	  // Since we probably end up invalidating the iterator,
	  // we gotta start from scratch.
	  // It's ugly and slow but Value doesn't provide a better interface.
	  removedSomething = true;
	  break;
	}

      }

    } while (removedSomething);
  }

  inline void replaceUsesInScope(llvm::Function::iterator bb,
				 const llvm::Function::iterator& bbEnd,
				 llvm::Value* from,
				 llvm::Value* to)
  {
    for (; bb != bbEnd; ++bb) {
      replaceUsesInBlock(&(*bb), from, to);
    }
  }

  inline void eraseInstructions(llvm::BasicBlock* BB) {
    llvm::BasicBlock::InstListType& instList = BB->getInstList();
    
    while (!instList.empty()) {
      instList.back().eraseFromParent();
    }
  }

  inline void pruneFunction(llvm::Function* F, 
			    llvm::BasicBlock* keepStart,
			    llvm::BasicBlock* keepEnd) 
  {
    typedef llvm::Function::iterator iterator;
    llvm::Function::BasicBlockListType& bbList = F->getBasicBlockList();

    // Remove all the instructions in reverse to safely clear all use dependencies.
    for (iterator it = bbList.end(), itEnd (keepEnd); it != itEnd;) {
      --it;
      eraseInstructions(&(*it));
    }
    for (iterator it (keepStart), itEnd = bbList.begin(); it != itEnd;) {
      --it;
      eraseInstructions(&(*it));
    }

    // Delete the basic blocks working forward (all use dependencies were cleared up).
    llvm::BasicBlock* bb;
    while ((bb = &bbList.front()) != keepStart) {
      bb->eraseFromParent();
    }
    while ((bb = &bbList.back()) != keepEnd) {
      bb->eraseFromParent();
    }
    keepEnd->eraseFromParent();
  }

} // namespace yarn

#endif // YARN_COMMON_H
