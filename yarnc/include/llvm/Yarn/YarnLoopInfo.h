//===- YarnLoop.h - Example code from "Writing an LLVM Pass" --------------===//
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

#ifndef YARN_LOOP_INFO_H
#define YARN_LOOP_INFO_H

#include "YarnCommon.h"
#include <llvm/Pass.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/BasicBlock.h>
#include <vector>
#include <map>
#include <set>


namespace yarn {

  class llvm::Value;
  class llvm::User;
  class llvm::Instruction;
  class llvm::PHINode;
  class llvm::AnalysisUsage;
  class llvm::AliasAnalysis;
  class llvm::DominatorTree;
  class llvm::LoopInfo;
  class llvm::raw_ostream;
  class std::ostream;

  class YarnLoop;

//===----------------------------------------------------------------------===//
/// Describes a pointer used in the loop.
///
  class LoopPointer : public Noncopyable {
  public:
    
    typedef std::vector<Value*> AliasList;

  private:

    AliasList Aliases;

  public:
    
    LoopPointer () : Aliases() {}


    /// Returns the list of pointers that are known to always be aliases.
    inline const AliasList& getAliasList () const { return AliasList; }


    /// Debug.  
    inline void print (llvm::raw_ostream &OS) const;


  private:
    
    // It's a friend because it needs to manipulate the IsRead/Write members.
    friend class YarnLoop;

  };


//===----------------------------------------------------------------------===//
/// Contains the dependency defined by the loops' phi nodes.
///
  class LoopValue : public Noncopyable {
    
    // Node that defines the Entry and Exiting Value
    llvm::PHINode* HeaderNode;
    //Node that uses the Exiting Value to define the Exit value.
    llvm::PHINode* FooterNode;

    // Value of the dependency before the loop.
    llvm::Value* EntryValue;

  public:
    
    LoopValue () :
      HeaderNode(NULL), FooterNode(NULL), EntryValue(NULL)
    {}

    /// The phi node in the loop header used to determine what the 
    /// entry and exiting values are.
    inline llvm::PHINode* getHeaderNode () const { return HeaderNode; }
    /// The phi node in the loop footer used to determine what the 
    /// exit value is.
    inline llvm::PHINode* getFooterNode () const { return FooterNode; }

    /// Value that existed before the loop that is used within the loop.
    /// Null if the value isn't used before the loop.
    inline llvm::Value* getEntryValue () const { return EntryValue; }
    /// Value that is used within the loop and that continues to exist after the loop.
    /// Null if the value isn't used after the loop.
    inline llvm::Value* getExitValue () const { return FooterNode; }

    /// Debug.  
    inline void print (llvm::raw_ostream &OS) const;

  private :
    
    /// Friend because it needs to manipulate the Entry/ExitValue.
    friend class YarnLoop;

  };

//===----------------------------------------------------------------------===//
/// Represents that type of instrumentation that needs to take place.
  enum InstrType {
    InstrLoad = 1,
    InstrStore = 2
  };


//===----------------------------------------------------------------------===//
/// Holds the pointer instruction that needs to be instrumented.
  class PointerInsertPoint : public Noncopyable {
    
    InstrType Type;
    llvm::Instruction* I;

  public :
    
    PointerInsertPoint (InstrType t, llvm::Instruction* i) : 
      Type(t),
      I(i) 
    {}

    inline InstrType getType () const { return Type; }
    /// Returns the instruction to instrument.
    inline llvm::Instruction* getInstruction () const { return I; }

  };

//===----------------------------------------------------------------------===//
/// Indicates where the load/store instrumentation for a value should placed.
  class ValueInsertPoint : public Noncopyable {
    
    InstrType Type;
    llvm::Value* V;

    // Instrumentation will happen AFTER this instruction.
    llvm::Instruction* Pos;

  public :

    ValueInsertPoint (InstrType t, llvm::Value* v, llvm::Instruction p) : 
      Type(t),
      V(v), 
      Pos(p) 
    {}

    inline InstrType getType () const  { return Type; }
    /// Returns the value to load/store.
    inline llvm::Value* getValue () const { return V; }
    /// The instrumentation should take place AFTER the instruction returned.
    inline llvm::Instruction* getPosition () const { return Pos; }
    
  };


  


//===----------------------------------------------------------------------===//
/// Contains info about a loop that needs to be instrumented. 
  class YarnLoop : public Noncopyable {

  public:

    typedef std::vector<LoopValue*> ValueList;
    typedef std::vector<LoopPointer*> PointerList; 
    typedef std::vector<llvm::Value*> InvariantList;
   
    typedef std::vector<PointerInsertPoint*> PtrInsertList;
    typedef std::vector<ValueInsertPoint*> ValueInsertList;

    // A true value indicates that it should be loaded right-away.
    // while false indicates that it should be loaded as needed with yarn_load/store.
    typedef std::pair<Value*, bool> EntryValueTuple;
    typedef std::vector<EntryValueTuple> EntryValueList;

    typedef llvm::Instruction* BBPos;
    typedef std::vector<BBPos> BBPosList;

  private:

    llvm::LoopInfo* LI;
    llvm::AliasAnalysis* AA;
    llvm::DominatorTree* DT;
    llvm::PostDominatorTree* PDT;
    
    llvm::Loop* L;

    /// Represents a list of values that are used within and before/after the loop.
    ValueList Dependencies;
    
    /// Represents pointers who are known to treat with aliased memory.
    PointerList Pointers;

    /// Represents values that are used outside the loop but only read within the loop.
    /// May be a pointer or a regular value.
    InvariantList Invariants;

    /// List locations where isntrumentation should be added.
    PtrInsertList PtrInstrPoints;
    ValueInsertList ValueInstrPoints;

    /// List of all the values that need to be passed to the speculative function.
    EntryValueList EntryValues;

  public:
    
    YarnLoop(llvm::Loop* l, 
	     llvm::LoopInfo* li,
	     llvm::AliasAnalysis* aa, 
	     llvm::DominatorTree* dt,
	     llvm::PostDominatorTree* pdt);

    ~YarnLoop();

    /// Returns the llvm::Loop object that represents our loop.
    inline const llvm::Loop* getLoop () const { return L; }

    /// List of all the values that need to be instrumented.
    inline const ValueList& getDependencies () const { return Dependencies; }
    
    /// List of all the pointers that need to be instrumented.
    inline const PointerList& getPointers () const { return Pointers; }

    /// List of all the loop invariants that are needed but should not be instrumented.
    /// There's no overlap between tthe getDependencies and the getPointers list.
    inline const InvariantList& getInvariants () const { return Invariants; }

    /// Instrumentation points for the pointer dependencies.
    inline const PtrInstrList& getPtrInstrPoints () const { return PtrInstrPoints; }

    /// Instrumentation points for the value dependencies.
    inline const ValueInstrList& getValueInstrPoints () const { return ValueInstrPoints; }

    /// List of all the values that need to be passed to the speculative function.
    /// If the tuple contains a true value then it should be loaded in the header.
    inline const EntryValues& getEntryValues () const { return EntreyValues; }

    
    /// Debug.  
    void print (llvm::raw_ostream &OS) const;
    /* 
    */

  private:

    typedef std::map<llvm::Value*, LoopValue*>  ValueMap;    
    typedef std::set<llvm::Instruction*> PointerInstSet;

    // Loops over the instructions and dispatches to functions below.
    void processLoop ();
    // Called for PHINode in the loop header and fills in Dependencies.
    void processHeaderPHINode (llvm::PHINode* N, ValueMap& ExitingValueMap);
    // Called for PHINode in the loop footer and fills in Dependencies.
    void processFooterPHINode (llvm::PHINode* N, ValueMap& ExitingValueMap);
    // Called for a store/load instruction and fills in Pointers.
    void processPointers (PointerInstSet& loadSet, PointerInstSet& storeSet);
    // Called for anything else that has operands and fills in Invariants.
    void processInvariants (llvm::Instruction* I);

    /// Finds the instrumentation points for all the pointer dependencies.
    void processPtrPoints ();
    /// Finds the instrumentation points for all the value dependencies.
    void processValuePoints ();

    void processEntryValues ();

    /// Finds the earliest point where a yarn_load can occur that will 
    /// dominate all the possible reads.
    /// Only supports LoopValues for now.
    BBPosList findLoadPos (const Value* value) const;

    /// Finds the latest point where a yarn_store can occur that will be dominated
    /// by all the possible writes.
    /// Only supports LoopValues for now.
    /// \todo Could also do LoopPointer if we know that they're aliases.
    BBPosList findStorePos (const Value* value) const;


  };


//===----------------------------------------------------------------------===//
/// Function pass that finds and analyzes loops that needs to be instrumented.
  class YarnLoopInfo : public llvm::FunctionPass, public Noncopyable {
  public:

    typedef std::vector<YarnLoop*> LoopList;
    typedef Loops::iterator iterator;

  private:

    llvm::LoopInfo* LI;
    llvm::AliasAnalysis* AA;

    LoopList Loops;

  public:
    static char ID; // Pass identification, replacement for typeid.

    YarnLoopInfo() : FunctionPass(ID) {}
    
    inline iterator begin () { return Loops.begin(); }
    inline iterator end () { return Loops.end(); }
    bool empty () const { return Loops.empty(); }
    
    virtual void getAnalysisUsage (llvm::AnalysisUsage &AU) const;    
    virtual bool runOnFunction (llvm::Function& F);
    virtual void releaseMemory ();
    virtual void print (llvm::raw_ostream &O, const llvm::Module *M) const;

  private:

    /// Checks the loop to make sure it doesn't contain any unsupported features.
    bool checkLoop (llvm::Loop* L);
    
    /// Determines whether the loop is worth instrumenting or not.
    bool keepLoop (YarnLoop* YL);


  };


}; // namespace yarn.


#endif // YARN_LOOP_INFO_H
