#ifndef NECKID_NECKID_BB_BFS_Q_H
#define NECKID_NECKID_BB_BFS_Q_H

#include <algorithm>
#include <any>
#include <deque>
#include <limits>
#include <llvm/IR/Function.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include "NeckID/NeckID/NeckAnalysis.h"
#include "NeckID/NeckID/NeckSearchTypes.h"
#include "NeckID/NeckID/NeckUtils.h"
#include "NeckID/NeckID/TaintAnalysis.h"

namespace neckid {

class NeckAnalysis;

// names of keys in an attributes hash table.
extern const std::string attrname_numSucceededLoopHeads;
extern const std::string attrname_numSucceededTaintedLoopHeads;
extern const std::string attrname_numSucceededGAFs;
extern const std::string expandresult_expanded;
extern const std::string expandresult_yield;

// Functions associated with properties when we summarize the contribution of
// a single exit path.
extern AttributeMapPolicy policy_summarize_exit_path;

// Functions associated with properties when we summarize all of the
// contributions of a set of exit paths.
extern AttributeMapPolicy policy_merge_exit_path_summaries;

// When we 're accumulating a callee' s contributions into the basic block that
// called it, use this set of functions associated with properties.
extern AttributeMapPolicy policy_merge_basic_block_contribution;

// Utility Function
// Lookup the propName policy function in policy and apply it to the dst and src
// property key value and store in dst. This side effects into dst and also
// returns the new value.
// dst and src are hash tables, policy is a hash table of propname -> lambda
std::any apply_policy_to_property(AttributeMap &dst, AttributeMap &src,
                                  const std::string &propName,
                                  AttributeMapPolicy &policy);

// Utility Function
// Apply the policy to the src and dst hash tables.
AttributeMap &apply_policy(AttributeMap &dst, AttributeMap &src,
                           AttributeMapPolicy &policy);

// Take a single exit path and produce a contribution table
// Summarize Exit Path (Function local only)
//   1. Exit Path Summary Function.
//       (-> exit_path exit_path_contribution_ht)
AttributeMap summarize_exit_path(NeckAnalysis &NA, PathElemBBs &exit_path,
                                 AttributeMapPolicy &policy);

// take a list of exit path contributions and return a single summary table
// Merge Exit Path Summaries (Function local only)
//   2. Exit Summary Merge Function.
//       (-> exit_path_contribution_ht_list final_contribution_ht)
AttributeMap merge_exit_path_summaries(NeckAnalysis &NA,
                                       AttributeMapList &exit_paths_contribs,
                                       AttributeMapPolicy &policy,
                                       bool startLeft);

// Side effect into the summarization table the merging of the summary
// table by the policy. Also return the modified summarization table.
// Merge Basic Block Contribution
//   3. Basic Block Summerize a Summary Function.
//       (-> summarization_ht summary_ht summarization_ht)
AttributeMap merge_basic_block_contribution(NeckAnalysis &NA,
                                            AttributeMap &summarization,
                                            AttributeMap &summary,
                                            AttributeMapPolicy &policy);

// Walks the path from the leaf to the root and produces a summary of the
// attributes of that path according to the policy given.
AttributeMap summarize_complete_path(NeckAnalysis &NA, BB_BFS_Q_Path &fpath);

// This class implement a breadth first search of a single function.
// The BFS walks though each instruction in each basic block and can
// decide that recursing into a function is warranted, at which point
// it yields control to a higher level controller of the search:
// the NeckSearch object. This object takes care of synthesizing and
// inheriting the attribute information for each basic block.
// The NeckSearch object contains a stack of these objects.
//
// TODO: There is one real limitation of this code, it does not compute
// the all paths from one basic block to a future one in the BFS.
// This means some attribute information may be lost.
// In practice, it is ok _for now_ but it is clear it must be done
// for full observability of the attribute data.
class BB_BFS_Q {
private:
  NeckAnalysis &NA;
  bool debug;
  bool choke_point_forbidden;
  llvm::Function *func;
  std::deque<llvm::BasicBlock *> queue;
  ObservedBBs observed;
  Parents parents;
  llvm::BasicBlock *visiting;
  bool chokep;
  bool yielded;
  std::optional<BBInstIter> yielded_future;
  llvm::BasicBlock *yield_at_visiting;
  llvm::Function *yield_at_callee_func;
  int yield_at_inst_idx;
  bool visiting_inst_processed;
  BB_BFS_Q_Exit_Paths exit_paths;
  BB_BFS_Q_Callee_Summaries callee_summaries;

  // The BB_BFS_Q can discover, when choke points are fobidden, that the
  // function it has already searched was previously memoized. In this
  // case the BB_BFS_Q will get the previously used value, since it
  // shouldn't have changed, and mark itself finished() immediately.
  bool memoized;
  AttributeMap memoizedSummary;

public:
  BB_BFS_Q(NeckAnalysis &NA, llvm::Function *afunc, llvm::BasicBlock *bb_start,
           bool choke_point_forbiddenp, bool debugp = false);

  /// Emit a debugging log line if debugp == true
  void log(const char *);

  /// Return the length of the queue.
  BB_BFS_Q_Size length(void);

  bool finished(void);

  // Iterate over the exit_paths and compute a final contribution table
  AttributeMap compute_final_contribution();

  // We can check to see if there are summarizing attributes for this specific
  // basic block. If not, return an empty hash table.
  AttributeMap &get_callee_summary(llvm::BasicBlock *bb);

  // NOTE: This is ONLY called by the USER of BB_BFS_Q in order to summarize
  // the props of an entire function call both into the choke point BB's
  // summarization props from where it was called, and also into the
  // collective summary we're keeping.
  void accumulate_callee_contribution(AttributeMap &final_contrib);

  /// Visit the head of the queue by popping it off and storing it
  /// as the currently visiting node.
  VisitingBB visit();

  // Get the current BFS path through this function's CFG
  PathElemBBs currentPath();

  // This function walks through the instructions of basic blocks during
  // the BFS of the CFG.
  // It will figure out the children of the expanding node or yield
  // the search because we're about to recurse into a function.
  BB_BFS_Q_ExpandResult expand();

  // Get the Function that for which this BFS is working.
  llvm::Function *getFunction();

  // Return of we're considering choke points to be forbidden in the
  // search of this function.
  bool getChokePointForbidden();

  // Return if the BB_BFS_Q is yielded or not.
  bool getYielded();

  // Return the function at which we yielded.
  llvm::Function *getYieldAtCalleeFunc();

  // Return the instruction index at which we yielded.
  int getYieldAtInstIdx();

  // Return of the currently visiting node is a choke point or false
  // otherwise (including if no visit had happened yet!).
  bool getChokeP();
};

} // namespace neckid
  //
  //
#endif
