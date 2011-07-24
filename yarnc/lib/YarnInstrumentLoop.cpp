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
#include <llvm/Instruction.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/BasicBlockUtils.h>
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

    inline LLVMContext& getContext() { return M->getContext(); }

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

  cleanupTmpFct();
  createNewFct();

  instrumentSrcFooter();
  instrumentSrcHeader();

}

void InstrumentLoopUtil::createTmpFct () {
  // Clone the function that we will be working on.
  CodeInfo codeInfo;
  TmpFct = CloneFunction(SrcFct, TmpVMap, false, &codeInfo);

  // Create a new header BB before the loop. 
  // This will eventually be the first BB in the fct.
  BasicBlock* loopHeader = TmpVMap[YL->getLoop()->getHeader()];
  BasicBlock* bbHeader = 
    BasicBlock::Create(IMU->getContext(), IMU->makeName('h'), TmpFct, loopHeader);


  //
  // Setup the header.
  //

  // Create placeholder value for the pool_id and the *task arguments
  Value* poolIdVal = 
    new BitCastInst (ConstantInt::get(IMU->getYarnWordType(), 0),
		     IMU->makeName('p'), bbHeader);
  Value* taskPtr = 
    new BitCastInst (ConstantInt::get(IMU->getVoidPtrType(), 0),
		     IMU->makeName('p'), bbHeader);
  
  // struct task* s = (struct task*) task;
  Value* loopArrayPtr =
    new BitCastInst (taskPtr, 
		     PointerType::getUnqual(IMU->getLoopArrayTyp(YL)),
		     IMU->makeName('s'), bbHeader);

  // Keeps track of the ptr into the array for a given value.
  std::map<Value*, Value*> headerValMap;
  
  // Extract the structure for use within the function.
  using YarnLoop::EntryValueList;
  const EntryValueList& entryValues = YL->getEntryValues();

  for (unsigned i = 0; i < entryValues.size(); ++i) {

    Value* val = TmpVMap[entryValues[i].first];
    bool loadNow = entryValues[i].second;
    std::string name = IMU->makeName('p');

    std::vector<const Value*> indexes;
    indexes.push_back(ConstantInt::get(Type::Int32Ty, 0));
    indexes.push_back(ConstantInt::get(Type::Int32Ty, i));

    // Get the pointer to the array index.
    Value* ptr =
      GetElementPtrInst::Create(loopArrayPtr,
				indexes.begin(), indexes.end(),
				IMU->makeName('p'), bbHeader);
    
    if (loadNow) {
      Value* invariant = 
	new LoadInst(ptr, IMU->makeName('v'), bbHeader);

      // The value is an invariant so replace the old val with the loaded val.
      BasicBlock::iterator valIt = BasicBlock::iterator(val);
      ReplaceInstWithValue(val->getParent()->getInstList(),
			   valIt, invariant);
    }
    else {
      // The pointer will be used during instrumentation so remember it.
      headerValMap[val] = ptr;
    }
  }
  
  // Create a buffer that will be used to load and store the instrumented values.
  Value* bufferVal = 
    new AllocaInstr(IMU->getYarnWordType(), 0, IMU->makeName('b'), bbHeader);

  // Add the terminator instruction.
  BranchInst::Create(TmpVMap[YL->getLoop()->getHeader()], bbHeader);


  //
  // Instrument the Body.
  // \todo We should process the return value of the yarn_dep_xxx calls.
  //

  // Process the value accesses.
  using YarnLoop::ValueInsertList;  
  const ValueInsertList& valueInstr = YI->getValueInstrPoints();
  for (ValueInsertList::iterator it = valueInstr.begin(), itEnd = valueIntr.end();
       it != itEnd; ++it)
  {
    ValueInsertPoint* valuePoint = *it;
    
    if (Type == InstrLoad) {
      Value* oldVal = TmpVMap[valuePoint->getValue()];

      // Create the arguments for the yarn_dep call.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(ConstantInt::get(IMU->getYarnWordType, valuePoint->getIndex()));
      args.push_back(headerValMap[oldVal]); // src
      args.push_back(bufferVal); // dest

      Value* retVal;
      Instruction* newVal;

      if (valuePoint->getInstPos()) {
	Instruction* pos = TmpVMap[valuePoint->getInstPos()];

	// Call yarn_dep_load to load the desired value into the buffer.
	// Place before the target instruction.
	retVal = CallInst::Create(IMU->getYarnDepLoadFastFct(), args, 
			 IMU->makeName('r'), pos);

	// Load the buffer into a new value.
	// Place before the target instruction.
	newVal = new LoadInst(bufferVal, IMU->makeName('l'), pos);		
      }

      else {
	BasicBlock* pos = TmpVMap[ValuePoint->getBBPos()];
	assert (pos && "Either getInstPos or getBBPos should be non-null.");
	
	// Call yarn_dep_load to load the desired value into the buffer. 
	//Append at the end of the BB.
	retVal = CallInst::Create(IMU->getYarnDepLoadFastFct(), args, 
			 IMU->makeName('r'), pos);

	// Load the buffer into a new value.
	//Append at the end of the BB.
	newVal = new LoadInst(bufferVal, IMU->makeName('l'), pos);	
      }

      // Replace the old value with the loaded value 
      BasicBlock::iterator valIt = BasicBlock::iterator(oldVal);
      ReplaceInstWithValue(oldVal->getParent()->getInstList(), valIt, newVal);

    }

    else if (Type == InstrStore) {
      Value* oldVal = TmpVMap[valuePoint->getValue()];

      // Create the arguments for the yarn_dep call.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(ConstantInt::get(IMU->getYarnWordType, valuePoint->getIndex()));
      args.push_back(bufferVal); // src
      args.push_back(headerValMap[oldVal]); // dest

      Value* retVal;

      if (valuePoint->getInstPos()) {
	Instruction* pos = TmpVMap[valuePoint->getInstPos()];
	
	// We Want to place the instructions after pos.
	BasicBlock::iterator posIt = BasicBlock::iterator(pos);
	posIt++;

	// Call yarn to store the buffer into memory.
	retVal = CallInst::Create(IMU->getYarnDepStoreFastFct(), args,
				  IMU->makeName('r'), *posIt);

	// Write the value into the buffer and place this instruction before the call.
	new StoreInst(oldVal, bufferVal, IMU->makeName('s'), retVal);
	
      }
      else {
	BasicBlock* pos = TmpVMap[ValuePoint->getBBPos()];
	assert (pos && "Either getInstPos or getBBPos should be non-null.");

	// Call yarn to store the buffer into memory.
	// Place the call at the beginning of the BB.
	retVal = CallInst::Create(IMU->getYarnDepStoreFastFct(), args,
				  IMU->makeName('r'), *pos->front());

	// Write the value into the buffer and place this instruction before the call.
	new StoreInst(oldVal, bufferVal, IMU->makeName('s'), retVal);	
      }
    }

    else {
      assert(false && "Sanity check.");
    }    
  }  


  // Instrument the pointer acceses.
  using YarnLoop::PtrInsertList;  
  const PtrInsertList& ptrInstr = YI->getPtrInstrPoints();
  for (PtrInsertList::iterator it = ptrInstr.begin(), itEnd = ptrIntr.end();
       it != itEnd; ++it)
  {
    PointerInsertPoint* ptrPoint = *it;
    
    if (Type == InstrLoad) {
      LoadInst* loadInst = dyn_cast<LoadInst*>(TmpVMap[ptrPoint->getInstruction()]);
      assert(loadInst && "InstrLoad should be a LoadInst");

      // Call yarn_dep_load to load the desired value into the buffer.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(loadInst->getPointerOperand()); // src
      args.push_back(bufferVal); // dest
      Value* retVal = 
	CallInst::Create(IMU->getYarnDepLoadFct(), args, 
			 IMU->makeName('r'), loadInst);

      // Load from the buffer which contains the result of the yarn call.
      loadInst->setOperand(0, bufferVal);
    }

    else if (Type == InstrStore) {
      StoreInst* storeInst = dyn_cast<StoreInst*>(TmpVMap[ptrPoint->getInstruction()]);
      assert(storeInst && "InstrStore should be a StoreInst");

      // call yarn_dep_load after the store instruction.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(bufferVal); // src
      args.push_back(storeInst->getPointerOperand()); // dest
      Value* retVal = 
	CallInst::Create(IMU->getYarnDepLoadFct(), args, 
			 IMU->makeName('r'), storeInst);

      // Store into the buffer which is then used by the yarn call.
      storeInst->setOperand(1, bufferVal);
    }

    else {
      assert(false && "Sanity check.");
    }    
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
