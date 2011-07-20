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
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/Statistic.h>
using namespace llvm;

STATISTIC(YarnLoopAnalyzed, "Counts number of loops Instrumented.");

namespace yarn {

  

};


char Yarnc::ID = 0;
INITIALIZE_PASS(Yarnc, "yarn-loopinfo",
                "Yarn loop analysis.",
                false, false);
