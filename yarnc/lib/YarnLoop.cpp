//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Yarnc loop instrumentation pass. Part of the yarn project which can be found
// somewhere else.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "yarnloop"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

STATISTIC(YarnLoopCounter, "Counts number of loops Instrumented.");


namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct YarnLoop : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    Yarnc() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      ++YarnLoopCounter;
      errs() << "Yarn Module -- WEE";
      return false;
    }

    // We don't modify the program, so we preserve all analyses
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
    }
  };
}

char Yarnc::ID = 0;
INITIALIZE_PASS(Yarnc, "yarn-loop",
                "Yarn loop instrumentation",
                false, false);
