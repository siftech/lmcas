#ifndef NECKID_NECKID_NECKSEARCH_H
#define NECKID_NECKID_NECKSEARCH_H

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

#include "NeckID/NeckID/BB_BFS_Q.h"
#include "NeckID/NeckID/NeckAnalysis.h"
#include "NeckID/NeckID/NeckSearchTypes.h"
#include "NeckID/NeckID/NeckUtils.h"

namespace neckid {

class NeckAnalysis;

// This object performs a BFS-like search starting at a function
// (usually main()) at the front basic block. When a choke point
// is reached, a user supplied visit function is called and passed the
// path and attribute data specific to nodes in thst path that can decide
// if it is a neck choice or not. When the BB_BFS_Q that this uses
// yields, it means to push a new BB_BFS_Q onto a stack and keep searching
// that new function. When the search is done, the BB_BFS_Q is popped
// off the stack and the summarized attributes in that function call are
// synthesized into the basic block caller. It is possible and expected
// that functions may be searched multiple times if they are encountered
// multiple times. Though, there is some logic and memoization to make
// this fast by reusing results from a previous search of that function.
// The memoization is quite necessary for performance (3 hours to 3
// minutes speed improvement!)
//
class NeckSearch {
private:
  bool debug;
  NeckAnalysis &NA;
  // NOTE: When the NeckSearch is instantiated, S will always have at least
  // ONE entry in it.
  BB_BFS_Q_Stack S;
  BB_BFS_Q *F;
  llvm::Function *start_func;
  NS_Visit_Next_Result visit_next(VisitFunc);
  bool IsCalleeInPath(llvm::Function *callee, BB_BFS_Q_Func_Path &fpath);

public:
  NeckSearch(NeckAnalysis &NA, llvm::Function *afunc, bool debugp = false);

  void reinit();
  /// Emit a debugging log line if debugp == true
  void log(const char *);

  /// Are we completed searching the entire space?
  bool finished();

  // This returns a path that describes not only the steps through
  // the function caller/calle graph to the current location of the
  // search, but the exact basic blocks chosen through the CFG of each
  // function.
  BB_BFS_Q_Path get_complete_path();

  // This is the path through the function space from start to where
  // the search currently is.
  BB_BFS_Q_Func_Path get_function_path();

  // The VisitFunc is user supplied. The VisitFunc can decide to keep
  // searching or to stop the search.
  NS_Visit_All_Result visit_all(VisitFunc);
};
} // namespace neckid

#endif
