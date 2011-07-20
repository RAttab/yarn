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
    llvm::Value* Pointer;
    bool IsRead;
    bool IsWrite;

  public:
    
    LoopPointer (llvm::Value* ptr) : 
      Pointer(ptr),
      IsRead(false),
      IsWrite(false)
    {}

    inline llvm::Value* getPointer() const { return Pointer; }

    inline bool isReadOnly () const { return IsRead && !IsWrite; }
    inline bool isRead () const { return IsRead; }
    inline bool isWrite () const {return IsWrite; }

  private:
    
    // It's a friend because it needs to manipulate the IsRead/Write members.
    friend class YarnLoop;
  };


//===----------------------------------------------------------------------===//
/// Contains the dependency defined by the loops' phi nodes.
///
  class LoopValue : public Noncopyable {
    llvm::PHINode* Node;
    llvm::Value* EntryValue;
    llvm::Value* ExitValue;

  public:
    
    LoopValue (llvm::PHINode* node) :
      Node(node),
      EntryValue(NULL),
      ExitValue(NULL)
    {}

    /// The phi node in the loop header used to determine what the 
    /// entry and exit value is.
    inline llvm::PHINode* getPhiNode () const { return Node; }

    /// Value that existed before the loop that is used within the loop.
    /// Null if the value isn't used before the loop.
    /// \todo This will always be non-null because it's to hard to detect the other way.
    inline llvm::Value* getEntryValue () const { return EntryValue; }

    /// Value that is used within the loop and that continues to exist after the loop.
    /// Null if the value isn't used after the loop.
    inline llvm::Value* getExitValue () const { return ExitValue; }

  private:
    
    /// Friend because it needs to manipulate the Entry/ExitValue.
    friend class YarnLoop;

  };


//===----------------------------------------------------------------------===//
/// Contains info about a loop that needs to be instrumented. 
  class YarnLoop : public Noncopyable {

  public:

    typedef std::vector<LoopValue*> ValueList;
    typedef std::vector<LoopPointer*> PointerList; 
    typedef std::vector<llvm::Value*> InvariantList;

    typedef llvm::BasicBlock::iterator BBPos;
    typedef std::vector<BBPos> BBPosList;

  private:

    llvm::AliasAnalysis* AA;
    llvm::DominatorTree* DT;
    
    llvm::Loop* L;
    ValueList Dependencies;
    PointerList Pointers;
    InvariantList Invariants;

  public:
    
    YarnLoop(llvm::Loop* l, llvm::AliasAnalysis* aa, llvm::DominatorTree*);

    /// Returns the llvm::Loop object that represents our loop.
    inline llvm::Loop* getLoop () const { return L; }

    /// List of all the values that need to be instrumented.
    inline ValueList& getDependencies () const { return Dependencies; }
    
    /// List of all the pointers that need to be instrumented.
    inline PointerList& getPointers () const { return Pointers; }

    /// List of all the loop invariants that are needed but should not be instrumented.
    /// There's no overlap between tthe getDependencies and the getPointers list.
    inline InvariantList& getInvariants() const { return Invariants; }
    
    /// Finds the earliest point where a yarn_load can occur that will 
    /// dominate all the possible reads.
    /// Only supports LoopValues for now.
    /// \todo Could also do LoopPointer if we know that they're aliases.
    BBPos findLoadPos (const Value* value) const;
    //  Find the BB that dominates all the read's parent.
    //    if that BB contains a load
    //      return that load.
    //    else return the end of the block.

    /// Finds the latest point where a yarn_store can occur that will be dominated
    /// by all the possible writes.
    /// Only supports LoopValues for now.
    /// \todo Could also do LoopPointer if we know that they're aliases.
    BBPosList findStorePos (const Value* value) const;
    //  If value isa PHINode
    //    list.pushback(findStorePos(valueBranchA));
    //    list.pushback(findStorePos(valueBranchB));
    //  else list.pushback(toPos(value)+1);
    //  return list;

    
    /// Debug.  
    inline void print (llvm::raw_ostream &OS) const;
    /* 
    {
      m_loop->print(OS, m_loop->getLoopDepth());
    }
    */

  private:

    // Loops over the instructions and dispatches to functions below.
    void processLoop (llvm::Loop* L);
    // Called for PHINode in the loop header fills in Dependencies.
    void processHeaderPHINode (llvm::PHINode* N);
    // Called for a store/load instruction and fills in Pointers.
    void processPointer (llvm::Instruction* I);
    // Called for anything else that has operands and fills in Invariants.
    void processInvariants (llvm::User* U);

  };


//===----------------------------------------------------------------------===//
/// Function pass that finds and analyzes loops that needs to be instrumented.
  class YarnLoopInfo : public llvm::FunctionPass, public Noncopyable {
  public :

    typedef std::vector<YarnLoop*> LoopList;
    typedef Loops::iterator iterator;

  private:

    llvm::LoopInfo* LI;
    llvm::AliasAnalysis* AA;
    llvm::DominatorTree* DT;

    LoopList Loops;

  public:
    static char ID; // Pass identification, replacement for typeid.

    YarnLoopInfo() : FunctionPass(ID) {}
    
    inline iterator begin () { return Loops.begin(); }
    inline iterator end () { return Loops.end(); }
    bool empty () const { return Loops.empty(); }
    
    virtual void getAnalysisUsage (llvm::AnalysisUsage &Info) const;    
    virtual bool runOnFunction (llvm::Function& F);
    virtual void releaseMemory ();
    virtual void print (std::ostream &O, const llvm::Module *M) const;

  };


}; // namespace yarn.


#endif // YARN_LOOP_INFO_H
