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
#include "YarnUtil.h"
#include <llvm/Yarn/YarnLoopInfo.h>
#include <llvm/Yarn/YarnCommon.h>
#include <llvm/Pass.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <llvm/Value.h>
#include <llvm/User.h>
#include <llvm/Instruction.h>
#include <llvm/Type.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/Statistic.h>
#include <algorithm>
#include <sstream>
#include <cassert>
using namespace llvm;
using namespace yarn;

//STATISTIC(YarnLoopCounter, "Counts number of loops Instrumented.");

namespace {

//===----------------------------------------------------------------------===//
/// InstrumentModuleUtil Decl

  class InstrumentModuleUtil : public Noncopyable {

    Module* M;

    typedef std::map<YarnLoop*, ArrayType*> LoopTypeMap;
    LoopTypeMap LoopTypes;

    bool DeclarationsInserted;

    const IntegerType* YarnWordTy;
    const IntegerType* EnumTy;
    const PointerType* VoidPtrTy;
    const FunctionType* YarnExecutorFctTy;

    Constant* YarnExecSimpleFct;
    Constant* YarnDepLoadFct;
    Constant* YarnDepLoadFastFct;
    Constant* YarnDepStoreFct;
    Constant* YarnDepStoreFastFct;

    unsigned ValCounter;

  public :
    
    InstrumentModuleUtil (Module* m) : 
      M(m), LoopTypes(), DeclarationsInserted(false),
      YarnWordTy(NULL), EnumTy(NULL), VoidPtrTy(NULL),
      YarnExecutorFctTy(NULL), YarnExecSimpleFct(NULL),
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
    inline const IntegerType* getEnumType() const {
      return EnumTy;
    }
    inline const PointerType* getVoidPtrType () const {
      return VoidPtrTy;
    }
    inline const FunctionType* getYarnExecutorFctType () const {
      return YarnExecutorFctTy;
    }

    inline Constant* getYarnExecSimpleFct () const { 
      return YarnExecSimpleFct; 
    }
    inline Constant* getYarnDepLoadFct () const { 
      return YarnDepLoadFct; 
    }
    inline Constant* getYarnDepLoadFastFct () const { 
      return YarnDepLoadFastFct; 
    }
    inline Constant* getYarnDepStoreFct () const { 
      return YarnDepStoreFct; 
    }
    inline Constant* getYarnDepStoreFastFct () const { 
      return YarnDepStoreFastFct; 
    }

    std::string makeName(char prefix);    
    void createDeclarations();
    ArrayType* createLoopArrayType (YarnLoop* yl, unsigned valueCount);

    inline ArrayType* getLoopArrayType (YarnLoop* yl) {
      return LoopTypes[yl];
    }

    void print (llvm::raw_ostream &O) const;

  };


//===----------------------------------------------------------------------===//
/// ValMapper

  typedef ValueMap<const Value*, Value*> VMapTy;

  template <typename ValTy = Value>
  struct map {      
    static ValTy* get(VMapTy& VMap, const Value* srcVal) {
      Value* val = VMap[srcVal];
      assert(val && "Provided srcVal isn't mapped.");
	
      ValTy* castedVal = dyn_cast<ValTy>(val);
      assert(castedVal && "Wrong ValTy for the mapped value.");
	
      return castedVal;	
    }
  };



//===----------------------------------------------------------------------===//
/// LoopInstrumentation Decl

  class InstrumentLoopUtil : public Noncopyable {

    InstrumentModuleUtil* IMU;
    YarnLoop* YL;

    Function* SrcFct; // Original function to instrument
    Function* TmpFct; // Copy of SrcFct used for instrumentation
    Function* NewFct; // Final instrumented function added to the module.

    VMapTy TmpVMap;
    VMapTy NewVMap;
    
  public:
   
    InstrumentLoopUtil(InstrumentModuleUtil* imu, Function* f, YarnLoop* yl) :
      IMU(imu), YL(yl),
      SrcFct(f), TmpFct(NULL), NewFct(NULL),
      TmpVMap(), NewVMap()
    {}
    

    void instrumentLoop ();
    void print (llvm::raw_ostream &O) const;

  private:

    void createTmpFct();
    void createNewFct();
    void instrumentSrcFct();

    void instrumentTmpBody (Value* poolIdVal, 
			    Value* bufferWordPtr, 
			    Value* bufferVoidPtr);
    void cleanupTmpFct(BasicBlock*);

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
    
} // anynmous namespace


//===----------------------------------------------------------------------===//
/// InstrumentModuleUtil Impl
void InstrumentModuleUtil::createDeclarations () {
  if (DeclarationsInserted) {
    return;
  }

  YarnWordTy = IntegerType::get(M->getContext(), YarnWordBitSize);
  M->addTypeName("yarn_word_t", YarnWordTy);

  EnumTy = Type::getInt32Ty(M->getContext());
  M->addTypeName("enum_t", EnumTy);

  VoidPtrTy = PointerType::getUnqual(Type::getInt8Ty(getContext()));
  M->addTypeName("void_ptr_t", VoidPtrTy);

  const Type* boolTy = Type::getInt1Ty(getContext());

  {
    std::vector<const Type*> args;
    args.push_back(YarnWordTy); // yarn_word_t pool_id
    args.push_back(VoidPtrTy);  // void* data
    YarnExecutorFctTy = FunctionType::get(YarnWordTy, args, false); // enum yarn_ret
    M->addTypeName("yarn_executor_t", YarnExecutorFctTy);
  }

  {
    std::vector<const Type*> args;
    args.push_back(PointerType::getUnqual(YarnExecutorFctTy)); //yarn_executor_t executor
    args.push_back(VoidPtrTy); // void* data
    args.push_back(YarnWordTy); // thread_count
    args.push_back(YarnWordTy); // ws_size
    args.push_back(YarnWordTy); // index_size
    FunctionType* t = FunctionType::get(boolTy, args, false); 

    YarnExecSimpleFct = M->getOrInsertFunction("yarn_exec_simple", t);
  }

  {
    std::vector<const Type*> args;
    args.push_back(YarnWordTy); // yarn_word_t pool_id
    args.push_back(VoidPtrTy); // const void* src
    args.push_back(VoidPtrTy); // const void* dest
    FunctionType* t = FunctionType::get(boolTy, args, false);

    YarnDepLoadFct = M->getOrInsertFunction("yarn_dep_load", t);
    YarnDepStoreFct = M->getOrInsertFunction("yarn_dep_store", t);
  }

  {
    std::vector<const Type*> args;
    args.push_back(YarnWordTy); // yarn_word_t pool_id
    args.push_back(YarnWordTy); // yarn_word_t index_id
    args.push_back(VoidPtrTy); // const void* src
    args.push_back(VoidPtrTy); // const void* dest
    FunctionType* t = FunctionType::get(boolTy, args, false);

    YarnDepLoadFastFct = M->getOrInsertFunction("yarn_dep_load_fast", t);
    YarnDepStoreFastFct = M->getOrInsertFunction("yarn_dep_store_fast", t);
  }

  DeclarationsInserted = true;
}

ArrayType* InstrumentModuleUtil::createLoopArrayType (YarnLoop* yl, unsigned valueCount) {
  ArrayType* t = ArrayType::get(YarnWordTy, valueCount);
  M->addTypeName(makeName('t'), t);
  LoopTypes[yl] = t;
  return t;
}

std::string InstrumentModuleUtil::makeName(char prefix) {
  std::stringstream ss;
  ss << "y" << prefix << (++ValCounter);
  return ss.str();
}





//===----------------------------------------------------------------------===//
/// InstrumentLoopUtil Impl


void InstrumentLoopUtil::instrumentLoop() {
  // Add the struct type required to store the deps and invariants.
  IMU->createLoopArrayType(YL, YL->getArrayEntries().size());

  // Create the speculative function and instrument it.
  createTmpFct();
  createNewFct();

  // Add the call to the speculative function.
  // These will trash our analysis data. Do them last.
  instrumentSrcFct();

}

void InstrumentLoopUtil::createTmpFct () {
  // Clone the function that we will be working on.
  TmpFct = CloneFunction(SrcFct, TmpVMap, false);

  // Create a new header BB before the loop. 
  // This will eventually be the first BB in the fct.
  BasicBlock* loopHeader = map<BasicBlock>::get(TmpVMap, YL->getLoop()->getHeader());
  BasicBlock* instrHeader = 
    BasicBlock::Create(IMU->getContext(), IMU->makeName('b'), TmpFct, loopHeader);


  // Create pool_id and task argument placeholders. Needed by createNewFct.
  Value* poolIdVal = 
    new BitCastInst(ConstantInt::get(IMU->getYarnWordType(), 0),
		    IMU->getYarnWordType(), IMU->makeName('z'), instrHeader);
  Value* taskPtr = 
    new IntToPtrInst(ConstantInt::get(IMU->getYarnWordType(), 0),
		      IMU->getVoidPtrType(), IMU->makeName('z'), instrHeader);
  
  // struct task* s = (struct task*) task;
  Value* loopArrayPtr =
    new BitCastInst(taskPtr, 
		    PointerType::getUnqual(IMU->getLoopArrayType(YL)),
		    IMU->makeName('s'), instrHeader);
  
  // Extract the structure for use within the function.
  typedef YarnLoop::ArrayEntryList AEL;
  const AEL& arrayEntries = YL->getArrayEntries();

  for (unsigned i = 0; i < arrayEntries.size(); ++i) {
    ArrayEntry* ae = arrayEntries[i];

    std::vector<Value*> indexes;
    indexes.push_back(ConstantInt::get(Type::getInt32Ty(IMU->getContext()), 0));
    indexes.push_back(ConstantInt::get(Type::getInt32Ty(IMU->getContext()), i));

    // Get the pointer to the array index.
    Value* ptr =
      GetElementPtrInst::Create(loopArrayPtr,
				indexes.begin(), indexes.end(),
				IMU->makeName('p'), instrHeader);

    if (ae->getIsInvariant()) {
      Value* entryVal = map<>::get(TmpVMap, ae->getEntryValue());

      Value* invariant = 
	new LoadInst(ptr, IMU->makeName('v'), instrHeader);

      // The value is an invariant so replace the old val with the loaded val.
      Function::iterator bbIt (instrHeader);      
      replaceUsesInScope(bbIt, TmpFct->end(), entryVal, invariant); 
    }
    else {
      // Gotta make sure it's a void* for the calls to yarn_dep
      Value* castedPtr = 
	new BitCastInst(ptr, IMU->getVoidPtrType(), IMU->makeName('p'), instrHeader);

      ae->setPointer(castedPtr);
    }
  }
    
  // Create a buffer that will be used to load and store the instrumented values.
  Value* bufferWordPtr = 
    new AllocaInst(IMU->getYarnWordType(), 0, IMU->makeName('z'), instrHeader);

  // Gotta make sure it's a void ptr type.
  Value* bufferVoidPtr = 
    new BitCastInst(bufferWordPtr, IMU->getVoidPtrType(), 
		    IMU->makeName('b'), instrHeader);
  

  // Add the terminator instruction.
  BranchInst::Create(loopHeader, instrHeader);

  // Do the rest of the instrumentation.
  instrumentTmpBody(poolIdVal, bufferWordPtr, bufferVoidPtr);
  cleanupTmpFct(instrHeader);
  
}


void InstrumentLoopUtil::instrumentTmpBody (Value* poolIdVal, 
					    Value* bufferWordPtr, 
					    Value* bufferVoidPtr)
{

  // Process the value accesses.
  typedef YarnLoop::ValueInstrList VIL;
  const VIL& valInstrList = YL->getValueInstrs();
  for (VIL::const_iterator it = valInstrList.begin(), itEnd = valInstrList.end();
       it != itEnd; ++it)
  {
    const ValueInstr* valueInstr = *it;
    const ArrayEntry* ae = YL->getArrayEntry(valueInstr->getIndex());
    
    if (valueInstr->getType() == InstrLoad) {
      Instruction* oldVal = map<Instruction>::get(TmpVMap, valueInstr->getValue());
      
      // Create the arguments for the yarn_dep call.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(ConstantInt::get(IMU->getYarnWordType(), valueInstr->getIndex()));
      args.push_back(ae->getPointer()); // src
      args.push_back(bufferVoidPtr); // dest

      Instruction* retVal;
      Instruction* newVal;

      if (valueInstr->getInstPos()) {
	Instruction* pos = map<Instruction>::get(TmpVMap, valueInstr->getInstPos());

	// Call yarn_dep_load to load the desired value into the buffer.
	// Place before the target instruction.
	retVal = CallInst::Create(IMU->getYarnDepLoadFastFct(), 
				  args.begin(), args.end(), 
				  IMU->makeName('r'), pos);

	// Load the buffer into a new value.
	// Place before the target instruction.
	newVal = new LoadInst(bufferWordPtr, IMU->makeName('l'), pos);		
      }

      else {
	BasicBlock* pos = map<BasicBlock>::get(TmpVMap, valueInstr->getBBPos());
	assert (pos && "Either getInstPos or getBBPos should be non-null.");
	
	// Call yarn_dep_load to load the desired value into the buffer. 
	//Append at the end of the BB.
	retVal = CallInst::Create(IMU->getYarnDepLoadFastFct(), 
				  args.begin(), args.end(),
				  IMU->makeName('r'), pos);

	// Load the buffer into a new value.
	//Append at the end of the BB.
	newVal = new LoadInst(bufferWordPtr, IMU->makeName('l'), pos);	
      }

      // Replace the old value with the loaded value 
      Function::iterator bbIt (oldVal->getParent());      
      replaceUsesInScope(bbIt, TmpFct->end(), oldVal, newVal); 
    }

    else if (valueInstr->getType() == InstrStore) {
      Value* oldVal = map<>::get(TmpVMap, valueInstr->getValue());

      // Create the arguments for the yarn_dep call.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(ConstantInt::get(IMU->getYarnWordType(), valueInstr->getIndex()));
      args.push_back(bufferVoidPtr); // src
      args.push_back(ae->getPointer()); // dest

      Instruction* retVal;

      if (valueInstr->getInstPos()) {
	// Call yarn to store the buffer into memory.
	retVal = CallInst::Create(IMU->getYarnDepStoreFastFct(), 
				  args.begin(), args.end(),
				  IMU->makeName('r'));

	// Write the value into the buffer and place this instruction before the call.
	Instruction* storeInst = new StoreInst(oldVal, bufferWordPtr);


	// Place both instructions after the write instruction.
	Instruction* pos = map<Instruction>::get(TmpVMap, valueInstr->getInstPos());
	BasicBlock::InstListType& instList = pos->getParent()->getInstList();
	BasicBlock::iterator posIt(pos);

	instList.insertAfter(pos, retVal);
	instList.insertAfter(pos, storeInst);
	
      }
      else {
	BasicBlock* pos = map<BasicBlock>::get(TmpVMap, valueInstr->getBBPos());
	assert (pos && "Either getInstPos or getBBPos should be non-null.");

	// Call yarn to store the buffer into memory.
	// Place the call at the beginning of the BB.
	retVal = CallInst::Create(IMU->getYarnDepStoreFastFct(), 
				  args.begin(), args.end(),
				  IMU->makeName('r'), &pos->front());

	// Write the value into the buffer and place this instruction before the call.
	new StoreInst(oldVal, bufferWordPtr, &pos->front());	
      }
    }

    else {
      assert(false && "Sanity check.");
    }    
  }  


  // Instrument the pointer acceses.
  typedef YarnLoop::PointerInstrList PIL;  
  const PIL& ptrInstrList = YL->getPointerInstrs();
  for (PIL::const_iterator it = ptrInstrList.begin(), itEnd = ptrInstrList.end();
       it != itEnd; ++it)
  {
    const PointerInstr* ptrInstr = *it;
    
    if (ptrInstr->getType() == InstrLoad) {
      LoadInst* loadInst = map<LoadInst>::get(TmpVMap, ptrInstr->getInstruction());

      // Gotta make sure it's a void ptr type.
      Instruction* srcVoidPtr = 
	new BitCastInst(loadInst->getPointerOperand(), IMU->getVoidPtrType(), 
			IMU->makeName('v'), loadInst);

      // Call yarn_dep_load to load the desired value into the buffer.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(srcVoidPtr); // src
      args.push_back(bufferVoidPtr); // dest
      Value* retVal = CallInst::Create(IMU->getYarnDepLoadFct(), 
				       args.begin(), args.end(), 
				       IMU->makeName('r'), loadInst);
      (void) retVal; // \todo do some error checking.

      // Load from the buffer which contains the result of the yarn call.
      loadInst->setOperand(0, bufferWordPtr);
    }

    else if (ptrInstr->getType() == InstrStore) {
      StoreInst* storeInst = map<StoreInst>::get(TmpVMap, ptrInstr->getInstruction());
      
      Instruction* destVoidPtr = 
	new BitCastInst(storeInst->getPointerOperand(), IMU->getVoidPtrType(), 
			IMU->makeName('v'));

      // call yarn_dep_load after the store instruction.
      std::vector<Value*> args;
      args.push_back(poolIdVal);
      args.push_back(bufferVoidPtr); // src
      args.push_back(destVoidPtr); // dest
      Instruction* retVal = CallInst::Create(IMU->getYarnDepLoadFct(), 
				       args.begin(), args.end(), 
				       IMU->makeName('r'));
      (void) retVal; // \todo Do some error checking.

      // Insert both the instructions after the store instruction.
      BasicBlock::InstListType& instList = storeInst->getParent()->getInstList();
      BasicBlock::iterator insertPos(storeInst);
      instList.insertAfter(insertPos, retVal);
      instList.insertAfter(insertPos, destVoidPtr);

      // Store into the buffer which is then used by the yarn call.
      storeInst->setOperand(1, bufferWordPtr);
    }

    else {
      assert(false && "Sanity check.");
    }    
  }
}
void InstrumentLoopUtil::cleanupTmpFct(BasicBlock* headerBB) {  

  const Loop* l = YL->getLoop();

  // replace all forward out-of-loop edges by a ret break
  for (Loop::block_iterator it = l->block_begin(), itEnd = l->block_end();
       it != itEnd; ++it)
  {
    if (!l->isLoopExiting(*it)) {
      continue;
    }
    BasicBlock* exitingBB = map<BasicBlock>::get(TmpVMap, *it);
    
    // Remove the old terminator.
    exitingBB->getTerminator()->eraseFromParent();
    
    // Create a return inst that breaks out of the loop.
    ReturnInst::Create(IMU->getContext(), 
		       ConstantInt::get(IMU->getEnumType(), yarn_ret_break),
		       exitingBB);
  }

  // replace the backedge by a ret continue.
  {
    BasicBlock* latch = map<BasicBlock>::get(TmpVMap, l->getLoopLatch());
    assert(latch && "SimplifyLoop should ensure that we have a latch BB.");
    
    // Remove the old terminator.
    latch->getTerminator()->eraseFromParent();

    // Create a return inst that continues with the next iteration.
    ReturnInst::Create(IMU->getContext(), 
		       ConstantInt::get(IMU->getEnumType(), yarn_ret_continue),
		       latch);
  }  

  BasicBlock* exitBlock =  
    map<BasicBlock>::get(TmpVMap, YL->getLoop()->getUniqueExitBlock());

  errs() << "\n\n\n";
  TmpFct->print(errs());
  errs() << "\n\n\n";

  // Remove all the excess BB.
  pruneFunction(TmpFct, headerBB, exitBlock);
}


void InstrumentLoopUtil::createNewFct() {

  // Create the final speculative function
  NewFct = Function::Create(IMU->getYarnExecutorFctType(), 
			    GlobalValue::InternalLinkage,
			    IMU->makeName('f'), IMU->getModule());

  // Setup the VMap so that we delete all the old arguments.
  for (Function::arg_iterator it = TmpFct->arg_begin(), itEnd = TmpFct->arg_end();
       it != itEnd; ++it)
  {    
    NewVMap[&(*it)] = NULL;
  }

  // Clone tmp into the new function
  SmallVectorImpl<ReturnInst*> retList(4);
  CloneFunctionInto(NewFct, TmpFct, NewVMap, false, retList);

  // replace the placeholders by the arguments.
  Function::arg_iterator argIt = NewFct->arg_begin();
  BasicBlock::iterator bbIt = NewFct->front().begin();

  // Replace the pool_id argument with it's placeholder.
  Argument* argPoolId = &(*argIt);
  Instruction* tmpPoolId = dyn_cast<Instruction>(&(*bbIt));
  tmpPoolId->replaceAllUsesWith(argPoolId);

  argIt++;
  bbIt++;

  // Replace the task  argument with it's placeholder.
  Argument* argTaskPtr = &(*argIt);
  Instruction* tmpTaskPtr = dyn_cast<Instruction>(&(*bbIt));
  tmpTaskPtr->replaceAllUsesWith(argTaskPtr);

  // Remove the placeholders
  tmpPoolId->eraseFromParent();
  tmpPoolId = NULL;
  tmpTaskPtr->eraseFromParent();
  tmpTaskPtr = NULL;
}


void InstrumentLoopUtil::instrumentSrcFct() {
  // create new header BB
  BasicBlock* loopHeader = YL->getLoop()->getHeader();
  BasicBlock* loopExit = YL->getLoop()->getExitBlock();
  BasicBlock* oldPred = YL->getLoop()->getLoopPredecessor();

  BasicBlock* instrHeader = 
    BasicBlock::Create(IMU->getContext(), IMU->makeName('b'), NewFct, loopHeader);

  // Create a new array for all the value used by the speculative sys.
  Value* arrayPtr = 
    new AllocaInst(IMU->getLoopArrayType(YL), 0, IMU->makeName('s'), instrHeader);

  // Store the current values of the dep and invariants into the local strucutre.
  typedef YarnLoop::ArrayEntryList AEL;
  const AEL& arrayEntries = YL->getArrayEntries();

  for (unsigned i = 0; i < arrayEntries.size(); ++i) {
    ArrayEntry* ae = arrayEntries[i];
    Value* entryValue = ae->getEntryValue();

    std::vector<Value*> indexes;
    indexes.push_back(ConstantInt::get(Type::getInt32Ty(IMU->getContext()), 0));
    indexes.push_back(ConstantInt::get(Type::getInt32Ty(IMU->getContext()), i));

    // Get the pointer to the array index.
    Value* ptr =
      GetElementPtrInst::Create(arrayPtr,
				indexes.begin(), indexes.end(),
				IMU->makeName('p'), instrHeader);

    // Store the current value into the structure.
    new StoreInst(entryValue, ptr, instrHeader);

    if (!ae->getIsInvariant()) {
      ae->setPointer(ptr);
    }
  }

  // Cast the array pointer for the call to yarn_exec.
  Value* arrayVoidPtr = 
    new BitCastInst(arrayPtr, IMU->getVoidPtrType(), IMU->makeName('p'), instrHeader);

  // call yarn_exec_simple
  std::vector<Value*> args;
  args.push_back(TmpFct); // executor
  args.push_back(arrayVoidPtr); // ddata
  // threadCount - default value.
  args.push_back(ConstantInt::get(IMU->getYarnWordType(), 0));
  // ws_size - default value.
  args.push_back(ConstantInt::get(IMU->getYarnWordType(), 0));
  // index_size;
  args.push_back(ConstantInt::get(IMU->getYarnWordType(), 
				  YL->getValueInstrs().size()));

  Value* retVal = CallInst::Create(IMU->getYarnExecSimpleFct(), 
				   args.begin(), args.end(), 
				   IMU->makeName('r'), instrHeader);

  // Load the results of the call.
  for (size_t i = 0; i < arrayEntries.size(); ++i) {
    ArrayEntry* ae = arrayEntries[i];
    if (ae->getIsInvariant()) {
      continue;
    }

    Value* oldVal = ae->getEntryValue();

    // Load the calc results into a variable.
    Value* newVal = new LoadInst(ae->getPointer(), IMU->makeName('v'), instrHeader);
    // Save it for when we do the footer.
    ae->setNewValue(newVal);

    // lv might be NULL if we're dealing with exit only dep.
    LoopValue* lv = YL->getDependencyForValue(oldVal);
    if (lv) {
      // Update the entry phi node so that it takes our newly loaded value.
      PHINode* phi = lv->getHeaderNode();
      phi->removeIncomingValue(oldPred, false);
      phi->addIncoming(newVal, instrHeader);
    }
  }  
  
  // Check the return value of the speculative call.
  Value* comp = 
    new ICmpInst(*instrHeader, CmpInst::ICMP_EQ, retVal, 
		 ConstantInt::get(Type::getInt1Ty(IMU->getContext()), 1));

  // Goto to the end of the loop if we were successfull 
  // otherwise execute the loop with our partially calculated values.
  BranchInst::Create(loopHeader, loopExit, comp, instrHeader);

  // Redirect loop pred to our header.
  TerminatorInst* term = oldPred->getTerminator();
  for(unsigned i = 0; i < term->getNumSuccessors(); i++) {
    if (term->getSuccessor(i) == loopHeader) {
      term->setSuccessor(i, instrHeader);
    }
  }

  // Update the exit PHINode of the loop to take the calc values.
  for (size_t i = 0; i < arrayEntries.size(); ++i) {
    ArrayEntry* ae = arrayEntries[i];
    
    // lv might be null if we're dealing with entry only deps.
    PHINode* exitNode = ae->getExitNode();
    if (exitNode) {
      // If we come straight from the header then take the value specified there.
      exitNode->addIncoming(ae->getNewValue(), instrHeader);
    }
  }  

}



//===----------------------------------------------------------------------===//
/// YarnInstrumentLoop Impl

char YarnInstrumentLoop::ID = 0;

bool YarnInstrumentLoop::runOnModule(Module &M) {
  bool didSomething = false;

  InstrumentModuleUtil imu(&M);

  for (Module::iterator it = M.begin(), itEnd = M.end(); it != itEnd; ++it) {
    Function& fct = *it;
    YarnLoopInfo& yli = getAnalysis<YarnLoopInfo>(fct);
    
    for (YarnLoopInfo::iterator loopIt = yli.begin(), loopItEnd = yli.end();
	 loopIt != loopItEnd; ++loopIt)
    {
      YarnLoop* loop = *loopIt;

      imu.createDeclarations();

      InstrumentLoopUtil ilu(&imu, &fct, loop);
      ilu.instrumentLoop();

      didSomething = true;
    }
  }

  return didSomething;
}


void YarnInstrumentLoop::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<YarnLoopInfo>();
}


void YarnInstrumentLoop::print (llvm::raw_ostream &O, const llvm::Module *M) const {

}



//===----------------------------------------------------------------------===//
/// Pass Registration

INITIALIZE_PASS(YarnInstrumentLoop, "yarn-loop",
                "Yarn loop instrumentation",
                false, false);
