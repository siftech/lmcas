#ifndef NECKID_NECKID_NECKANALYSIS_H
#define NECKID_NECKID_NECKANALYSIS_H

/******************************************************************************
 * Copyright (c) 2021 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

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

#include "NeckID/NeckID/BB_BFS_Q.h"
#include "NeckID/NeckID/NeckSearch.h"
#include "NeckID/NeckID/NeckSearchTypes.h"
#include "NeckID/NeckID/NeckUtils.h"
#include "NeckID/NeckID/TaintAnalysis.h"

#include "GuiNeSS/Tape.h"

template <typename MapT, typename Predicate>
std::size_t eraseIf(MapT &C, Predicate Pred) {
  auto OldSize = C.size();
  // TODO: This loop seems sketchy, should it follow more like how this
  // c++11 loop works?
  // https://stackoverflow.com/questions/2874441/deleting-elements-from-stdset-while-iterating
  // Namely, the Last variable seems like it could be a reference to something
  // that gets deleted half way through the loop.
  for (auto It = C.begin(), Last = C.end(); It != Last;) {
    if (Pred(*It)) {
      It = C.erase(It);
    } else {
      ++It;
    }
  }
  return OldSize - C.size();
}

namespace neckid {

using NeckMinerResults = std::tuple<llvm::BasicBlock *, uint32_t>;
using NeckMinerOutput = std::tuple<const std::string, const std::string,
                                   const uint64_t, const uint32_t>;

/// Types for the Boost directed graph representation of the LLVM CFG so we can
/// get articulation points out of boost.

struct VertexProps {
  llvm::BasicBlock *bb;
};

/*
// just a reminder to me for how to specify this if I need it
using EdgeWeightProperty  =
    boost::property<boost::edge_weight_t, int>;
*/

// An undirected graph without parallel edges needed for articulation point
// computation. Constructed from the LLVM CFG.
using BCFGGraph = boost::adjacency_list<
    // OutEdgeList
    boost::vecS,

    // VertexList
    boost::vecS,

    // Directed
    boost::undirectedS,

    // VertexProperties
    VertexProps,

    // EdgeProperties
    // EdgeWeightProperty,
    boost::no_property,

    // GraphProperties
    boost::disallow_parallel_edge_tag,

    // EdgeList
    boost::listS>;

using bcfg_edge_iterator = boost::graph_traits<BCFGGraph>::edge_iterator;

using bcfg_edge_descriptor = boost::graph_traits<BCFGGraph>::edge_descriptor;

using bcfg_vertex_descriptor =
    boost::graph_traits<BCFGGraph>::vertex_descriptor;

class NeckAnalysis {
private:
  llvm::Module &M;
  TaintAnalysis TA;
  // Dominator trees and loop infos are constructed lazily. Use
  // getDominatorTree() and getLoopInfo() to access them.
  std::unordered_map<llvm::Function *, llvm::DominatorTree> DTs;
  std::unordered_map<llvm::Function *, llvm::LoopInfo> LIs;
  std::unordered_map<llvm::BasicBlock *, std::unordered_set<llvm::BasicBlock *>>
      LoopPassthroughMap;
  std::unordered_set<llvm::BasicBlock *> NeckCandidates;
  std::unordered_set<llvm::BasicBlock *> ArticulationPoints;
  std::unordered_set<llvm::BasicBlock *> TaintedBasicBlocks;
  std::unordered_set<llvm::Function *> TaintedFunctions;
  std::unordered_map<llvm::Function *, std::unordered_set<llvm::Function *>>
      TaintedCalleeToCallerMap;
  std::unordered_set<llvm::BasicBlock *> UserBranchAndCompInstructions;
  std::unordered_set<llvm::BasicBlock *> GetOptAndFriends;
  std::unordered_map<llvm::BasicBlock *, std::deque<std::string>>
      PassAnnotations;
  std::unordered_map<int, std::string> PassDescriptions;
  std::unordered_map<llvm::Function *, AttributeMap> ForbiddenMemoCache;

  llvm::BasicBlock *NeckBasicBlock;
  uint32_t NeckInstructionIndex;
  BB_BFS_Q_Path NeckPath;
  llvm::Function *Main;
  [[maybe_unused]] bool Debug;

  std::optional<std::string> CombinedModulePath;
  std::optional<std::string> GuiNeSSTapePath;

  std::optional<const NeckMinerOutput> FinalOutput;

  int NumCallsToGAF;
  std::unordered_map<llvm::Function *, std::unordered_set<llvm::BasicBlock *>>
      FunctionToGetOptAndFriendsMap;
  std::unordered_set<llvm::BasicBlock *> ChokePoints;
  std::unordered_map<llvm::BasicBlock *, AttributeMap> BB_AttrMaps;

  // A set of functions that contain tainted blocks or that control
  // passed through in order to get to a tainted block.
  ParticipatingFunctionsSet ParticipatingFunctions;
  ParticipatingBBsSet ParticipatingBBs;

  // A loop is considered tainted if any of the basic blocks it contains
  // are also tainted. We only insert the loop head into this set
  // to represent that a loop is tainted.
  std::unordered_set<llvm::BasicBlock *> TaintedLoops;

public:
  /// Computes neck candidates and the definitive neck.
  NeckAnalysis(llvm::Module &M, const std::string &TaintConfigPath,
               bool FunctionLocalPTAwoGlobals = false,
               bool UseSimplifiedDFA = false,
               const std::string &FunctionName = "main",
               const std::optional<std::string> &CombinedPath = std::nullopt,
               const std::optional<std::string> &TapePath = std::nullopt,
               bool Debug = false);

  /// Generally the original algorithm, with some changeds and a parallel
  /// version we've been writing.
  bool theOriginalAlgorithm(int &passID);

  /// Where we cut over to the new algorithm, entail, and extend
  /// the original algorithm.
  bool theNewAlgorithm(int &passID);

  // ------------------------------------------------------------------
  /// Duplicate any leaf BBs in the function if possible. Return true
  /// if any BBs were duplicated and false of none were.
  bool duplicateTailBasicBlocks(llvm::Function *func, int passID,
                                int &tot_shared, int &tot_sharedPred,
                                int &tot_duplicated);

  bool erodeTailBasicBlocks(llvm::Function *func, int passID, int &tot_eroded);

  void dumpTaintedCalleeToCallerGraph();
  void passConstructParticipatingFunctionsFromTaintedFunctions(int &passID);
  void passCollectAllParticipatingBBs(int &passID);

  /// For each participating function, duplicate all IR tails so they
  /// have a single parent. This makes finding articulation points in
  /// that function be more productive.
  void passTailDuplicateParticipatingFunctions(int &passID);

  /// Acquire all tainted blocks from TaintAnalysis
  void passAcquireAllTaintedBasicBlocks(int &passID);

  /// Acquire all tainted functions from TaintAnalysis
  void passAcquireAllTaintedFunctions(int &passID);

  /// Select initial set of Neck Candidates from the Tainted Block set.
  /// Return false of no neck candidates from the Taint Analysis pass
  /// are found. True otherwise.
  bool passInitializeNeckCandidatesFromTaintAnalysis(int &passID);

  /// Any Neck Candidates inside of loops are pushed to the loop exit blocks.
  void passFlowNeckCandidatesInLoopsToEndOfLoop(int &passID);

  /// If the basic block is a choke point and has a single instruction
  /// that is a branch to a single successor that is also a choke point,
  /// then return true.
  bool isFlowableBookKeepingBlock(llvm::BasicBlock *bb);

  // Similar to passFlowNeckCandidatesThroughUnconditionalBranches(),
  // this iteratively pushes the neck candidates on choke points, which
  // are also book keeping blocks, into the future to new choke points
  void passFlowNeckCandidatesThroughBookKeepingBBs(int &passID);

  /// For each neck candidate, if it is in an already choke point
  /// basic block leave it there. Otherwise search into the future of
  /// ONLY the function containing that neck candidate until either
  /// a set of choke points are found, in which case they all become
  /// neck candidates, or no choke points are found, in which case the
  /// NeckCandidate is dropped.
  void passPushNeckCandidatesToNearestFutureChokePointHorizon(int &passID);
  // Helper for the above.
  void pushNeckToFutureChokePointHorizon(
      llvm::BasicBlock *neck,
      std::unordered_set<llvm::BasicBlock *> &chokePointHorizon);

  /// WARNING: Experimental pass to extract articulation points from ALL
  /// functional participating in the TaintedBasicBlocks, or in control paths
  /// to the tainted blocks.
  void passExtractAllArticulationPoints(int &passID);

  /// WARNING: Experimental pass to extract articulation points from a
  /// single function. Helper to passExtractAllArticulationPoints().
  void extractArticulationPointsFromFunc(
      llvm::Function *func, BCFGGraph &undCFG,
      std::unordered_set<llvm::BasicBlock *> &artPoints);

  /// WARNING: Experimental pass to select articulation points that are
  /// also choke-points. Choke points are articulation points that dominate
  /// their entire descendants.
  void passSelectChokePoints(int &passID);

  // FIXME this pass probably should not be necessary but loop heads
  // are been marked as choke points. There may be some mismatch in
  // our description of choke points that needs to be updated to
  // prevent loop heads from being marked
  void passRemoveLoopChokePoints(int &passID);

  /// NOTE: The formal semantics of this pass are a little unclear, but
  /// the idea is that we want to find necks after loops that use tainted
  /// data. Of course, this will not be perfect: (a very early loop that
  /// does argv duplication and nothing else is an example.
  ///
  /// If a loop in a participating function uses tainted data in
  /// any the basic blocks contained in that loop, consider the
  /// entire loop tainted and put the loop head into a
  /// "tainted loop head" set.
  void passMarkTaintedLoops(int &passID);

  void passCollectLoopPassthroughMap(int &passID);
  void passCollectGetOptAndFriendsMap(int &passID);
  void passCollectAllBasicBlockAttributes(int &passID);

  void passExecuteNeckSearch(int &passID);

  /// Enqueue the annotation into the BB's associated annotation queue.
  void addAnnotation(llvm::BasicBlock *BB, std::string ann);

  /// Dump the PassAnnotations so we can see them.
  void dumpPassAnnotations();

  /// Record the pass into the pass description table and emit a message.
  void initPass(int passID, std::string passDesc);

  // Record that the pass finished and update the passID.
  void finiPass(int &passID);

  /// Collect all loop exit BBs from NeckCandidates that are inside of loops.
  void collectLoopExitsOfNeckCandidates(
      std::unordered_set<llvm::BasicBlock *> &LoopBBs);

  /// Create a copy of the Src's parent Function's CFG into boost's BGL.
  void copyFunctionCFGIntoBoostGraph(llvm::BasicBlock *Src, BCFGGraph &G);

  /// TODO: lift into a standalone util API
  void findAllDescendants(llvm::BasicBlock *Src,
                          std::unordered_set<llvm::BasicBlock *> &Descendants);

  /// TODO: lift into a standalone util API
  void
  findCommonDescendants(const std::unordered_set<llvm::BasicBlock *> &Ancestors,
                        std::unordered_set<llvm::BasicBlock *> &Descendants);

  /// Get cached dominator tree for the specified function.
  llvm::DominatorTree &getDominatorTree(llvm::Function *F);

  /// Get cached dominator tree for the specified basic block's function.
  inline llvm::DominatorTree &getDominatorTree(llvm::BasicBlock *BB) {
    return getDominatorTree(BB->getParent());
  }

  /// When the CFG is modified for a function, ensure to refresh the
  /// memoization caches for the dominator tree and loop info.
  void refreshFunctionCaches(llvm::Function *func);

  /// Get cached loop info for the specified function.
  llvm::LoopInfo &getLoopInfo(llvm::Function *F);

  /// Get cached loop info for the specified basic block's function.
  inline llvm::LoopInfo &getLoopInfo(llvm::BasicBlock *BB) {
    return getLoopInfo(BB->getParent());
  }

  // Retrieves all reachable callsites within a basic block.
  std::unordered_set<llvm::Instruction *>
  getReachableCallSites(llvm::BasicBlock *Src);

  std::unordered_set<llvm::BasicBlock *>
  getLoopExitBlocks(llvm::BasicBlock *BB);

  llvm::BasicBlock *closestNeckCandidateReachableFromEntry();

  bool isInLoopStructue(llvm::BasicBlock *BB);

  /// TODO: lift into a standalone util API
  bool isFullDominator(llvm::BasicBlock *BB, const llvm::DominatorTree *DT);

  bool succeedsLoop(llvm::BasicBlock *BB);

  /// Returns the analyzed module.
  [[nodiscard]] llvm::Module &getModule();

  /// Returns the set of neck candidate.
  [[nodiscard]] std::unordered_set<llvm::BasicBlock *> getNeckCandidates();

  /// Returns a set of basic blocks that were tainted
  [[nodiscard]] std::unordered_set<llvm::BasicBlock *> getTaintedBasicBlocks();

  /// Returns a set of basic blocks that are articulation points
  [[nodiscard]] std::unordered_set<llvm::BasicBlock *> getArticulationPoints();

  /// Returns a set of basic blocks that are Choke Points
  [[nodiscard]] std::unordered_set<llvm::BasicBlock *> getChokePoints();

  /// Returns a map of Choke Points to ChokePointInfo
  [[nodiscard]] std::unordered_map<llvm::BasicBlock *, AttributeMap> &
  getBasicBlockAttributeMap();

  /// Returns a set of llvm::Function* representing the participating function
  [[nodiscard]] ParticipatingFunctionsSet &getParticipatingFunctions();

  /// Returns a ref to an ordered map of function pointers to AttributeMaps
  /// that represent memoized version of the BFS search of that function.

  [[nodiscard]] std::unordered_map<llvm::Function *, AttributeMap> &
  getForbiddenMemoCache();

  /// Returns a set of basic blocks that contain a comparison/branch instruction
  /// that is using a tainted value.
  [[nodiscard]] std::unordered_set<llvm::BasicBlock *>
  getUserBranchAndCompInstructions();

  std::unordered_map<llvm::BasicBlock *, std::deque<std::string>>
  getPassAnnotations();

  /// Returns the definite neck or nullptr, if no neck could be found.
  [[nodiscard]] NeckMinerResults getNeck();
  [[nodiscard]] BB_BFS_Q_Path &getNeckPath();
  [[nodiscard]] std::optional<const NeckMinerOutput> getOutput();

  /// Dumps the underlying module to the output stream.
  void dumpModule(llvm::raw_ostream &OS = llvm::outs());

  /// ////////////////////////////////////////////////////////////////
  /// WARNING: The collection of methods below are crufty and do not need
  /// code review. It is highly likely we'll remove them.
  /// ////////////////////////////////////////////////////////////////
  ///
  /// Compute the neck candidate that is closed to the program's entry point
  void passPickClosestNeckCandidate(int &passID);

  /// Remove all Neck Candidates that preceed the last getopt loop
  /// if getopt and friends are invoked at all. If there is no getopt
  /// loop, so nothing.
  void passRemoveNeckCandidatesThatPreceedLastGetoptLoop(int &passID);

  /// Remove all basic blocks that are not reachable from main
  void passRemoveNeckCandidatesNotReachableFromMain(int &passID);

  /// Remove all basic blocks that do not succeed a loop
  void passRemoveNeckCandidatesThatDoNotSucceedALoop(int &passID);

  /// Remove all basic blocks that do not dominate their successors.
  void passRemoveNeckCandidatesThatDontDominateSuccessors(int &passID);

  /// In the LLVM IR, it is the case that sometimes there are BBs that
  /// contain a single unconditional jump to the next BB. This code
  /// flows the NeckCandidate through such a portion of the IR graph.
  /// TODO: No, it actually doesn't! It just does a single step. FIXME.
  void passFlowNeckCandidatesThroughUnconditionalBranches(int &passID);

  /// Find any BB's that call C's:
  /// getopt() / getopt_long() / getop_long_only()
  /// or anything else that looks getopt-like.
  void passAnnotateGetoptAndFriends(int &passID);

  /// Any reachable neck candidates from main() are contracted into
  /// main().
  void passContractIntraproceduralNeckCandidatesIntoMain(int &passID);

  /// Breadth-first search starting from main's entry point.
  bool isReachableFromFunctionsEntry(llvm::BasicBlock *Dst,
                                     llvm::StringRef FunName);

  /// Breadth-first search starting from main's entry point.
  bool isReachableFromFunctionsEntry(llvm::BasicBlock *Dst,
                                     llvm::Function *Fun);

  /// Breadth-first search.
  bool isReachable(llvm::BasicBlock *Src, llvm::BasicBlock *Dst,
                   bool InterProcSearch);

  /// Breadth-first search that computes distance.
  bool isReachable(llvm::BasicBlock *Src, llvm::BasicBlock *Dst, size_t &Dist,
                   bool InterProcSearch);

  /// Breadth-first search that computes distance.
  bool isReachable(llvm::BasicBlock *Src, llvm::BasicBlock *Dst, size_t &Dist,
                   bool InterProcSearch,
                   llvm::BasicBlock **CallSiteBB = nullptr);

  bool hasSingleSuccessorThatDominatesSucessors(llvm::BasicBlock *BB);
  bool dominatesSuccessors(llvm::BasicBlock *BB);

  /// TODO: lift into a standalone util API
  static bool isBackEdge(llvm::BasicBlock *From, llvm::BasicBlock *To,
                         const llvm::DominatorTree *DT);

  /// Inserts a special function call to '' to denote the basic block that has
  /// been identified as the neck.
  void markNeck(const std::string &FunName = "_lmcas_neck");
};

} // namespace neckid

#endif
