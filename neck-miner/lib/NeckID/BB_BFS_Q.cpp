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
#include "NeckID/NeckID/TaintAnalysis.h"

#include "FmtLlvm.h"
#include <spdlog/spdlog.h>

namespace neckid {

const std::string attrname_numSucceededLoopHeads("numSucceededLoopHeads");
const std::string
    attrname_numSucceededTaintedLoopHeads("numSucceededTaintedLoopHeads");
const std::string attrname_numSucceededGAFs("numSucceededGAFs");
const std::string expandresult_expanded("expanded");
const std::string expandresult_yield("yield");

// These are higher order functions to compute attributes associated
// with basic blocks.

AttributeMapPolicy policy_summarize_exit_path{
    {attrname_numSucceededLoopHeads,
     [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::any_cast<const int &>(a) +
                       std::any_cast<const int &>(b));
     }},
    {attrname_numSucceededTaintedLoopHeads,
     [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::any_cast<const int &>(a) +
                       std::any_cast<const int &>(b));
     }},
    {attrname_numSucceededGAFs, [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::any_cast<const int &>(a) +
                       std::any_cast<const int &>(b));
     }}};

AttributeMapPolicy policy_merge_exit_path_summaries{
    {attrname_numSucceededLoopHeads,
     [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::max(std::any_cast<const int &>(a),
                                std::any_cast<const int &>(b)));
     }},
    {attrname_numSucceededTaintedLoopHeads,
     [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::max(std::any_cast<const int &>(a),
                                std::any_cast<const int &>(b)));
     }},
    {attrname_numSucceededGAFs, [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::max(std::any_cast<const int &>(a),
                                std::any_cast<const int &>(b)));
     }}};

AttributeMapPolicy policy_merge_basic_block_contribution{
    {attrname_numSucceededLoopHeads,
     [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::any_cast<const int &>(a) +
                       std::any_cast<const int &>(b));
     }},
    {attrname_numSucceededTaintedLoopHeads,
     [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::any_cast<const int &>(a) +
                       std::any_cast<const int &>(b));
     }},
    {attrname_numSucceededGAFs, [](const std::any &a, const std::any &b) {
       if (!a.has_value()) {
         return std::any(b);
       }
       return std::any(std::any_cast<const int &>(a) +
                       std::any_cast<const int &>(b));
     }}};

std::any apply_policy_to_property(AttributeMap &dst, AttributeMap &src,
                                  const std::string &propName,
                                  AttributeMapPolicy &policy) {
  AttributeMapIter tmpIter = dst.find(propName);
  std::any tmp = std::any();
  if (tmpIter != dst.end()) {
    tmp = tmpIter->second;
  }
  AttributeMapPolicyIter funcIter = policy.find(propName);
  if (funcIter == policy.end()) {
    spdlog::error("{}: Programmer error!", __func__);
    std::exit(EXIT_FAILURE);
  }
  // NOTE: We could pass an empty any as tmp here and it is expected that the
  // policy function deals with this case.
  PolicyFunc func = funcIter->second;
  dst[propName] = func(tmp, src[propName]);
  return dst[propName];
}

AttributeMap &apply_policy(AttributeMap &dst, AttributeMap &src,
                           AttributeMapPolicy &policy) {
  for (auto &i : src) {
    apply_policy_to_property(dst, src, i.first, policy);
  }
  return dst;
}

AttributeMap summarize_exit_path(NeckAnalysis &NA, PathElemBBs &exit_path,
                                 AttributeMapPolicy &policy) {

  // We only want the static contribution of the the last node in the basic
  // block path....
  AttributeMap accumulator;
  auto &[bb, idx, bb_summary_ht] = exit_path.back();
  auto attrMaps = NA.getBasicBlockAttributeMap();
  AttributeMap &bb_static_contrib = attrMaps.at(bb);
  apply_policy(accumulator, bb_static_contrib, policy);

  // Then, gather the callee_contributions for each BB to the start.
  // So it as a reduce from the right side of the list.
  for (auto it = exit_path.rbegin(); it != exit_path.rend(); ++it) {
    auto &[bbitem_bb, bbitem_idx, bbitem_contrib] = *it;
    apply_policy(accumulator, bbitem_contrib, policy);
  }
  return accumulator;
}

AttributeMap merge_exit_path_summaries(NeckAnalysis &NA,
                                       AttributeMapList &exit_paths_contribs,
                                       AttributeMapPolicy &policy,
                                       bool startLeft = true) {
  AttributeMap final_contribution;
  if (startLeft) {
    for (auto it = exit_paths_contribs.begin(); it != exit_paths_contribs.end();
         ++it) {
      apply_policy(final_contribution, *it, policy);
    }
  } else {
    for (auto it = exit_paths_contribs.rbegin();
         it != exit_paths_contribs.rend(); ++it) {
      apply_policy(final_contribution, *it, policy);
    }
  }
  return final_contribution;
}

AttributeMap merge_basic_block_contribution(NeckAnalysis &NA,
                                            AttributeMap &summarization,
                                            AttributeMap &summary,
                                            AttributeMapPolicy &policy) {
  apply_policy(summarization, summary, policy);
  return summarization;
}

AttributeMap summarize_complete_path(NeckAnalysis &NA, BB_BFS_Q_Path &fpath) {
  AttributeMap accumulate;
  AttributeMapList contrib;

  // Treat each portion as an exit path and compute the contrbution of it.
  // These can happen in any order.
  for (auto it = fpath.begin(); it != fpath.end(); ++it) {
    auto &[func, bbs] = *it;
    contrib.push_back(summarize_exit_path(NA, bbs, policy_summarize_exit_path));
  }

  // Then, we accumulate the result from leaf to root.
  for (auto it = contrib.rbegin(); it != contrib.rend(); ++it) {
    apply_policy(accumulate, *it, policy_merge_basic_block_contribution);
  }

  return accumulate;
}

BB_BFS_Q::BB_BFS_Q(NeckAnalysis &NA, llvm::Function *afunc,
                   llvm::BasicBlock *bb_start, bool choke_point_forbiddenp,
                   bool debugp)
    : NA(NA) {
  debug = debugp;
  choke_point_forbidden = choke_point_forbiddenp;
  func = afunc;
  queue.push_back(bb_start);
  observed.insert(bb_start);
  parents.insert({bb_start, nullptr});
  visiting = nullptr;
  chokep = false;
  visiting_inst_processed = false;

  yielded = false;
  yielded_future.reset();
  yield_at_visiting = nullptr;
  yield_at_callee_func = nullptr;
  yield_at_inst_idx = -1;

  memoized = false; // But we're about to see if this is true.

  // We now immediately check if we have a forbidden memoization of this
  // function we're about to explore and if we need to act as if we
  // already performed our search.
  if (not choke_point_forbidden) {
    // We must complete the search since all choke points in here are
    // something the visit function can see and possibly pick.
    return;
  }

  // Since choke points are forbidden we're only looking for the callee
  // summary information for this function. Let's see if we have an
  // entry in the forbidden memoization cache...
  auto &FMCache = NA.getForbiddenMemoCache();
  auto memoizedIter = FMCache.find(afunc);

  if (memoizedIter == FMCache.end()) {
    // No entry, so we'll have to complete the search anyway!
    return;
  }

  // OOO! There is an entry! Now we fix ourselves up to be marked as
  // finished so that the higher level NeckSearch function merely
  // gets our contribution and we never actuallysearch.

  // This sequence will mark ourselves as finished() and allow
  // compute_final_contribution() to do the right thing.
  memoized = true;
  (void)queue.pop_front(); // drop our start node, so length() == 0.
  visiting_inst_processed = true;
  memoizedSummary = memoizedIter->second;
  if (debug) {
    spdlog::debug("BB_BFS_Q: F[{}] is memoized.", func->getName());
  }
}

// log a message.
void BB_BFS_Q::log(const char *msg) { spdlog::debug(msg); }

BB_BFS_Q_Size BB_BFS_Q::length(void) { return queue.size(); }

// Determine if the BB_BFS_Q is done searching or not.
bool BB_BFS_Q::finished(void) {
  if (yielded && visiting_inst_processed) {
    spdlog::error("{}: This is a broken state. Can't be yielded and all "
                  "instructions were processed for a basic block.",
                  __func__);
    std::exit(EXIT_FAILURE);
  }
  return !yielded && queue.size() == 0 && visiting_inst_processed;
}

AttributeMap BB_BFS_Q::compute_final_contribution() {
  AttributeMapList results;

  if (memoized) {
    // TODO: Debugging output, factor into a better
    // "print an AttributeMap" function.
    /*
    std::any anyNumSucceededLoopHeads =
        memoizedSummary[attrname_numSucceededLoopHeads];
    std::any anyNumSucceededTaintedLoopHeads =
        memoizedSummary[attrname_numSucceededTaintedLoopHeads];
    std::any anyNumSucceededGAFs = memoizedSummary[attrname_numSucceededGAFs];
    const int numSucceededLoopHeads =
        std::any_cast<const int &>(anyNumSucceededLoopHeads);
    const int numSucceededGAFs =
        std::any_cast<const int &>(anyNumSucceededGAFs);
    if (debug) {
      llvm::outs() << "memoized summary: {numSucceededLoopHeads: "
                   << numSucceededLoopHeads
                   << ", numSucceededTaintedLoopHeads: "
                   << numSucceeded TaintedLoopHeads
                   << ", numSucceededLoopGAFs: " << numSucceededLoopHeads
                   << "\n";
    }
    */
    return memoizedSummary;
  }

  for (PathElemBBs &ep : exit_paths) {
    AttributeMap contrib =
        summarize_exit_path(NA, ep, policy_summarize_exit_path);
    results.push_back(contrib);
  }

  AttributeMap final_contribution =
      merge_exit_path_summaries(NA, results, policy_merge_exit_path_summaries);

  if (choke_point_forbidden) {
    // Then if we did all the work to get the final contribution,
    // and we're in a choke point forbidden state, then we can
    // memoize this information for the next time.
    auto &FMCache = NA.getForbiddenMemoCache();
    FMCache[func] = final_contribution;
  }

  return final_contribution;
}

// TODO verify this return type is sane (not falling out of scope etc)
AttributeMap &BB_BFS_Q::get_callee_summary(llvm::BasicBlock *bb) {
  BB_BFS_Q_Callee_Summaries_Iter csumIter = callee_summaries.find(bb);
  if (csumIter != callee_summaries.end()) {
    return csumIter->second;
  }
  callee_summaries[bb] = {};
  return callee_summaries[bb];
}

void BB_BFS_Q::accumulate_callee_contribution(AttributeMap &final_contrib) {
  // We accumulate the contribution INTO the visiting basic block's
  // callee_summaries. Get a reference to the callee summary.
  AttributeMap &csum = get_callee_summary(visiting);

  // side effect into the reference csum, which is stored in the
  // callee_summaries.
  merge_basic_block_contribution(NA, csum, final_contrib,
                                 policy_merge_basic_block_contribution);
}

VisitingBB BB_BFS_Q::visit() {
  if (yielded) {
    log("Programmer Error: Cannot visit() while yielded!");
    exit(EXIT_FAILURE);
  }

  visiting = queue.front();
  queue.pop_front();

  std::unordered_set<llvm::BasicBlock *> chokePoints = NA.getChokePoints();
  chokep = chokePoints.find(visiting) != chokePoints.end();

  visiting_inst_processed = false;

  return {visiting, chokep};
}

PathElemBBs BB_BFS_Q::currentPath() {
  PathElemBBs path;
  llvm::BasicBlock *parent;

  if (visiting == nullptr) {
    return {};
  }

  // NOTE: We can only yielded at a choke point.
  AttributeMap &csum = get_callee_summary(visiting);
  if (yielded) {
    // tuple of basic block and instruction idx yielded at or None
    path = {{visiting, yield_at_inst_idx, csum}};
  } else {
    path = {{visiting, -1, csum}};
  }

  ParentsIter parentIter = parents.find(visiting);
  while (parentIter != parents.end()) {
    llvm::BasicBlock *parent = parentIter->second;
    // It is intended the call summary is copied into the PathElemBB
    path.push_back({parent, -1, get_callee_summary(parent)});
    parentIter = parents.find(parent);
  }
  path.reverse();
  return path;
}

BB_BFS_Q_ExpandResult BB_BFS_Q::expand() {
  BB_BFS_Q_ExpandedBBs expanded;
  if (finished()) {
    return {expandresult_expanded, std::any(nullptr), std::any(expanded)};
  }

  BBInstIter future_work;
  if (yielded) {
    if (debug) {
      spdlog::debug("  BB_BFS_Q: expand(): RESUME visiting: \n{}", *visiting);
    }

    // We're restarting from a previous yield.
    if (not yielded_future.has_value()) {
      spdlog::error("{}: There is supposed to be a yielded iterator here, but "
                    "there isn't!",
                    __func__);
      std::exit(EXIT_FAILURE);
    }
    future_work = yielded_future.value();
    yielded = false;
    yielded_future.reset();
  } else {
    if (yielded_future.has_value()) {
      spdlog::error("Can't have a yielded future while not yielded!");
      std::exit(EXIT_FAILURE);
    }

    if (debug) {
      spdlog::debug("  BB_BFS_Q: expand(): START visiting: \n{}",
                    getBBName(visiting));
    }

    // insts
    // In the C++, prolly store this as the concrete list I'm
    // processing.
    future_work = visiting->begin();
    yield_at_callee_func = nullptr;
    yield_at_inst_idx = -1;
  }

  // We can only yield if we find a participating function we want to call
  // in the instruction stream of the basic block.

  // TODO: The current feature set is implemented such that we can only
  // visit() at the start basic block. A basic block that is also
  // a choke point in which we did recurse, can not yet be split
  // into visitations of the basic block _at an instruction index_.

  // TODO: (DO LATER) It seems like to me, that when we visit what would
  // normally be a chokepoint, we actually secret it away, then visit
  // EVERYTHING else until we either reach another choke point (which is a
  // multi-neck situation!) or the open list becomes empty, in
  // which case, visiting the secreted away choke point allows us to
  // expand into the next part of the computation on the other side of the
  // choke point. The reason for this feature is we can both ensure that
  // all the aggregating info from functions calls in basic blocks are up
  // to date, but also we can detect multi-neck situations by enountering
  // a choke point in the queue when we alreasy have one (or more)
  // deferred ones.

  ParticipatingFunctionsSet &p = NA.getParticipatingFunctions();

  int idx = yield_at_inst_idx;
  while (future_work != visiting->end()) {
    auto &inst = *future_work;
    idx += 1;
    future_work = std::next(future_work);

    if (!llvm::isa<llvm::CallBase>(inst)) {
      // Skip instructions which are not direct function calls
      continue;
    }

    auto *call = llvm::dyn_cast<llvm::CallBase>(&inst);
    llvm::Function *callee = call->getCalledFunction();
    if (!callee) {
      // Skip indirect function calls
      continue;
    }
    if (p.count(callee) < 1) {
      // Skip non-partitipating functions.

      // if (debug) {
      //  llvm::outs() << "  BB_BFS_Q: expand(): NO_YIELD at F["
      //               << visiting->getParent()->getName() << "]@{"
      //               << getBBName(visiting) << "}:[" << idx << "] \n "
      //               << inst << "\n";
      //}
      continue;
    }

    // We're about to enter into a new function so save state
    // and effect the yield.
    yield_at_visiting = visiting;
    yield_at_callee_func = callee;
    yield_at_inst_idx = idx;
    yielded = true;

    // The enumerator object stores its own recovery state,
    // so we can just pick it back up later.
    yielded_future.emplace(future_work);
    if (debug) {
      spdlog::debug("  BB_BFS_Q: expand(): YIELD at F[{}]@{{}}:[{}] \n    {}",
                    visiting->getParent()->getName(), getBBName(visiting), idx,
                    inst);
    }
    return {expandresult_yield, std::any(callee), std::any(idx)};
  }

  if (visiting_inst_processed) {
    spdlog::error("Attempted to expand when already visited the instructions!");
    std::exit(EXIT_FAILURE);
  }
  visiting_inst_processed = true;

  // Now finally, when we can expand the visiting node into its children.
  if (!llvm::succ_empty(visiting)) {
    for (auto itr = llvm::succ_begin(visiting); itr != llvm::succ_end(visiting);
         ++itr) {
      llvm::BasicBlock *succ = *itr;
      if (observed.count(succ) > 0) {
        continue;
      }
      observed.insert(succ);
      parents[succ] = visiting;
      queue.push_back(succ);
      expanded.push_back(succ);
    }
  } else {
    // Save off in BFS order the current exit path to be summarized
    // later. NOTE: This path is _a_ path to this basic block, not
    // all paths to this no successor basic block.
    //
    // TODO: We probably want all paths from here back to the front
    // if the function and add all of those paths as exit paths.
    // This needs thinking about since capturing the path at the right
    // time is critical to make it work.
    //
    // TODO ensure copying of current_path is valid
    exit_paths.push_back(currentPath());
  }
  if (debug) {
    spdlog::debug("  BB_BFS_Q: expand(): END visiting: {}"
                  ", callee summary: TODO print get_callee_summary(visiting)",
                  getBBName(visiting));
  }
  return {expandresult_expanded, std::any(visiting), std::any(expanded)};
}

llvm::Function *BB_BFS_Q::getFunction() { return func; }

bool BB_BFS_Q::getChokePointForbidden() { return choke_point_forbidden; }

bool BB_BFS_Q::getYielded() { return yielded; }

llvm::Function *BB_BFS_Q::getYieldAtCalleeFunc() {
  return yield_at_callee_func;
}

int BB_BFS_Q::getYieldAtInstIdx() { return yield_at_inst_idx; }

bool BB_BFS_Q::getChokeP() { return chokep; }

} // namespace neckid
