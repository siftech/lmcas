#ifndef NECKID_NECKID_NECKSEARCHTYPES_H
#define NECKID_NECKID_NECKSEARCHTYPES_H

#include <algorithm>
#include <any>
#include <deque>
#include <limits>
#include <llvm/IR/Function.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

namespace neckid {
// Forward declarations
class BB_BFS_Q;

// The pleathora of types to model the internals of the neck search
// data structures. These probably should be actual objects one fay.

using BB_BFS_Q_Size = std::deque<llvm::BasicBlock *>::size_type;

// Key: Node, Value: true if we've seen it, false otherwise.
using ObservedBBs = std::unordered_set<llvm::BasicBlock *>;
// Key: node, Value: parent node
using Parents = std::unordered_map<llvm::BasicBlock *, llvm::BasicBlock *>;
using ParentsIter = Parents::iterator;
using AttributeMap = std::unordered_map<std::string, std::any>;
using AttributeMapIter = AttributeMap::iterator;
using AttributeMapList = std::list<AttributeMap>;
using AttributeMapListIter = AttributeMapList::iterator;
using PolicyFunc = std::any (*)(const std::any &, const std::any &);
using AttributeMapPolicy = std::unordered_map<std::string, PolicyFunc>;
using AttributeMapPolicyIter = AttributeMapPolicy::iterator;
// The path through the function/bb graph.
using PathElemBB = std::tuple<llvm::BasicBlock *, int, AttributeMap>;
using PathElemBBs = std::list<PathElemBB>;
using PathElemBBsIter = PathElemBBs::iterator;
using PathElemFunc = std::tuple<llvm::Function *, PathElemBBs>;
using BB_BFS_Q_Path = std::list<PathElemFunc>;
using BB_BFS_Q_Path_Iter = BB_BFS_Q_Path::iterator;
using BB_BFS_Q_Exit_Paths = std::list<PathElemBBs>;
using BB_BFS_Q_Callee_Summaries =
    std::unordered_map<llvm::BasicBlock *, AttributeMap>;
using BB_BFS_Q_Callee_Summaries_Iter = BB_BFS_Q_Callee_Summaries::iterator;
using BB_BFS_Q_ExpandedBBs = std::list<llvm::BasicBlock *>;
using BB_BFS_Q_ExpandedBBsIter = BB_BFS_Q_ExpandedBBs::iterator;
using BB_BFS_Q_ExpandResult = std::tuple<std::string, std::any, std::any>;
using ParticipatingFunctionsSet = std::unordered_set<llvm::Function *>;
using ParticipatingFunctionsSetIter = ParticipatingFunctionsSet::iterator;
using ParticipatingBBsSet = std::unordered_set<llvm::BasicBlock *>;
using ParticipatingBBsSetIter = ParticipatingBBsSet::iterator;

using VisitNeckCandidate = std::tuple<llvm::BasicBlock *, BB_BFS_Q_Path>;

using VisitingBB = std::tuple<llvm::BasicBlock *, bool>;
using VisitFunc = std::function<bool(llvm::BasicBlock *node, bool chokep,
                                     bool forbidden, BB_BFS_Q_Path &fpath)>;

// Need to explicitly iterate over basic block instructions and keep a
// reference to the iterator.
using BBInstIter = llvm::BasicBlock::iterator;

// The stack of BB_BFS_Q objects we use to implement the NeckSearch
// Actually it is a deque which we treat as a stack with the back
// of the deque being the top of the stack. This allows us to iterate
// from the front to the back over the BB_BFS_Q objects build the
// complete path.
using BB_BFS_Q_Stack = std::deque<BB_BFS_Q>;
using BB_BFS_Q_Func_Path = std::list<llvm::Function *>;
using NS_Visit_Next_Result = std::tuple<bool, bool, bool>;
using NS_Visit_All_Result = std::tuple<bool, bool>;
} // namespace neckid

#endif
