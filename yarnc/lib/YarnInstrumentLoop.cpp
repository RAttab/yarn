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
// Note that we split the instrumentation functions into multiple class so that
// the whole thing cleans itself up after each call to runOnModule.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "yarnloop"
#include <llvm/Yarn/YarnLoopInfo.h>
#include <llvm/Yarn/YarnCommon.h>
#include <llvm/Pass.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Value.h>
#include <llvm/User.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/Statistic.h>
#include <algorithm>
using namespace llvm;
using namespace yarn;

STATISTIC(YarnLoopCounter, "Counts number of loops Instrumented.");

namespace {

//===----------------------------------------------------------------------===//
/// InstrumentModuleUtil Decl

  class InstrumentModuleUtil : public Noncopyable {

    Module* M;

    typedef std::map<YarnLoop*, ArrayType*> LoopTypeMap;
    LoopTypeMap LoopTypes;

    bool DeclarationsInserted;

    IntegerType* YarnWordTy;
    PointerType* VoidPtrTy

    Function* YarnExecutorFct;
    Function* YarnExecSimpleFct;
    Function* YarnDepLoadFct;
    Function* YarnDepLoadFastFct;
    Function* YarnDepStoreFct;
    Function* YarnDepStoreFastFct;

    unsigned ValCounter;

  public :
    
    InstrumentModuleUtil (Module* m) : 
      M(m), LoopTypes(), DeclarationsInserted(false),
      YarnWordTy(NULL), VoidPtrTy(NULL),
      YarnExecutorFct(NULL), YarnExecSimpleFct(NULL),
      YarnDepLoadFct(NULL), YarnDepLoadFastFct(NULL), 
      YarnDepStoreFct(NULL), YarnDepStoreFastFct(NULL),
      ValCounter(0)
    {}

    // We do nothing here because nothings needs to be cleaned up.
    ~InstrumentModuleUtil () {}


    inline Module* getModule () { return M; }

    inline const IntegerType* getYarnWordType () const { 
      return YarnWordTy; 
    }
    inline const PointerType* getVoidPtrType () const {
      return VoidPtrTy;
    }
    inline const Function* getYarnExecSimpleFct () const { 
      return YarnExecSimpleFct; 
    }
    inline const Function* YarnDepLoadFct () const { 
      return YarnDepLoadFct; 
    }
    inline const Function* YarnDepLoadFastFct () const { 
      return YarnDepLoadFastFct; 
    }
    inline const Function* getYarnDepStoreFct () const { 
      return YarnDepStoreFct; 
    }
    inline const Function* getYarnDepStoreFastFct () const { 
      return YarnDepStoreFastFct; 
    }

    std::string makeName(char prefix);    
    void createDeclarations();
    ArrayType* createLoopArrayType (YarnLoop* yl, unsigned valueCount);

    inline ArrayType* getLoopArrayType (YarnLoop* yl) const {
      return LoopTypes[f];
    }

    void print (llvm::raw_ostream &O) const;

  };


//===----------------------------------------------------------------------===//
/// LoopInstrumentation Decl

  class InstrumentLoopUtil : public NonCopyable {

    InstrumentModuleUtil* IMU;
    YarnLoop* YL;

    Function* SrcFct; // Original function to instrument
    Function* TmpFct; // Copy of SrcFct used for instrumentation
    Function* NewFct; // Final instrumented function added to the module.

    ValueMap TmpVMap;
    ValueMap MewVMap;
    
  public:
   
    InstrumentLoopUtil(InstrumentModuleUtil* imu, Function* f, YarnLoop* yl) :
      IMU(imu), YL(yl),
      SrcFct(f), TmpFct(NULL), NewFct(NULL)
      TmpVMap(), NewVMap()
    {}
    

    void instrumentLoop ();
    void print (llvm::raw_ostream &O) const;

  private:

    void instrumentTmpHeader();
    void instrumentTmpBody();
    void cleanupTmp();    

    void createNewFct();

    void instrumentSrcHeader();
    void instrumentSrcFooter();
    
  };



//===----------------------------------------------------------------------===//
/// InstrumentLoopUtil Decl


  class YarnInstrumentLoop : public ModulePass, public Noncopyable {
  public:

    static char ID; // Pass identification, replacement for typeid

    YarnInstrumentLoop() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual void print (llvm::raw_ostream &O, const llvm::Module *M) const;

  };    
    

}; // anynmous namespace


//===----------------------------------------------------------------------===//
/// InstrumentModuleUtil Impl
void InstrumentModuleUtil::createDeclarations () {
  if (DeclarationsInserted) {
    return;
  }

  YarnWordTy = IntergerType::get(M->getContext(), YarnWordBitSize);
  M->addTypeName("yarn_word_t", YarnWordTy);

  VoidPtrTy = PointerType::getUnqual(Type::Int8Ty);
  M->addTypeName("void_ptr_t", VoidPtrTy);

  {
    std::vector<Type*> args;
    args.push_back(YarnWordTy); // yarn_word_t pool_id
    args.push_back(VoidPtrTy);  // void* data
    YarnExecutorFct = FunctionType::get(YarnWordTy, args); // enum yarn_ret
    M->addTypeName("yarn_executor_t", YarnExecutorFct);
  }

  {
    std::vector<Type*> args;
    args.push_back(YarnExecutorFct); //yarn_executor_t executor
    args.push_back(VoidPtrTy); // void* data
    args.push_back(YarnWordTy); // thread_count
    args.push_back(YarnWordTy); // ws_size
    args.push_back(YarnWordTy); // index_size
    FunctionType* t = FunctionType::get(Type::Int1Ty, args); 

    YarnExecSimpleFct = M->getOrInsertFunction("yarn_exec_simple", t);
  }

  {
    std::vector<Type*> args;
    args.push_back(YarnWordTy); // yarn_word_t pool_id
    args.push_back(VoidPtrTy); // const void* src
    args.push_back(VoidPtrTy); // const void* dest
    FunctionType* t = FunctionType::get(Type::Int1ty, args);

    YarnDepLoadFct = M->getOrInsertFunction("yarn_dep_load", t);
    YarnDepStoreFct = M->getOrInsertFunction("yarn_dep_store", t);
  }

  {
    std::vector<Type*> args;
    args.push_back(YarnWordTy); // yarn_word_t pool_id
    args.push_back(YarnWordTy); // yarn_word_t index_id
    args.push_back(VoidPtrTy); // const void* src
    args.push_back(VoidPtrTy); // const void* dest
    FunctionType* t = FunctionType::get(Type::Int1ty, args);

    YarnDepLoadFastFct = M->getOrInsertFunction("yarn_dep_load_fast", t);
    YarnDepStoreFastFct = M->getOrInsertFunction("yarn_dep_store_fast", t);
  }

  DeclaratrionsInserted = true;
}

ArrayType* InstrumentModuleUtil::createLoopArrayType (YarnLoop* yl, unsigned valueCount) {
  ArrayType* t = ArrayType::get(YarnWordTy, valueCount);
  M->addTypeName(makeName('t'));
  LoopTypes[yl] = t;
  return t;
}

std::string InstrumentModuleUtil::makeName(char prefix); {
  std::string str = "y";
  str += prefix;
  str += ++ValCounter;
  return str;
}





//===----------------------------------------------------------------------===//
/// InstrumentLoopUtil Impl


void InstrumentLoopUtil::instrumentLoop() {
  // Add the struct type required to store the deps and invariants.
  imu->createStructType(YL, YL->getEntryValues().size());

  CodeInfo codeInfo;
  TmpFct = CloneFunction(SrcFct, TmpVMap, false, &codeInfo);

  instrumentTmpHeader();
  instrumentTmpBody();
  cleanupTmp();

  createNewFct();

  instrumentSrcFooter();
  instrumentSrcHeader();

}

void InstrumentLoopUtil::createTmpHeader () {
  
  BasicBlock* oldPredBB = TmpVMap[YL->getLoop()->getPredecessor()];
  // oldPredBB will be trashed.

  // Create a new BB 
  // Create placeholder value for the pool_id and the *task arguments
  // %ys = bitcast getStructType() task

  // foreach valueDep
  // %yvi = getelementptr YarnWord i
  
  // foreach ptrDep & invariant
  // %i = getelementptr i8* i
  // %ypi = load %i

  // %yb1 = alloca YarnWord

  // Probably should keep a map of the pointers.
}

void InstrumentLoopUtil::instrumentTmpBody() {
  using YarnLoop::PtrInsertList;  
  const PtrInsertList& ptrInstr = YI->getPtrInstrPoints();
  for (PtrInsertList::iterator it = ptrInstr.begin(), itEnd = ptrIntr.end();
       it != itEnd; ++it)
  {
    PointerInsertPoint* ptrPoint = *it;
    
    if (Type == InstrLoad) {
      // \todo
    }

    else if (Type == InstrStore) {
      // \todo
    }

    else {
      assert(false && "Sanity check.");
    }    
  }


  using YarnLoop::ValueInsertList;  
  const ValueInsertList& valueInstr = YI->getValueInstrPoints();
  for (ValueInsertList::iterator it = valueInstr.begin(), itEnd = valueIntr.end();
       it != itEnd; ++it)
  {
    PointerInsertPoint* valuePoint = *it;
    
    if (Type == InstrLoad) {
      // \todo

    }

    else if (Type == InstrStore) {
      // \todo
    }

    else {
      assert(false && "Sanity check.");
    }    
  }  

  using YarnLoop::InvariantList;
  const InvariantList& invariants = YI->getInvariants();
  for (InvariantList::iterator it = invariants.begin(), itEnd = invariants.end();
       it != itEnd; ++it)
  {
    Value* inva = TmpVMap[*it];
    
    // \todo replace its user list by the newly created var.
  }
}
void InstrumentLoopUtil::cleanupTmp() {

  // replace the backedge by a ret continue.
  // replace all forward out-of-loop edges by a ret break

  // remove any pred BBs
  // remove any succ BBs

}


void InstrumentLoopUtil::createNewFct() {
  // Create the function
  // Clone tmp into the new function
  // replace the placeholders by the arguments.
}


void InstrumentLoopUtil::instrumentSrcHeader() {
  // create new header BB = %yh1
  // %ys1 = alloca %yt1

  // foreach values in the struct
  //  %ypi = getelementptr %ys1 i
  //  store %1 %ypi

  // %yr1 = call yarn_exec_simple(%ys1, %yf1)

  // foreach values in the struct
  //  %yvi = load %ypi
  //  => replace %i users by %yvi

  // %yc1 = cmp %yr1 == error
  // br %yc1 %loop, yfooter  

}
void InstrumentLoopUtil::instrumentSrcFooter() {
  // create new footer BB => %yf1

  // for each value in the struct
  // %yn1 = phi %yh1 %yv1, %latch %i (exitValue)
  // => replace %i by %yn1
    
}




//===----------------------------------------------------------------------===//
/// YarnInstrumentLoop Impl

char YarnInstrumentLoop::ID = 0;

bool YarnInstrumentLoop::runOnModule(Module &M) {
  bool didSomething = false;

  InstrumentModuleUtil imu = InstrumentModuleUtil(M);

  for (Module::iterator it = M.begin(), itEnd = M.end(); it != itEnd; ++it) {
    Function* func = *it;
    YarnLoopInfo* yli = getAnalysis<YarnLoopInfo>(func);
    
    for (YarnLoopInfo::iteratore loopIt = yli->begin(), loopItEnd = yli->end();
	 loopIt != loopItEnd; ++loopIt)
    {
      YarnLoop* loop = *loopIt;

      ymu.insertDeclarations();

      InstrumentLoopUtil ilu = InstrumentLoopUtil(&ymu, f, loop);
      ilu.instrumentLoop();

      didSomething = true;
    }
  }

  return didSomething;
}


void YarnInstrumentLoop::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<YarnLoopInfo>();
}


void print (llvm::raw_ostream &O, const llvm::Module *M) const {

}



//===----------------------------------------------------------------------===//
/// Pass Registration

INITIALIZE_PASS(YarnInstrumentLoop, "yarn-loop",
                "Yarn loop instrumentation",
                false, false);
