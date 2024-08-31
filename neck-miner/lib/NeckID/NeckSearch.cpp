#include <algorithm>
#include <deque>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/biconnected_components.hpp>

#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "NeckID/NeckID/BB_BFS_Q.h"
#include "NeckID/NeckID/NeckAnalysis.h"
#include "NeckID/NeckID/NeckSearch.h"
#include "NeckID/NeckID/TaintAnalysis.h"

#include "FmtLlvm.h"
#include <spdlog/spdlog.h>

namespace neckid {

NeckSearch::NeckSearch(NeckAnalysis &NA, llvm::Function *afunc, bool debugp)
    : debug(debugp), NA(NA), F(nullptr), start_func(afunc) {
  reinit();
}

void NeckSearch::reinit() {
  S.clear();

  S.push_back({NA, start_func, &start_func->getEntryBlock(), false, debug});
  F = &S.back();
}

// TODO: This function and the call sites should be replaced by spdlog.
void NeckSearch::log(const char *msg) { spdlog::debug(msg); }

bool NeckSearch::finished() {
  if (&S.back() != F) {
    spdlog::error("{}: F is not top of S", __func__);
    std::exit(EXIT_FAILURE);
  }
  return S.size() == 1 && F->finished();
}

BB_BFS_Q_Path NeckSearch::get_complete_path() {
  BB_BFS_Q_Path cpath;
  for (BB_BFS_Q &bbq : S) {
    cpath.push_back({bbq.getFunction(), bbq.currentPath()});
  }
  return cpath;
}

BB_BFS_Q_Func_Path NeckSearch::get_function_path() {
  BB_BFS_Q_Func_Path fpath;
  for (BB_BFS_Q &bbq : S) {
    fpath.push_back(bbq.getFunction());
  }
  return fpath;
}

// This models the "is it a member of the list" predicate in the python
bool NeckSearch::IsCalleeInPath(llvm::Function *callee,
                                BB_BFS_Q_Func_Path &fpath) {
  for (llvm::Function *func : fpath) {
    if (func == callee) {
      return true;
    }
  }
  return false;
}

// Perform one visit and process all yields until the next visit.
// Pass the visited node, if it is a choke point, and the current
// function path from start_func to the node to the visit_func.
// Return a tuple where:
//  first element is True if there are more nodes to expand else False,
//  second value is if the visit func was actually called or not.
//  third element is return value of VisitFunc or False if not called,
// return value of the visit_func.

NS_Visit_Next_Result NeckSearch::visit_next(VisitFunc visit_func) {
  if (F->length() == 0) {
    // This should only happen if visit_next() was called on an already
    // finished NeckSearch. This is because the visit_next() will
    // automatically push the expand() until the last possible moment
    // and either we'll know we're done with the search, or know we can
    // visit_next() again.
    return {not finished(), false, false};
  }

  // Otherwise, call the visit_func and expand until the search is found
  // to be completed, or we can call visit_next() again.

  // visit current node ONCE.
  auto [v, chokep] = F->visit();
  BB_BFS_Q_Path complete_path = get_complete_path();
  auto ret = visit_func(v, chokep, F->getChokePointForbidden(), complete_path);

  // self.log(f"** Visit: F[{self.F.funcName}]@{v} Chokep: {chokep}")
  // self.log(f"   | Complete path: {complete_path}")

  // Purpose of this is to fully expand() until the only thing left to
  // do is visit (or ALL DONE)
  while (true) {
    BB_BFS_Q_ExpandResult outcome = F->expand();
    if (debug) {
      spdlog::debug("++ F[{}] Outcome: TODO: <outcome>", *(F->getFunction()));
    }

    if (F->finished()) {
      if (S.size() == 1) {
        // The search is completely finished. ALL DONE
        break;
      }

      // POP AND BACKTRACK

      // Back track to a previous function we're exploring...
      //
      // First, get contribution of the function call we are leaving.
      AttributeMap final_contrib = F->compute_final_contribution();

      // Then pop off the function and return back to previous call.
      S.pop_back();  // Get rid of top of S.
      F = &S.back(); // F is now the new top.
      // Then finally accumulate into the visiting basic block
      // the contribution of the previous callee.
      F->accumulate_callee_contribution(final_contrib);
      // expand the less deep F we are coming back to.

      // debugging output
      llvm::BasicBlock *vnode = std::get<0>(F->currentPath().back());
      if (debug) {
        spdlog::debug("Backtracked to: F[{}]@{}", vnode->getParent()->getName(),
                      getBBName(vnode));
      }
      continue;
    }

    if (F->getYielded()) {
      if (debug) {
        spdlog::debug("Yield: YIELD at Path: F[{}]@ TODO <the path>",
                      F->getFunction()->getName());
      }

      BB_BFS_Q_Func_Path fpath = get_function_path();
      llvm::Function *callee_func = F->getYieldAtCalleeFunc();
      int inst_idx = F->getYieldAtInstIdx();

      if (IsCalleeInPath(callee_func, fpath)) {
        if (debug) {
          spdlog::debug(
              "Yield: IGNORE attempted recursion into current function path");
        }
        continue;
      }

      // PUSH AND RECURSE

      // Looks like we can recurse into callee
      if (debug) {
        spdlog::debug("Yield: RECURSE into {}@{}", callee_func->getName(),
                      inst_idx);
      }

      // BIG NOTE: Recurse ingo new F here
      bool forbid_choke_points =
          (not F->getChokeP()) || F->getChokePointForbidden();
      // NOTE: Out of order from the python due to C++ semantics.
      // The newly contructing F will not be yielded here because we
      // just made it.
      S.push_back(
          {NA, callee_func, &callee_func->front(), forbid_choke_points, debug});
      // Make F the new top of the stack.
      F = &S.back();

      // Now, we check to see if the new BB_BFS_Q we just created has
      // immediately finished itself. This represents a state where the
      // BB_BFS_Q has decided it is memoized and has marked itself
      // done and we need to immediately process this condition
      // WITHOUT visit() being called!
      if (F->finished()) {
        // It is memoized! Handle the immediately finished BB_BFS_Q
        // right now in the loop!
        continue;
      }
      // The only thing left to do is visit() on the new F,
      // so stop expanding.
      break;
    }
    // Here F is not yielded and not finished, so we can only visit(),
    // so stop expanding.
    break;
  }

  return {not finished(), true, ret};
}

NS_Visit_All_Result NeckSearch::visit_all(VisitFunc visit_func) {
  bool user_continue = false;
  while (not finished()) {
    NS_Visit_Next_Result result = visit_next(visit_func);
    // TODO: Can I destructure with auto when one variable is already
    // defined and I just want to assign a new value into it?
    // It seems not.
    bool more_to_search = std::get<0>(result);
    bool visit_func_called = std::get<1>(result);
    user_continue = std::get<2>(result);
    if (more_to_search == false or user_continue == false) {
      return {more_to_search, user_continue};
    }
  }
  return {false, user_continue};
}

} // namespace neckid
