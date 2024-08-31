#ifndef NECKID_NECKID_NECKUTILS_H
#define NECKID_NECKID_NECKUTILS_H

#include <algorithm>
#include <deque>
#include <limits>
#include <llvm/IR/Function.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/biconnected_components.hpp>

#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include "NeckID/NeckID/NeckSearchTypes.h"

namespace neckid {

/// Annotations (not the LMCAS basic block id, but annotations about
/// operations we're doing to basic blocks) for marking which passes
/// assert or refute the boundary candidate property on basic blocks.

enum Annotation {
  ASSERT_TAINT,        // Assert the Tainted Condition Property
  REFUTE_TAINT,        // Refute the Tainted Condition Property
  ASSERT_DUP,          // Assert the "Basic Block Duplicated" property.
  REFUTE_DUP,          // Refute the "Basic Block Duplicated" property.
  ASSERT_LOOPTAINT,    // Assert the Loop Tainted Condition Property
  REFUTE_LOOPTAINT,    // Refute the Loop Tainted Condition Property
  ASSERT_BOUNDARY,     // Assert the Boundary Condition Property
  REFUTE_BOUNDARY,     // Refute the Boundary Condition Property
  ASSERT_ARTICULATION, // Assert the Articulation Condition Property
  REFUTE_ARTICULATION, // Refute the Articulation Condition Property
  ASSERT_CHOKEPOINT,   // Assert the Choke Point Condition Property
  REFUTE_CHOKEPOINT,   // Refute the Choke Point Condition Property
  ASSERT_NECK,         // Assert the Final Neck Candidate Property
  REFUTE_NECK,         // Refute the Final Neck Candidate Property
};
std::string toString(Annotation ann);
std::string makeAnnotation(Annotation ann, int pass);
std::string makeAnnotation(Annotation ann, int pass, int iter);

// The different errors that mergeBBs can encounter. Each of these
// correspond to a different violated precondition, and all probably
// correspond to bugs.
enum class MergeBBsError {
  // The parent and child basic block come from a different functions.
  DifferentFunction,

  // The parent's terminator was not a br.
  ParentTerminatorNotBranch,

  // The parent's terminator was a br, but it was not unconditional.
  ParentTerminatorConditional,

  // The parent's terminator was an unconditional br, but it did not
  // go to the child.
  ParentTerminatorNotToChild,

  // The child had a predecessor other than the parent (maybe it
  // should get duplicated?) or had no predecessors (maybe an awful
  // low-level API has been invoked that doesn't maintain backedges?).
  ChildHasWrongPredecessors,
};

using MergeBBResult = std::variant<llvm::BasicBlock *, MergeBBsError>;

// get the the id string '<operand>&<LmcasBasicBlockID>'
// for the given basic block
std::string getBBIdsString(llvm::BasicBlock *BB);

// let's define some handy helper functions
std::string getBBListAsString(std::unordered_set<llvm::BasicBlock *> &BBs);

// Return either the function name or "???"
llvm::StringRef getSafeName(llvm::Function *func);

// Return either the name of the BB or the unique(ish) name given to it by
// LLVM.
// TODO: Spread around through this code, we use the printAsOperand a LOT.
std::string getBBName(llvm::BasicBlock *Node, bool removePrefix = false);

MergeBBResult mergeBBs(llvm::BasicBlock *parent, llvm::BasicBlock *child);

} // namespace neckid

#endif
