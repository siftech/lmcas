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
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FmtLlvm.h"
#include <spdlog/spdlog.h>

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

#include "NeckID/NeckID/Annotation.h"
#include "NeckID/NeckID/BB_BFS_Q.h"
#include "NeckID/NeckID/NeckAnalysis.h"
#include "NeckID/NeckID/TaintAnalysis.h"

#include "GuiNeSS/Annotation.h"
#include "GuiNeSS/GuiNeSS.h"
#include "GuiNeSS/Tape.h"
#include "GuiNeSS/TapeWalker.h"

namespace {} // anonymous namespace

namespace neckid {

llvm::DominatorTree &NeckAnalysis::getDominatorTree(llvm::Function *F) {
  auto Search = DTs.find(F);
  if (Search != DTs.end()) {
    return Search->second;
  }
  DTs[F] = llvm::DominatorTree(*F);
  return DTs[F];
}

llvm::LoopInfo &NeckAnalysis::getLoopInfo(llvm::Function *F) {
  auto Search = LIs.find(F);
  if (Search != LIs.end()) {
    return Search->second;
  }
  auto &DT = getDominatorTree(F);
  LIs[F] = llvm::LoopInfo(DT);
  return LIs[F];
}

void NeckAnalysis::refreshFunctionCaches(llvm::Function *F) {
  DTs[F] = llvm::DominatorTree(*F);
  LIs[F] = llvm::LoopInfo(DTs[F]);
}

std::unordered_set<llvm::BasicBlock *>
NeckAnalysis::getLoopExitBlocks(llvm::BasicBlock *BB) {
  auto *Loop = getLoopInfo(BB).getLoopFor(BB);
  if (Loop) {
    llvm::SmallVector<llvm::BasicBlock *, 5> Exits;
    // Return all unique successor blocks of this loop.
    // These are the blocks outside of the current loop which are
    // branched to.
    Loop->getUniqueExitBlocks(Exits);
    std::unordered_set<llvm::BasicBlock *> Result;
    Result.insert(Exits.begin(), Exits.end());
    return Result;
  }
  return {};
}

bool NeckAnalysis::isInLoopStructue(llvm::BasicBlock *BB) {
  return getLoopInfo(BB).getLoopFor(BB) != nullptr;
}

// SOURCE: https://llvm.org/doxygen/SanitizerCoverage_8cpp_source.html#l00516
// True if block has successors and it dominates all of them.
// TODO: If the BB is a choke point and also a loop head,
// it can be errnonously marked as full(strict) dominating its
// descendents. This needs to be fixed (though in the algorithm
// elsewhere we have a workaround for this error).
bool NeckAnalysis::isFullDominator(llvm::BasicBlock *BB,
                                   const llvm::DominatorTree *DT) {

  std::unordered_set<llvm::BasicBlock *> Descendants;

  if (succ_empty(BB)) {
    // TODO: Technically, if there are no successors, then since a node
    // already dominates itself, it should also be a full dominator. Look
    // at this soon....
    return false;
  }

  findAllDescendants(BB, Descendants);

  return llvm::all_of(Descendants, [&](const llvm::BasicBlock *SUCC) {
    return DT->dominates(BB, SUCC);
  });
}

void NeckAnalysis::findAllDescendants(
    llvm::BasicBlock *Src,
    std::unordered_set<llvm::BasicBlock *> &Descendants) {
  Descendants.clear();
  std::queue<llvm::BasicBlock *> Queue;
  std::unordered_map<llvm::BasicBlock *, bool> Visited;
  for (llvm::BasicBlock &BB : *Src->getParent()) {
    Visited[&BB] = false;
  }
  Visited[Src] = true;
  Queue.push(Src);
  while (!Queue.empty()) {
    llvm::BasicBlock *Current = Queue.front();
    Queue.pop();
    for (llvm::BasicBlock *Child : llvm::successors(Current)) {
      if (Visited[Child]) {
        continue;
      }
      Visited[Child] = true;
      Queue.push(Child);
      Descendants.insert(Child);
    }
  }
}

// TODO this is probably quite slow and can almost certainly be optimized
void NeckAnalysis::findCommonDescendants(
    const std::unordered_set<llvm::BasicBlock *> &Ancestors,
    std::unordered_set<llvm::BasicBlock *> &Descendants) {
  std::unordered_set<llvm::BasicBlock *> CurrentDescendants;
  std::unordered_set<llvm::BasicBlock *> RunningIntersection;

  bool FirstSet = true;
  for (llvm::BasicBlock *Ancestor : Ancestors) {
    findAllDescendants(Ancestor, CurrentDescendants);

    // Seed the initial set
    if (FirstSet) {
      FirstSet = false;
      for (llvm::BasicBlock *Desc : CurrentDescendants) {
        RunningIntersection.insert(Desc);
      }
      continue;
    }

    // determine the loop set and test set
    std::unordered_set<llvm::BasicBlock *> *LoopSet;
    std::unordered_set<llvm::BasicBlock *> *TestSet;
    if (CurrentDescendants.size() <= RunningIntersection.size()) {
      LoopSet = &CurrentDescendants;
      TestSet = &RunningIntersection;
    } else {
      LoopSet = &RunningIntersection;
      TestSet = &CurrentDescendants;
    }

    // determine the BBs in LoopSet that are also in TestSet
    std::unordered_set<llvm::BasicBlock *> Intersection;
    for (llvm::BasicBlock *BB : *LoopSet) {
      if (TestSet->count(BB) > 0) {
        Intersection.insert(BB);
      }
    }
    RunningIntersection.swap(Intersection);
  }

  Descendants.clear();
  for (llvm::BasicBlock *BB : RunningIntersection) {
    Descendants.insert(BB);
  }
}

void NeckAnalysis::addAnnotation(llvm::BasicBlock *BB, std::string ann) {
  std::deque<std::string> &q = PassAnnotations[BB];
  q.push_back(ann);
}

void NeckAnalysis::dumpPassAnnotations() {
  using pd_size_type =
      std::unordered_map<int, std::basic_string<char>>::size_type;

  spdlog::info("Dumping Basic Block Pass Assert/Refute Annotations:");

  // NOTE: passID always stsart from zero and increment by one to a
  // maximum number, in this case, the size of how many entries there were
  // in the PassDescriptions hash table.
  spdlog::info(" Pass ID Descriptions:");
  for (pd_size_type passID = 0; passID < PassDescriptions.size(); passID++) {
    std::string &passName = PassDescriptions[passID];
    spdlog::info("  {} : {}", passID, passName);
  }

  spdlog::info(" Basic Block Pass Annotations:");
  for (auto kv : PassAnnotations) {
    llvm::BasicBlock *BB = kv.first;
    std::deque<std::string> &anns = kv.second;
    llvm::Function *func = BB->getParent();
    llvm::StringRef funcName = getSafeName(func);

    std::stringstream annotationStr;
    for (auto ann : anns) {
      annotationStr << ann << " ";
    }
    spdlog::info("  {}:{} [{}] -> {}", funcName, getBBIdsString(BB), *BB,
                 annotationStr.str());
  }
  spdlog::info("  End.");
}

void NeckAnalysis::initPass(int passID, std::string passDesc) {
  spdlog::info("PASS {}: {}", passID, passDesc);
  PassDescriptions.insert({passID, passDesc});
}

void NeckAnalysis::finiPass(int &passID) {
  spdlog::debug("---");
  passID++;
}

/// Pass: Compute initial neck candidates from data-flow analysis.
/// Return True if NeckCandidates were found.
void NeckAnalysis::passAcquireAllTaintedBasicBlocks(int &passID) {
  int tb_before_pass = TaintedBasicBlocks.size();
  int tb_after_pass = 0;

  initPass(passID, __FUNCTION__);

  // Start with a list of potential neck candidates
  std::vector<llvm::Instruction *> InterestingInstructions =
      TA.getNeckCandidates();

  UserBranchAndCompInstructions = TA.getUserBranchAndCompInstructions();

  // Initialize with potential neck candidates
  for (auto *InterestingInstruction : InterestingInstructions) {
    auto *BB = InterestingInstruction->getParent();
    auto result = TaintedBasicBlocks.insert(BB);
    if (result.second) {
      addAnnotation(BB, makeAnnotation(ASSERT_TAINT, passID));
    }
  }
  tb_after_pass = TaintedBasicBlocks.size();

  spdlog::info("  Tainted Basic Blocks before pass: {}\n"
               "  Interesting Instructions from TA: {}\n"
               "  Tainted Basic Blocks after pass: {}",
               tb_before_pass, InterestingInstructions.size(), tb_after_pass);

  finiPass(passID);
}

/// Pass: Compute initial neck candidates from data-flow analysis.
/// Return True if NeckCandidates were found.
void NeckAnalysis::passAcquireAllTaintedFunctions(int &passID) {
  int tf_before_pass = TaintedFunctions.size();
  int tf_after_pass = 0;

  initPass(passID, __FUNCTION__);

  for (llvm::BasicBlock *BB : TaintedBasicBlocks) {
    TaintedFunctions.insert(BB->getParent());
  }

  tf_after_pass = TaintedFunctions.size();

  spdlog::info("  Tainted Functions before pass: {}\n"
               "  Tainted Functions after pass: {}",
               tf_before_pass, tf_after_pass);

  finiPass(passID);
}

/// Pass: Compute the initial set of Neck Candidates from the TaintedBlocks.
/// Currently, this is simply a 1:1 transfer. Each Tainted Block starts out
/// as a Neck Candidate.
bool NeckAnalysis::passInitializeNeckCandidatesFromTaintAnalysis(int &passID) {
  int nc_before_pass = NeckCandidates.size();
  int nc_after_pass = 0;

  initPass(passID, __FUNCTION__);

  NeckCandidates.clear();

  // Initialize with potential neck candidates
  for (auto *TB : TaintedBasicBlocks) {
    auto result = NeckCandidates.insert(TB);
    if (result.second) {
      addAnnotation(TB, makeAnnotation(ASSERT_BOUNDARY, passID));
    }
  }
  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Tainted Basic Blocks: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, TaintedBasicBlocks.size(), nc_after_pass);

  if (Debug) {
    spdlog::debug("Neck candidates found:");
    spdlog::debug(getBBListAsString(NeckCandidates));
  }

  finiPass(passID);
  return !NeckCandidates.empty();
}

/// Fill in LoopBBs with exit blocks of any loops that any neck candidates
/// are currently in.
void NeckAnalysis::collectLoopExitsOfNeckCandidates(
    std::unordered_set<llvm::BasicBlock *> &LoopBBs) {
  for (auto *NeckCandidate : NeckCandidates) {
    if (isInLoopStructue(NeckCandidate)) {
      auto ExitBlocks = getLoopExitBlocks(NeckCandidate);
      LoopBBs.insert(ExitBlocks.begin(), ExitBlocks.end());
    }
  }
}

/// Any neck candidates inside of a loop or on the loop head are
/// refuted and instead the loop exits asserted.
void NeckAnalysis::passFlowNeckCandidatesInLoopsToEndOfLoop(int &passID) {
  int nc_before_pass = NeckCandidates.size();
  int nc_after_pass = 0;
  int nc_in_loop = 0;
  int nc_not_in_loop = 0;
  int nc_loop_exits = 0;

  initPass(passID, __FUNCTION__);

  // Collect all neck candidates that are part of a loop add Loop Exits to
  // LoopBBs
  std::unordered_set<llvm::BasicBlock *> LoopBBs;
  collectLoopExitsOfNeckCandidates(LoopBBs);
  nc_loop_exits = LoopBBs.size();

  // Remove all basic blocks that are part of a loop (also removes loop exits)
  eraseIf(NeckCandidates,
          [this, &nc_in_loop, &nc_not_in_loop, passID](auto *BB) {
            bool ils = isInLoopStructue(BB);
            ils ? nc_in_loop++ : nc_not_in_loop++;
            if (ils) {
              addAnnotation(BB, makeAnnotation(REFUTE_BOUNDARY, passID));
            }
            return ils;
          });

  // Add the loops' respective exit blocks instead
  for (auto LoopBB : LoopBBs) {
    auto ret = NeckCandidates.insert(LoopBB);
    if (ret.second) {
      addAnnotation(LoopBB, makeAnnotation(ASSERT_BOUNDARY, passID));
    }
  }

  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Neck Candidates in a loop (flowing to Loop Exits): {}\n"
               "  Neck Candidates not in a loop: {}\n"
               "  Loop exits which are now Neck Candidates: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, nc_in_loop, nc_not_in_loop, nc_loop_exits,
               nc_after_pass);

  if (Debug) {
    spdlog::debug("  Neck candidates after handling loops:");
    spdlog::debug(getBBListAsString(NeckCandidates));
  }

  finiPass(passID);
}

/// Return true if the bb basic block follows the pattern of a
/// "bookeeping block" which, so far, means a choke point with a single
/// choke point successor and a single instruction that jmps to it.
/// If we start detecting more kinds of book keeping
/// blocks the flow in this function will prolly have to change.
bool NeckAnalysis::isFlowableBookKeepingBlock(llvm::BasicBlock *bb) {
  bool has_single_jmp_inst = false;
  bool is_choke_point = false;
  llvm::BasicBlock *succ = nullptr;
  bool single_successor_is_chokepoint = false;

  // TODO: This prolly has to get a little smarter to deal with
  // degenerate PHI nodes and a singe branch terminator, but I'll
  // do that later.
  has_single_jmp_inst =
      bb->size() == 1 &&
      (llvm::dyn_cast<llvm::BranchInst>(bb->getTerminator()) != nullptr);

  is_choke_point = ChokePoints.count(bb) > 0;

  succ = bb->getSingleSuccessor();

  single_successor_is_chokepoint =
      succ != nullptr && ChokePoints.count(succ) > 0;

  return has_single_jmp_inst && is_choke_point &&
         single_successor_is_chokepoint;
}

/// Perform an iterative algorithm that keeps pushing a neck candidate
/// through book keeping blocks until it can't. This chooses the latest
/// possible choke point still within the contiguous sequence of
/// bookeeping basic blocks.
void NeckAnalysis::passFlowNeckCandidatesThroughBookKeepingBBs(int &passID) {
  bool pushing = true;
  int num_initial_flowable_ncs = 0;
  int num_flow_iters = 0;
  int num_flow_operations = 0;
  bool record_once = true;
  llvm::BasicBlock *succ = nullptr;
  std::unordered_set<llvm::BasicBlock *> bookKeepingBlocks;

  initPass(passID, __FUNCTION__);

  while (pushing) {
    bookKeepingBlocks.clear();
    pushing = false;

    // 1. Find all neck candidates that are suitable book
    // keeping blocks (which are flowable).
    for (auto *NeckCandidate : NeckCandidates) {
      if (isFlowableBookKeepingBlock(NeckCandidate)) {
        auto ret = bookKeepingBlocks.insert(NeckCandidate);
        if (ret.second) {
          pushing = true;
          if (record_once) {
            num_initial_flowable_ncs++;
          }
        }
      }
    }
    record_once = false;

    // 2. Push each neck candidate forward to the next immediate
    // choke point. NOTE: the previous use of the
    // isFlowableBookKeepingBlock() function allows us to peform
    // the flow unconditionally here.
    for (auto *pNC : bookKeepingBlocks) {
      NeckCandidates.erase(pNC);
      addAnnotation(pNC, makeAnnotation(REFUTE_BOUNDARY, passID));

      succ = pNC->getSingleSuccessor();
      NeckCandidates.insert(succ);
      addAnnotation(succ, makeAnnotation(ASSERT_BOUNDARY, passID));
      num_flow_operations++;
    }

    if (num_flow_iters > 100) {
      spdlog::error("{}: Programmer Error! It took too many iterations "
                    "to perform this pass. There is a problem!",
                    __func__);
      std::exit(EXIT_FAILURE);
    }

    num_flow_iters++;
  }

  spdlog::info("  Flowable Book Keeping Neck Candidates : {}\n"
               "  Total Flow Iterations: {}\n"
               "  Cumulative Flow Operations: {}",
               num_initial_flowable_ncs, num_flow_iters, num_flow_operations);

  finiPass(passID);
}

/// Get the Function parent of the basic block and copy its CFG into a boost
/// graph library analog. WARNING: build an undirected graph!
void NeckAnalysis::copyFunctionCFGIntoBoostGraph(llvm::BasicBlock *Src,
                                                 BCFGGraph &G) {
  std::queue<llvm::BasicBlock *> Queue;
  std::unordered_map<llvm::BasicBlock *, bool> Visited;
  std::unordered_map<llvm::BasicBlock *, BCFGGraph::vertex_descriptor> Vertices;

  G.clear();

  for (llvm::BasicBlock &BB : *Src->getParent()) {
    Visited[&BB] = false;
    // Also construct all vertices for insertion into the boost graph.
    BCFGGraph::vertex_descriptor v = boost::add_vertex(G);
    G[v].bb = &BB;
    Vertices[&BB] = v;
  }

  // BFS which rebuilds the LLVM CFG graph into boost's graph library
  // representation.

  /*
  // TODO: Make a seperate function that dumps the undirected graph G into
  // a dotfile so we can lift this dotfile creation part of the code out
  // of here.
  std::error_code EC;
  llvm::raw_fd_ostream OF("bcfg.dot", EC);
  OF << "graph G {\n";
  */

  Visited[Src] = true;
  Queue.push(Src);

  while (!Queue.empty()) {
    llvm::BasicBlock *Current = Queue.front();
    Queue.pop();

    bcfg_vertex_descriptor source = Vertices[Current];

    for (llvm::BasicBlock *Child : llvm::successors(Current)) {
      /*
      llvm::outs() << "  About to add CFG edge: " << Current << " -> " <<
        Child << "\n";
      std::fflush(NULL);
      */

      bcfg_vertex_descriptor target = Vertices[Child];

      std::pair<bcfg_edge_descriptor, bool> ret =
          boost::add_edge(source, target, G);

      /*
      // If the edge was brand new, then we dump it out into the dotfile.
      if (ret.second) {
        OF << "  \"\\";
        G[source].bb->printAsOperand(OF, false);
        OF << " - " << G[source].bb;
        OF << "\" -- \"\\";
        G[target].bb->printAsOperand(OF, false);
        OF << " - " << G[target].bb;
        OF << "\"\n";
      }
      */

      if (Visited[Child]) {
        continue;
      }
      Visited[Child] = true;
      Queue.push(Child);
    }
  }
  /*
  OF << "}\n";
  */
}

/// Extract articulation points from the CFG of the function provided.
/// Store the undirected graph into the reference of undCFG
void NeckAnalysis::extractArticulationPointsFromFunc(
    llvm::Function *func, BCFGGraph &undCFG,
    std::unordered_set<llvm::BasicBlock *> &artPoints) {
  undCFG.clear();

  copyFunctionCFGIntoBoostGraph(&func->front(), undCFG);

  /*
  llvm::outs() << "-----------------------------------------\n";
  std::pair<bcfg_edge_iterator, bcfg_edge_iterator> ei = boost::edges(undCFG);
  llvm::outs() << "  Boost Graph edge list.\n";
  std::fflush(NULL);
  for (auto eit = ei.first; eit != ei.second; ++eit) {
    bcfg_vertex_descriptor source = boost::source(*eit, undCFG);
    bcfg_vertex_descriptor target = boost::target(*eit, undCFG);
    llvm::BasicBlock *bb_source = undCFG[source].bb;
    llvm::BasicBlock *bb_target = undCFG[target].bb;
    llvm::outs() << "    " << bb_source << " -- " << bb_target << "\n";
  }
  llvm::outs() << "-----------------------------------------\n";
  */

  std::vector<bcfg_vertex_descriptor> art_points;
  boost::articulation_points(undCFG, std::back_inserter(art_points));
  // Get the actual BB's out of the vertex container.
  for (bcfg_vertex_descriptor &v : art_points) {
    artPoints.insert(undCFG[v].bb);
  }
}

// From all the functions we've decided to inspect for articulation points,
// get the final set.
void NeckAnalysis::passExtractAllArticulationPoints(int &passID) {
  BCFGGraph undCFG;
  std::unordered_set<llvm::BasicBlock *> artPoints;

  initPass(passID, __FUNCTION__);

  for (llvm::Function *func : ParticipatingFunctions) {
    llvm::StringRef funcName = getSafeName(func);

    spdlog::info("  Extracting articulation points from function: {}",
                 funcName);

    artPoints.clear();
    extractArticulationPointsFromFunc(func, undCFG, artPoints);

    spdlog::info("   Observed {} articulation points.", artPoints.size());

    // Store them off into the collection we're building of all of them.
    for (auto *ap : artPoints) {
      auto ret = ArticulationPoints.insert(ap);
      if (ret.second) {
        addAnnotation(ap, makeAnnotation(ASSERT_ARTICULATION, passID));
      }
    }
  }

  spdlog::info("  Found {} total articulation points.",
               ArticulationPoints.size());

  finiPass(passID);
}

/// We discover articulation points that dominate their entire
/// descendent set and mark them as Choke Points. These points truly
/// represent a partition of execution before the choke point and
/// after the choke point and is the actual definition of the
/// underlying meaning of "neck" in the original paper. The original
/// paper's use of articulations points didn't actually conform to
/// the meaning they imbued upon it.
/// TODO: This algorithm will pick loop heads which are also articulation
/// points and this is wrong. There is a dedicated pass to repair this
/// but the fix should really be in isFullDominator().
void NeckAnalysis::passSelectChokePoints(int &passID) {
  int aps_tested = 0;
  int choke_points_marked = 0;

  initPass(passID, __FUNCTION__);

  for (auto *ap : ArticulationPoints) {
    llvm::DominatorTree &DT = getDominatorTree(ap->getParent());
    aps_tested++;
    if (isFullDominator(ap, &DT)) {
      auto ret = ChokePoints.insert(ap);
      if (ret.second) {
        addAnnotation(ap, makeAnnotation(ASSERT_CHOKEPOINT, passID));
        choke_points_marked++;
      }
    } else {
      addAnnotation(ap, makeAnnotation(REFUTE_CHOKEPOINT, passID));
    }
  }

  spdlog::info("  Articulation Points considered: {}\n"
               "  Choke Points Marked: {}",
               aps_tested, choke_points_marked);

  finiPass(passID);
}

/// Repair a wonky full(strict) dominator bug by removing any choke
/// point that are loop heads.
void NeckAnalysis::passRemoveLoopChokePoints(int &passID) {
  int before_loop_removal = ChokePoints.size();

  initPass(passID, __FUNCTION__);
  eraseIf(ChokePoints, [this](auto const &x) { return isInLoopStructue(x); });

  int removed_count = before_loop_removal - ChokePoints.size();
  spdlog::info("  Choke Points removed due to being in loops: {}",
               removed_count);

  finiPass(passID);
}

// If a loop has any basic block in it that is tainted, then give
// the "looptaint" property to the header node for that loop.
// head a special "looptaint" property that has
// NOTE: Must be run after ParticipatingFunctions has executed.
// TODO: Handle irreducible loops.
void NeckAnalysis::passMarkTaintedLoops(int &passID) {
  initPass(passID, __FUNCTION__);
  int numTaintedLoops = 0;

  for (llvm::Function *func : ParticipatingFunctions) {
    for (llvm::BasicBlock &bb : *func) {
      if (TaintedBasicBlocks.count(&bb) == 0) {
        continue;
      }
      auto *Loop = getLoopInfo(&bb).getLoopFor(&bb);
      if (Loop == nullptr) {
        continue;
      }
      llvm::BasicBlock *header = Loop->getHeader();
      auto result = TaintedLoops.insert(header);
      if (result.second) {
        addAnnotation(header, makeAnnotation(ASSERT_LOOPTAINT, passID));
        numTaintedLoops++;
      }
    }
  }

  spdlog::info("  Unique tainted loops: {}", numTaintedLoops);

  finiPass(passID);
}

// TODO handle irreducible loops?
// Build a map from each toplevel loop head to all of its exits.
// NOTE: We might actually not want this pass in the future
// because we'll need to account for the contributions of the
// basic block in those loops for purposes of decided if the
// path leading to a neck is valid.
// NOTE: We don't actually use this information yet, so far we just
// collect it.
void NeckAnalysis::passCollectLoopPassthroughMap(int &passID) {
  int loop_count = 0;

  initPass(passID, __FUNCTION__);

  for (llvm::Function *Func : ParticipatingFunctions) {
    llvm::LoopInfo &FuncLoopInfo = getLoopInfo(Func);
    const std::vector<llvm::Loop *> &FuncTopLevelLoops =
        FuncLoopInfo.getTopLevelLoops();
    for (llvm::Loop *TopLevelLoop : FuncTopLevelLoops) {
      llvm::BasicBlock *Header = TopLevelLoop->getHeader();

      llvm::SmallVector<llvm::BasicBlock *, 5> Exits;
      TopLevelLoop->getUniqueExitBlocks(Exits);
      std::unordered_set<llvm::BasicBlock *> LoopExits;
      LoopExits.insert(Exits.begin(), Exits.end());

      LoopPassthroughMap.insert(std::pair(Header, LoopExits));
      loop_count += 1;
    }
  }

  spdlog::info("  Top Level Loops mapped: {}", loop_count);

  finiPass(passID);
}

// We build a table of functions that call getopt and friend (GOAF) i
// functions.
// TODO: We should load the function names from a json config somewhere.
void NeckAnalysis::passCollectGetOptAndFriendsMap(int &passID) {
  initPass(passID, __FUNCTION__);

  std::unordered_set<std::string> all_goaf;
  // TODO WARNING: Currently assumes that the shared libraries are NOT in the
  // whole program LL file. When they are added we'll have to do probably more
  // work in determining the linkage habits of the getopt family functions..

  // 0. Define set: getopt(), getopt_long, getopt_long_only()
  // TODO: Read from command line or a config file too.
  all_goaf.insert("getopt");
  all_goaf.insert("getopt_long");
  all_goaf.insert("getopt_long_only");
  all_goaf.insert("rpl_getopt");
  all_goaf.insert("rpl_getopt_long");
  all_goaf.insert("rpl_getopt_long_only");

  // Filter the goaf functions into internal and external linkage sets.
  std::unordered_set<std::string> external_goaf;
  std::unordered_set<std::string> internal_goaf;
  for (auto it = all_goaf.begin(); it != all_goaf.end(); it++) {
    const std::string &name = *it;
    llvm::Function *func = M.getFunction(name);
    if (func == NULL) {
      // Assume if no known function, it is external.
      external_goaf.insert(name);
      continue;
    }
    llvm::GlobalValue::LinkageTypes ltype = func->getLinkage();
    if (ltype != llvm::GlobalValue::LinkageTypes::ExternalLinkage) {
      internal_goaf.insert(name);
    } else {
      external_goaf.insert(name);
    }
  }

  spdlog::info("  GOAF External Definitions: ");
  for (auto name : external_goaf) {
    spdlog::info("    {}", name);
  }
  spdlog::info("  GOAF Internal Definitions: ");
  for (auto name : internal_goaf) {
    spdlog::info("    {}", name);
  }

  int numCallGAF = 0;
  // For each found external GOAF
  for (std::string name : external_goaf) {
    // For each participating function
    for (llvm::Function *Func : ParticipatingFunctions) {
      // Prep to record and GOAF BasicBlocks
      FunctionToGetOptAndFriendsMap.insert(
          std::pair(Func, std::unordered_set<llvm::BasicBlock *>()));
      // For each
      for (auto &BB : *Func) {
        for (auto itr = BB.begin(); itr != BB.end(); ++itr) {
          llvm::CallInst *inst = llvm::dyn_cast<llvm::CallInst>(itr);
          if (!inst) {
            continue;
          }

          llvm::Function *PotentialGOAF = inst->getCalledFunction();
          // TODO inspect this closer
          if (!PotentialGOAF) {
            // Skip if indirect function call
            continue;
          }

          std::string name = PotentialGOAF->getName().str();
          if (external_goaf.count(name) == 0) {
            // Skip if internal GOAF call
            continue;
          }

          // Finally, we found an instruction that called an external getopt
          // and family function. Count the unique ones we find.
          FunctionToGetOptAndFriendsMap[Func].insert(inst->getParent());
          numCallGAF += 1;
        }
      }
    }
  }
  NumCallsToGAF = numCallGAF;
  finiPass(passID);
}

// Build a map with a key which is a basic block that is a choke point,
// and a value that is struct of these attributes:
// A. Count of loops heads before this chokepoint,
// B. Is getopt called in any of this function's BB's?
// C. True if all getopts are before this choke point.
// D. Max getopts passed at this point (if this is easy for you to do, if not,
// drop this one)
void NeckAnalysis::passCollectAllBasicBlockAttributes(int &passID) {
  initPass(passID, __FUNCTION__);

  // Mapping of loops headers to descendants
  std::unordered_map<llvm::BasicBlock *, std::unordered_set<llvm::BasicBlock *>>
      LoopDescendantMap;

  // For each basic block
  for (llvm::BasicBlock *BB : ParticipatingBBs) {
    int numSucceededLoopHeads = 0;
    int numSucceededTaintedLoopHeads = 0;
    int numSucceededGAFs = 0;

    // Get the parent Function
    llvm::Function *Func = BB->getParent();

    // Get the loop info for the parent Function
    llvm::LoopInfo &FuncLoopInfo = getLoopInfo(Func);
    // Get the top level loops in the function
    const std::vector<llvm::Loop *> &FuncTopLevelLoops =
        FuncLoopInfo.getTopLevelLoops();
    // For each top level loop in the function
    for (llvm::Loop *TopLevelLoop : FuncTopLevelLoops) {
      // Get the header
      llvm::BasicBlock *Header = TopLevelLoop->getHeader();
      // Get the descendants
      std::unordered_set<llvm::BasicBlock *> Descendants;
      if (LoopDescendantMap.count(Header) > 0) {
        Descendants = LoopDescendantMap[Header];
      } else {
        findAllDescendants(Header, Descendants);
        LoopDescendantMap.insert(std::pair(Header, Descendants));
      }

      // if the current choke point is in the descendants then
      if (Descendants.count(BB) > 0) {
        // increment the counter
        numSucceededLoopHeads++;

        // If the current BB is also after a "tainted loop" mark that
        // too!
        if (TaintedLoops.count(Header) > 0) {
          numSucceededTaintedLoopHeads++;
        }
      }
    }

    bool is_goaf_called_in_parent =
        FunctionToGetOptAndFriendsMap.count(Func) > 0 &&
        FunctionToGetOptAndFriendsMap[Func].size() > 0;
    if (is_goaf_called_in_parent) {
      // Get the ChokePoint descendants
      std::unordered_set<llvm::BasicBlock *> ChokePointDescendants;
      findAllDescendants(BB, ChokePointDescendants);
      // Since there is no path around the choke point then
      // and GOAF that is in the choke point descendant set
      // means that the choke point is not after all GOAF.
      // So for each GOAF in the current function
      for (auto goaf : FunctionToGetOptAndFriendsMap[Func]) {
        // If that GOAF is in the choke points descendant set
        if (ChokePointDescendants.count(goaf) <= 0) {
          // TODO the following does not handle cases where the
          // there are branching paths that use GOAF within them.
          //
          // Otherwise increment the counter for preceding goafs
          numSucceededGAFs++;
        }
      }
    }

    // llvm::outs() << "BasicBlock Attributes ";
    // BB->printAsOperand(llvm::outs(), false);
    // llvm::outs() << ":\n"
    //              << "  numSucceededLoopHeads: " << numSucceededLoopHeads <<
    //              "\n"
    //             << "  numSucceededGAFs: " << numSucceededGAFs << "\n";

    AttributeMap attributes;
    attributes.insert({attrname_numSucceededLoopHeads, numSucceededLoopHeads});
    attributes.insert(
        {attrname_numSucceededTaintedLoopHeads, numSucceededTaintedLoopHeads});
    attributes.insert({attrname_numSucceededGAFs, numSucceededGAFs});
    BB_AttrMaps.insert({BB, attributes});
  }

  finiPass(passID);
}

// Starting at each tainted function, walk up the parents until you
// get to main and keep all the functions in between in a set. Do
// this for every tainted function. The resulting set is called the
// ParticipatingFunctions and those are functions that, starting from
// main, can lead to tainted basic blocks.
void NeckAnalysis::passConstructParticipatingFunctionsFromTaintedFunctions(
    int &passID) {
  int num_funcs = 0;

  initPass(passID, __FUNCTION__);

  std::unordered_set<llvm::Function *> VisitedFunctions;
  std::queue<llvm::Function *> WorkQueue;
  for (llvm::Function *Func : TaintedFunctions) {
    WorkQueue.push(Func);
  }

  while (!WorkQueue.empty()) {
    // Take the next bit of work from the list
    auto *Func = WorkQueue.front();
    WorkQueue.pop();

    // Skip the function if we have already visited it
    if (VisitedFunctions.count(Func) > 0) {
      continue;
    }

    // Record the function has been visited
    VisitedFunctions.insert(Func);

    // Do not search before Main
    if (Func == Main) {
      continue;
    }

    std::unordered_set<llvm::Function *> Callers;
    // For every use of the current function
    for (llvm::Value::use_iterator ui = Func->use_begin();
         ui != Func->use_end(); ++ui) {
      // If the user of the function is a call instruction
      if (auto *call = llvm::dyn_cast<llvm::CallBase>(ui->getUser())) {
        // Collect the function parent of the call instruction
        llvm::Function *caller = call->getParent()->getParent();
        // Add the function to the work list
        WorkQueue.push(caller);
        Callers.insert(caller);
      }
    }
    std::pair<llvm::Function *, std::unordered_set<llvm::Function *>> p(
        Func, Callers);
    TaintedCalleeToCallerMap.insert(p);
  }

  if (VisitedFunctions.count(Main) <= 0) {
    spdlog::info("Main not added during visiting loop!");
  }
  VisitedFunctions.insert(Main);

  for (auto *Func : VisitedFunctions) {
    ParticipatingFunctions.insert(Func);
    num_funcs++;
  }

  spdlog::info("  Participating functions: {}", num_funcs);

  finiPass(passID);
}

// Emit a graphviz graph of the tainted callers to callees information.
void NeckAnalysis::dumpTaintedCalleeToCallerGraph() {
  spdlog::info("digraph {");
  for (auto *Func : TaintedFunctions) {
    spdlog::info("    {} [color=green]", Func->getName());
  }
  for (auto I : TaintedCalleeToCallerMap) {
    for (auto *Func : I.second) {
      spdlog::info("{} -> {}", Func->getName(), I.first->getName());
    }
  }
  spdlog::info("}");
}

// Collect all basic blocks from participating functions.
void NeckAnalysis::passCollectAllParticipatingBBs(int &passID) {
  int num_bbs = 0;
  initPass(passID, __FUNCTION__);

  for (auto *Func : ParticipatingFunctions) {
    for (auto &BB : *Func) {
      auto result = ParticipatingBBs.insert(&BB);
      if (result.second) {
        num_bbs++;
      }
    }
  }

  spdlog::info("  Participating basic blocks: {}", num_bbs);
  finiPass(passID);
}

// It is poorly documented if llvm::BasicBlock::hasNPredecessorsOrMore()
// checks the number of *links* to all predecessors or for
// if it checks the actual unique predecessor nodes themselves.
// So we wrote our own to be sure.
bool hasUniqueNPredecessorsOrMore(llvm::BasicBlock *BB, unsigned long num) {
  std::unordered_set<llvm::BasicBlock *> uniquePreds;

  for (auto *parent : llvm::predecessors(BB)) {
    uniquePreds.insert(parent);
  }

  return uniquePreds.size() >= num;
}

/// For a single function, find all leaf basic blocks with more than one
/// predecessor and no successors, then duplicate the block. In the case
/// where there is a single predecessor node and multiple links to the
/// same node (e.g. a C switch statement) we will duplicate the block once
/// for each edge. We always must fixup the terminator and phi functions of
/// the affected blocks. The algorithm has this funny shape since we're
/// being exceedingly careful about mutation of LLVM data causing us grief, so
/// the duplication happens in a set of phases so we don't shoot ourselves
/// in the foot.
///
/// NOTE: The places where this can result in unfortunate code duplication
/// is an if/switch statement early in a function and then a single large
/// basic block that is also the leaf of the function.
bool NeckAnalysis::duplicateTailBasicBlocks(llvm::Function *func, int passID,
                                            int &tot_sharedLeaves,
                                            int &tot_sharedLeafPreds,
                                            int &tot_duplicatedLeaves) {
  bool duplicated_nodesp = false;
  std::unordered_set<llvm::BasicBlock *> sharedLeaves;

  // The key is a shared basic block, the value is a copied list of
  // predecessors of that block. There may be duplicates in the
  // predecessors and that's ok (C switch statement, for example).
  std::unordered_map<llvm::BasicBlock *, std::list<llvm::BasicBlock *>>
      sharedPreds;

  // A list of tuples that represent the old edge from the predecessor to the
  // original leaf, and the new edge to the cloned leaf.
  // The tuple is: (predecessor, clonedBB, origBB)
  //
  // This handles the case where a switch
  // statement with many paths to the same basic block that has an
  // unreachable terminator needs a copy for each shared edge.
  std::list<
      std::tuple<llvm::BasicBlock *, llvm::BasicBlock *, llvm::BasicBlock *>>
      dupEdges;

  // ///////////////////////////////////////////////////
  // Phase 0
  // Find basic blocks that have no successors and multiple predecessors.
  // ///////////////////////////////////////////////////
  for (llvm::BasicBlock &BB : *func) {
    if (succ_empty(&BB) && hasUniqueNPredecessorsOrMore(&BB, 2)) {
      sharedLeaves.insert(&BB);
    }
  }
  tot_sharedLeaves = sharedLeaves.size();
  tot_sharedLeafPreds = 0;
  tot_duplicatedLeaves = 0;

  if (tot_sharedLeaves == 0) {
    return duplicated_nodesp;
  }

  // For each leaf, walk its predecessors and clone it (if applciable), then
  // fixup the precedessor terminators and cloned phi information.  Since it
  // is a leaf, there are no successors to fix.

  // ///////////////////////////////////////////////////
  // Phase 1
  // Build the association between each shared block and the list of their
  // (possibly duplicated) predecessors. We need this copy since we'll be
  // editing the predecessors later which will invalidate the iterator(s) in
  // what would be the usual way to write this.
  // ///////////////////////////////////////////////////
  for (auto *BB : sharedLeaves) {
    for (auto it = pred_begin(BB); it != pred_end(BB); it++) {
      sharedPreds[BB].push_back(*it);
      tot_sharedLeafPreds++;
    }
  }

  // ///////////////////////////////////////////////////
  // Phase 2
  // Construct the (possibly) duplicated leaf basic blocks with the right
  // dataflow in the instruction stream, but wrong terminators and
  // phi nodes in the participating basic blocks.
  // ///////////////////////////////////////////////////

  for (auto item : sharedPreds) {
    llvm::BasicBlock *BB = item.first;
    std::list<llvm::BasicBlock *> &copiedBBpreds = item.second;

    spdlog::debug("   Processing shared leaf: {}", getBBIdsString(BB));

    // 2. For each predecessor to the shared block, make a duplicate of the
    // shared block if appropriate and record an ephemeral edge between the
    // predecessor and the (possibly) new block.
    for (auto it = copiedBBpreds.begin(); it != copiedBBpreds.end(); it++) {
      llvm::BasicBlock *predBB = *it;
      if (it == copiedBBpreds.begin()) {
        // 2a. The very first predecessor can claim the current BB as its own.
        spdlog::debug("    Predecessor {} -> (keeps original: {})",
                      getBBIdsString(predBB), getBBIdsString(BB));
        dupEdges.push_back({predBB, BB, BB});
        addAnnotation(BB, makeAnnotation(ASSERT_DUP, passID));
        continue;
      }

      // 2b. The non first predecessors get duplicate copies of the shared
      // leaf.  Copy the block and its instructions, preserving the dataflow
      // between the new defs and the uses of them in the same manner as the
      // original BB we're copying.

      llvm::ValueToValueMapTy vmap;
      auto *clonedBB = llvm::CloneBasicBlock(BB, vmap, "", BB->getParent());
      std::string clonedBBName;
      // TODO: What do I need for the % prefixes here?
      clonedBBName.append(getBBName(clonedBB, true));
      clonedBBName.append("_copy_of_");
      clonedBBName.append(getBBName(BB, true));
      clonedBB->setName(clonedBBName);

      vmap[BB] = clonedBB;
      for (llvm::Instruction &i : *clonedBB) {
        RemapInstruction(&i, vmap,
                         llvm::RF_NoModuleLevelChanges |
                             llvm::RF_IgnoreMissingLocals);
      }
      dupEdges.push_back({predBB, clonedBB, BB});

      // If BB was tainted, then make clonedBB tainted.
      if (TaintedBasicBlocks.count(BB) > 0) {
        auto result = TaintedBasicBlocks.insert(clonedBB);
        if (result.second) {
          addAnnotation(clonedBB, makeAnnotation(ASSERT_DUP, passID));
          addAnnotation(clonedBB, makeAnnotation(ASSERT_TAINT, passID));
        }
      }
      // If BB was a Neck Candidate, then make cloned BB also a Neck
      // Candidate.
      if (NeckCandidates.count(BB) > 0) {
        auto result = NeckCandidates.insert(clonedBB);
        if (result.second) {
          addAnnotation(clonedBB, makeAnnotation(ASSERT_BOUNDARY, passID));
        }
      }

      // TODO: Deal with cmp, branch, or phi nodes that might have been
      // removed. I may need to reclassify if this BB (or clonedBB)
      // shbould be in the UserBranchAndCompInstructions set.

      tot_duplicatedLeaves++;
      duplicated_nodesp = true;

      spdlog::debug("    Predecessor {} -> (cloned: {} of original {})",
                    getBBIdsString(predBB), getBBIdsString(BB));
    }
  }

  // ///////////////////////////////////////////////////
  // Phase 3
  // Reifiy the new edges in the CFG we're mutating, and fix up the
  // terminator of the predecessor BB to point to the new successor we
  // assigned for it, and fixup the phi nodes of the successor to handle
  // that there is only one predecessor now.
  // ///////////////////////////////////////////////////
  for (auto tuple : dupEdges) {
    llvm::BasicBlock *predBB = nullptr;
    llvm::BasicBlock *clonedBB = nullptr;
    llvm::BasicBlock *origBB = nullptr;
    std::tie(predBB, clonedBB, origBB) = tuple;

    // Fixup terminator of predBB to point to new clone instead of old BB.
    llvm::Instruction *term = predBB->getTerminator();
    if (term != NULL) {
      term->replaceSuccessorWith(origBB, clonedBB);
    }

    // Process the Phi nodes of the clonedBB (either kept or
    // duplicated block) to ensure they are correct.
    // The way we do this, is to walk the origBB's original predecessors
    // and remove the ones that AREN'T the current predBB of clonedBB.
    // If All of them removed, LLVM does the smart thing and removes the
    // phi node and splices the correct value in (apprently?) correctly.
    // TODO: Figure out if _apparently_ is _actually. :)
    std::list<llvm::BasicBlock *> &origSharedPreds = sharedPreds[origBB];
    for (llvm::BasicBlock *origPredBB : origSharedPreds) {
      // when we find an origPredBB that _isn't_ predBB, we remove it!
      if (origPredBB != predBB) {
        clonedBB->removePredecessor(origPredBB);
      }
    }
  }
  return duplicated_nodesp;
}

// Return true if the BB is a CFG leaf with a single parent whose
// successor is the BB. This rule only indicates that N1 MIGHT
// be erodable, and we'll have to check against further rules.
// When called with N1, we look for a pattern like:
//
// <zero or more links to N0>
//  | ... |
//  V ... V
//    N0
//    |
//    v
//    N1
bool erosionRule_PossiblyErodable(llvm::BasicBlock *BB) {
  if (not succ_empty(BB)) {
    // BB is not a Leaf. Reject.
    return false;
  }

  auto *parent = BB->getSinglePredecessor();
  if (parent == nullptr) {
    // BB has more than one predecessor, or none when it is the
    // entry block. Reject.
    return false;
  }

  auto child = parent->getSingleSuccessor();
  if (child == nullptr) {
    // BB's parent has more than one successor. Reject.
    return false;
  }

  if (child != BB) {
    // The single child BETTER be the BB!
    spdlog::error("{}: Programmer Error! "
                  "In the function: {}"
                  ", how can the parent of the leaf: {}"
                  " have as a child a different basic block: {}"
                  "?",
                  __func__, getSafeName(BB->getParent()), getBBName(BB),
                  getBBName(child));
    std::exit(EXIT_FAILURE);
  }

  return true;
}

// Return true if the assumed possibly erodable leaf's parent has more
// than one unique grandfather.
// G0 ... GN <there must be more than 1 unique>
//  | ... |
//  V ... V
//    N0
//    |
//    v
//    N1
bool erosionRule_ParentDuplicatable(llvm::BasicBlock *peleaf) {
  std::unordered_set<llvm::BasicBlock *> uniquePreds;

  llvm::BasicBlock *parent = peleaf->getSinglePredecessor();

  for (auto *grandfather : llvm::predecessors(parent)) {
    uniquePreds.insert(grandfather);
  }

  return uniquePreds.size() > 1;
}

// Test to see if the grandfather's successors are to only the parent
// of the possibly erodable leaf.
//
// return false if there are no grandfathers to this leaf.
// return false if any grandfather has a non parent successor.
// return true if all grandfathers only have the parent as the succ.
// It is the case that this rule passes (returns true) when we are
// able to perform an erosion with the leaf.
//
// NOTE: It is assumed that the leaf is _actually a leaf_ in the
// CFG when function is called upon it.
bool erosionRule_Grandfather(llvm::BasicBlock *peleaf) {
  llvm::BasicBlock *parent = peleaf->getSinglePredecessor();

  if (llvm::pred_empty(parent)) {
    return false;
  }

  for (auto *grandfather : llvm::predecessors(parent)) {
    // If any successor of this grand father is not the parent, we
    // fail the grandfather test.
    for (auto *offspring : llvm::successors(grandfather)) {
      if (offspring != parent) {
        return false;
      }
    }
  }

  return true;
}

// Perform one iteration of erosion of all applicable erodable blocks.
bool NeckAnalysis::erodeTailBasicBlocks(llvm::Function *func, int passID,
                                        int &tot_eroded) {
  bool didErode = false;
  std::unordered_map<llvm::BasicBlock *, llvm::BasicBlock *> erodableBlocks;
  // ///////////////////////////////////////////////////
  // Find possibly erodable leaves.
  // ///////////////////////////////////////////////////
  for (llvm::BasicBlock &BB : *func) {
    if (not erosionRule_PossiblyErodable(&BB)) {
      continue;
    }

    llvm::BasicBlock *parent = BB.getSinglePredecessor();

    // There are different rules for erdability that must be checked
    // in a specific order and aren't exactly a disjunction.

    if (erosionRule_ParentDuplicatable(&BB)) {
      erodableBlocks.insert({&BB, parent});
      continue;
    }

    if (erosionRule_Grandfather(&BB)) {
      erodableBlocks.insert({&BB, parent});
      continue;
    }
  }

  // ///////////////////////////////////////////////////
  // Do ONE step of the contraction for each erodable leaf into its
  // single predecessor.
  // ///////////////////////////////////////////////////
  for (auto &item : erodableBlocks) {
    llvm::BasicBlock *bb = item.first;
    llvm::BasicBlock *pred = item.second;

    // We are about to merge, so assert/retract the currently defined
    // properties for these basic blocks as appropriate.

    // If bb is tainted, then make pred tainted.
    if (TaintedBasicBlocks.count(bb) > 0) {
      TaintedBasicBlocks.erase(bb);
      TaintedBasicBlocks.insert(pred);
    }

    // If pred is NeckCandidate, then retract it (cause it can't
    // be a as a leaf!)
    // TODO: Well, it can if it is the only leaf. deal with later
    // elsewhere....
    if (NeckCandidates.count(pred) > 0) {
      NeckCandidates.erase(pred);
    }

    // The leaf is getting erased, so erase it from the data structures.
    PassAnnotations.erase(bb);
    TaintedBasicBlocks.erase(bb);
    NeckCandidates.erase(bb);

    // Finally, merge bb into pred.
    spdlog::debug("  Eroding: {} <-- {}", getBBName(pred), getBBName(bb));

    MergeBBResult result = mergeBBs(pred, bb);
    if (std::holds_alternative<MergeBBsError>(result)) {
      auto err = std::get<MergeBBsError>(result);
      spdlog::error("{}: Failed to merge an eroding basic block into "
                    "predecessor. MergeBBsError is: {}",
                    __func__, (uint32_t)err);

      spdlog::error("Parent\n======\n{}"
                    "Child\n=====\n{}",
                    *pred, *bb);
      std::exit(EXIT_FAILURE);
    }
    didErode = true;
  }

  return didErode;
}

/// The purpose of tail duplication/erosion is to "unknot" the CFG graph
/// near the function leaf horizon such that we have a better chance
/// of finding more articulation points in the function's CFG (but
/// not unknot so much that we explode in size or suffer from a
/// bounty of articulation points).
///
/// Tail duplication takes common tails in the CFG and
/// duplicates/erodes them until a fixed point is reached such that each
/// tail has only one parent and at least one grandparent. Erosion is
/// merging of basic blocks with their parents under the right
/// circumstance. This phase stops when a graph complexity measure
/// (identified by the erosion rules)
/// is reached to prevent an explosion of the graph size and
/// complicating of the choke point disocvery.
///
/// This will often result in more meaningful articulation points for
/// each participating function.
void NeckAnalysis::passTailDuplicateParticipatingFunctions(int &passID) {
  int tot_sharedLeaves = 0;
  int tot_sharedLeafPreds = 0;
  int tot_erodedLeaves = 0;
  int tot_duplicatedLeaves = 0;
  initPass(passID, __FUNCTION__);

  bool tail_duplicating = ParticipatingFunctions.size() > 0;
  bool didLeafDup = false;
  bool didLeafErosion = false;
  int erosionPassID = 0;

  // We run a loop to iteratively erode leaves dominated by a
  // predecessor of exactly one parent, then duplicate them until
  // we reached a fixed point where nothing changes anymore.
  while (tail_duplicating) {

    // Step 1: Erode if possible.
    spdlog::debug("  --> Erosion Iteration: {}", erosionPassID);

    didLeafErosion = false;
    didLeafDup = false;

    // NOTE: These steps are split into two loops because in the
    // duplication of the tails we can be messing about with the
    // CFG of the function and silently invalidating iterator if
    // we merged the work of the loops into a single loop.

    // Step 1: Perform erosion of the tails, if possible.
    for (auto *func : ParticipatingFunctions) {
      int num_erodedLeaves = 0;
      spdlog::debug("  Eroding leaves in Func: {}", getSafeName(func));
      didLeafErosion = erodeTailBasicBlocks(func, passID, num_erodedLeaves)
                           ? true
                           : didLeafErosion;
      spdlog::debug("  NumErodedLeaves: {}", num_erodedLeaves);
      tot_erodedLeaves += num_erodedLeaves;
    }

    // Step 2: Duplicate any tails if possible.
    for (auto *func : ParticipatingFunctions) {
      int num_sharedLeaves = 0;
      int num_sharedLeafPreds = 0;
      int num_duplicatedLeaves = 0;
      spdlog::debug("  Duplicating shared leaves in Func: {}",
                    getSafeName(func));
      didLeafDup =
          duplicateTailBasicBlocks(func, passID, num_sharedLeaves,
                                   num_sharedLeafPreds, num_duplicatedLeaves)
              ? true
              : didLeafDup;

      spdlog::info("  NumSharedLeaves: {}\n"
                   ", NumSharedLeafPreds: {}\n"
                   ", NumDuplicatedLeaves: {}",
                   num_sharedLeaves, num_sharedLeafPreds, num_duplicatedLeaves);
      // Manage Statistics
      tot_sharedLeaves += num_sharedLeaves;
      tot_sharedLeafPreds += num_sharedLeafPreds;
      tot_duplicatedLeaves += num_duplicatedLeaves;
    }

    // Determine if I must do another pass.
    // When this is false we've reached a fixed point where nothing
    // has changed and we're done.
    tail_duplicating = didLeafErosion || didLeafDup;

    erosionPassID++;
  }

  // Step 3. Now that we've messed about with the functions CFG,
  // we will recalculate the function caches for the participating
  // functions.
  // This pass doesn't use the dominator tree or the loop information
  // so it is safe to do this after all the iterations are finished
  // as opposed to after each iteration in each for loop.
  // NOTE: THIS IS CRITICAL TO PERFORM WHEN MODIFYING THE CFG.
  for (auto *func : ParticipatingFunctions) {
    refreshFunctionCaches(func);
  }

  spdlog::info("  TailDuplicationSummary:\n"
               "    TotErodedLeaves: {}\n"
               "    TotSharedLeaves: {}\n"
               "    TotSharedLeafPreds: {}\n"
               "    TotDuplicatedLeaves: {}",
               tot_erodedLeaves, tot_sharedLeaves, tot_sharedLeafPreds,
               tot_duplicatedLeaves);

  finiPass(passID);
}

// This is the meat and potatoes of the neck miner search algorithm.
// We iterate a neck selection function over paths to choke points and
// store off paths that we like. Then we choose from those stored paths
// the final neck candidate. This is purely filter, rank, select of
// entire paths to a neck candidate.
// This is the function where the raw decisions about why we want to
// pick a particular neck is chosen.
void NeckAnalysis::passExecuteNeckSearch(int &passID) {
  initPass(passID, __FUNCTION__);

  // These two variables keep track of the maximum number of getopts
  // seen from the point of view of both after any loop, and after
  // tainted loops.
  int maxGetOptsSeen_any = -1;
  int maxGetOptsSeen_taint = -1;
  bool lookForAnyLoops = true;
  std::vector<VisitNeckCandidate> tainted_loop_candidates;
  std::vector<VisitNeckCandidate> any_loop_candidates;
  std::vector<VisitNeckCandidate> *candidates = nullptr;
  bool hasGAF = NumCallsToGAF > 0;

  VisitFunc visit = [&maxGetOptsSeen_any, &maxGetOptsSeen_taint,
                     &lookForAnyLoops, &tainted_loop_candidates,
                     &any_loop_candidates, &hasGAF,
                     this](llvm::BasicBlock *bb, bool chokep, bool forbidden,
                           BB_BFS_Q_Path &fpath) {
    llvm::Function *func = bb->getParent();
    if (Debug) {
      spdlog::debug("*** Visiting {}@{} { chokep: {}, forbidden: {} }",
                    func->getName(), getBBName(bb), chokep, forbidden);
    }

    if (not chokep || forbidden) {
      // Reject any non-choke point basic blocks.
      return true;
    }

    AttributeMap summary = summarize_complete_path(*this, fpath);
    std::any anyNumSucceededLoopHeads = summary[attrname_numSucceededLoopHeads];
    std::any anyNumSucceededTaintedLoopHeads =
        summary[attrname_numSucceededTaintedLoopHeads];
    std::any anyNumSucceededGAFs = summary[attrname_numSucceededGAFs];
    const int numSucceededLoopHeads =
        std::any_cast<const int &>(anyNumSucceededLoopHeads);
    const int numSucceededTaintedLoopHeads =
        std::any_cast<const int &>(anyNumSucceededTaintedLoopHeads);
    const int numSucceededGAFs =
        std::any_cast<const int &>(anyNumSucceededGAFs);
    if (Debug) {
      spdlog::debug("Considering choke point with: "
                    "{numSucceededLoopHeads: {}"
                    ", numSucceededTaintedLoopHeads: {}"
                    ", numSucceededGAFs: {}}",
                    numSucceededLoopHeads, numSucceededTaintedLoopHeads,
                    numSucceededGAFs);
    }

    // NOTE: I've modified the original SLASH algorithm such that neck
    // candidates are ALWAYS on choke points (which are a subset of
    // articulation points). The original algorithm
    // (erroneously, in my opinion) allowed neck candidates in places
    // that were not also articulation points.
    if (NeckCandidates.count(bb) == 0) {
      // Reject choke points that are not neck candidates.
      spdlog::debug("Rejecting cause not Neck Candidate.");
      return true;
    }

    // We keep track of ANY loops we pass in the same manner as the
    // tainted loops. This allows us a possible fallback if there
    // were no tainted loops. BUT, this portion of the algorithm can't
    // end the search early so we have to do some logic trickery to
    // stop it from recording when it comes time to! lookForAnyLoops
    // stops the collecting in the condition of the first loop we find
    // that was suceeeded EXCEPT in the case of discovery of a higher
    // GAFs in which case we again store the candidate.
    if (numSucceededLoopHeads > 0 &&
        (numSucceededGAFs > maxGetOptsSeen_any || lookForAnyLoops)) {
      maxGetOptsSeen_any = numSucceededGAFs;
      any_loop_candidates.push_back({bb, fpath});
      lookForAnyLoops = false;
    }

    // But, if we find a tained loop, we can terminate early if we're
    // not also looking for GAFs.
    if (numSucceededTaintedLoopHeads > 0 &&
        numSucceededGAFs > maxGetOptsSeen_taint) {
      maxGetOptsSeen_taint = numSucceededGAFs;

      tainted_loop_candidates.push_back({bb, fpath});

      // continue searching only if we need to find all GAF calls
      // or we haven't seen a tainted loop yet.
      return hasGAF;
    }

    // Otherwise, keep searching!
    return true;
  };

  NeckSearch NS(*this, Main, Debug);
  NS.visit_all(visit);

  // NOTE: Prioritize that we pick from tainted loop candidates first,
  // and then from any loop candidates if there were no tainted ones.
  if (tainted_loop_candidates.size() > 0) {
    candidates = &tainted_loop_candidates;
  } else if (any_loop_candidates.size() > 0) {
    candidates = &any_loop_candidates;
  }

  if (candidates == nullptr) {
    spdlog::info("Neck-miner did not find any candidates!");
    NeckBasicBlock = nullptr;
    NeckInstructionIndex = 0;
  } else {
    auto &[bb, fpath] = candidates->back();
    NeckBasicBlock = bb;
    NeckPath = fpath;
    NeckInstructionIndex = 0;

    // NOTE: Find index of first non phi instruction. This is because
    // currently the neck can only be placed at the start of the
    // instruction stream of a basic block. Basic Blocks may or may not
    // have PHI nodes, but they should have terminators which are
    // instructions.
    auto inst = NeckBasicBlock->begin();
    auto the_end = NeckBasicBlock->end();
    while (inst != the_end && llvm::isa<llvm::PHINode>(*inst)) {
      inst++;
      NeckInstructionIndex++;
    }
    if (inst == the_end) {
      spdlog::error("{}: Selected a neck candidate which had no viable "
                    "instructions in it.",
                    __func__);
      std::exit(EXIT_FAILURE);
    }

    spdlog::info("Neck-miner picked: {}@{}", bb->getParent()->getName(),
                 getBBIdsString(bb));
  }

  std::optional<uint32_t> finalInstIdx;
  std::optional<uint64_t> finalBasicBlockId;
  std::optional<std::string> finalFuncName;
  std::optional<std::string> finalBBName;
  // assign the results from neck-miner (which may be overridden by guiness)
  if (NeckBasicBlock != nullptr) {
    finalInstIdx = NeckInstructionIndex;
    finalBasicBlockId = guiness::getBasicBlockAnnotation(NeckBasicBlock);
    if (!finalBasicBlockId) {
      throw std::logic_error{
          fmt::format("NeckBasicBlock {} in {} didn't have an ID, despite "
                      "containing the neck instruction?",
                      *NeckBasicBlock, *NeckBasicBlock->getParent())};
    }
    finalFuncName = NeckBasicBlock->getParent()->getName().str();
    finalBBName =
        neckid::getBBName(const_cast<llvm::BasicBlock *>(NeckBasicBlock));
  }

  // TODO maybe move this into its own function (in guiness?)
  // find GuiNeSS candidates
  bool useGuinessResults = false;
  if (CombinedModulePath.has_value() && GuiNeSSTapePath.has_value()) {
    llvm::LLVMContext ctx;
    // Load the combined module.
    llvm::SMDiagnostic parseErr;
    auto CombinedModule =
        llvm::parseIRFile(CombinedModulePath.value(), parseErr, ctx);
    if (!CombinedModule) {
      spdlog::error("Failed to parse IRFile for combined-module");
      std::exit(EXIT_FAILURE);
    }
    auto GuiNeSSTape = tape::load_tape_from_file(GuiNeSSTapePath.value());

    guiness::Config config; // TODO: accept as args ?
    // collect all instructions that the tape visited
    auto insts =
        guiness::TapeWalker{config, *CombinedModule, GuiNeSSTape}.collect();
    // find the basic blocks that could contain the neck
    auto candidateBBs = findCandidateBBs(insts);

    // Remove the instructions that could not possibly contain the neck as
    // guiness see it, because they're in BBs that could not be the neck or
    // whose stack-frame call instructions could not be the neck.
    insts.erase(
        std::remove_if(insts.begin(), insts.end(),
                       [&candidateBBs = std::as_const(candidateBBs)](
                           guiness::InstructionWithStack const &inst) {
                         if (!candidateBBs.contains(inst.inst->getParent()))
                           return true;

                         for (auto const &stackInst : inst.stack)
                           if (!candidateBBs.contains(stackInst->getParent()))
                             return true;

                         return false;
                       }),
        insts.end());

    // indicator that NM found a neck
    bool nmSelectedNeck = NeckBasicBlock != nullptr;
    // optional ref to where the neck-miner BB was found in the guiness tape
    // FIXME Ideally this would be the same ref as what neck-miner already
    // found but for now I have left NM as using the original annotated module
    // while guiness needs the combined annotated module.
    std::optional<llvm::BasicBlock *> nmTapeBB;
    llvm::Instruction *nmTapeInst;
    std::vector<guiness::InstructionWithStack>::iterator nmInstWithStackIter;

    // grab the neck-miner selected instruction index
    uint32_t nmTapeInstructionIndex = NeckInstructionIndex;

    // Check if the NM neck appears at all in the tape
    if (nmSelectedNeck) {
      auto nmAnno = guiness::getBasicBlockAnnotation(NeckBasicBlock);
      // search for the neck in the tape BBs
      // if neck-miner picked an annotated block
      if (nmAnno) {
        // determine if neck-miners block is in the set of guiness tape
        // candidates
        for (auto bb : candidateBBs) {
          if (auto anno = guiness::getBasicBlockAnnotation(bb)) {
            if (anno.value() == nmAnno.value()) {
              // Store the BB
              nmTapeBB = bb;

              // Store the instruction
              auto itr = bb->begin();
              std::advance(itr, nmTapeInstructionIndex);
              nmTapeInst = &(*itr);

              // Store the InstructionWithStack
              nmInstWithStackIter = std::find_if(
                  insts.begin(), insts.end(),
                  [&nmTapeInst](guiness::InstructionWithStack iws) {
                    return iws.inst == nmTapeInst;
                  });
              break;
            }
          }
        }
      }

      if (nmTapeBB) {
        spdlog::info("NeckMiner BB found in tape");
        // ensure the NM instruction is on the tape
        if (nmInstWithStackIter < insts.begin() ||
            nmInstWithStackIter >= insts.end()) {
          spdlog::error("neck-miner instruction iterator is out of bounds");
          std::exit(EXIT_FAILURE);
        }
      } else {
        spdlog::warn("Failed to find neck-miner neck in the tape.");
      }
    }

    // Find the GuiNeSS neck
    auto potentialGuinessInstCount = insts.size();
    auto minMaxPair = std::minmax_element(
        insts.begin(), insts.end(),
        [](guiness::InstructionWithStack lhs,
           guiness::InstructionWithStack rhs) {
          return guiness::getSyscallGoodness(lhs.syscallsSoFar) <
                 guiness::getSyscallGoodness(rhs.syscallsSoFar);
        });
    auto guinessInstWithStackIter = minMaxPair.second;

    if (guinessInstWithStackIter < insts.begin() ||
        guinessInstWithStackIter > insts.end()) {
      spdlog::error(
          "guiness instruction iterator is out of bounds:\n  size: {}\n  "
          "before begin: {}\n  is end: {}\n  after end: {}",
          insts.size(),
          guinessInstWithStackIter<insts.begin(),
                                   guinessInstWithStackIter == insts.end(),
                                   guinessInstWithStackIter>
              insts.end());
      std::exit(EXIT_FAILURE);
    }

    // TODO revisit the below block if we ever extend guiness
    // more complex guiness selection
    /*
    if (potentialGuinessInstCount == 0 ||
        guinessInstWithStackIter == insts.end()) {
      // guiness has filtered out all candidates
      spdlog::warn("GuiNeSS found zero neck candidates!");
      useGuinessResults = false;
    } else if (nmTapeBB) {
      spdlog::info("NeckMiner BB found in Tape");
      if (nmInstWithStackIter < insts.begin() ||
          nmInstWithStackIter >= insts.end()) {
        spdlog::error("neck-miner instruction iterator is out of bounds");
        std::exit(EXIT_FAILURE);
      }

      bool guinessBeforeNeckMiner =
          guinessInstWithStackIter < nmInstWithStackIter;
      auto nmSysCalls = nmInstWithStackIter->syscallsSoFar;
      auto guinessSysCalls = guinessInstWithStackIter->syscallsSoFar;

      // find earlier and later syscall sets
      auto earlier = (guinessBeforeNeckMiner) ? guinessSysCalls : nmSysCalls;
      auto later = (guinessBeforeNeckMiner) ? nmSysCalls : guinessSysCalls;

      // difference the earlier syscalls from the later syscalls
      // to get the syscalls that take place between the two candidates
      std::vector<tape::SyscallStart const *> syscallDiff;
      std::set_difference(later.begin(), later.end(), earlier.begin(),
                          earlier.end(),
                          std::inserter(syscallDiff, syscallDiff.begin()));

      spdlog::error("guiness is {} neck-miner",
                    (guinessBeforeNeckMiner) ? "before" : "after");
      auto syscallCount = syscallDiff.size();
      auto scoreBetween = guiness::getSyscallGoodness(syscallDiff);
      // If there is no good IO between the candidates
      if (scoreBetween <= 0) {
        // go with the earlier
        spdlog::error("NEGATIVE GOODNESS: {}", scoreBetween);
        useGuinessResults = guinessBeforeNeckMiner;
      } else {
        // otherwise there is good IO so if
        if (guinessBeforeNeckMiner) {
          // guiness then neck-miner
          // suggests there is something between that specialization can't
          // handle
          spdlog::error("G->NM");
          useGuinessResults = false;
        } else {
          // neck-miner then guiness
          spdlog::error("NM->G");
          useGuinessResults = true;
        }
      }
    } else {
      // Fallback to guiness if the NM neck is not in the tape
      spdlog::warn("NM not found in tape");
      useGuinessResults = !nmSelectedNeck;
    }
    */

    // Simple guiness selection
    // make sure guiness neck exists otherwise use NM
    if (guinessInstWithStackIter == insts.end()) {
      spdlog::debug("GuiNeSS failed to find a neck. Not using GuiNeSS...");
      useGuinessResults = false;
    }
    // Use guiness in cases where neck-miner failed to find a neck
    // Or the neck is not in the candidate BBs that Guiness identified
    else if (!nmSelectedNeck || !nmTapeBB) {
      spdlog::debug("Neck-miner failed to find a neck or the neck is not on "
                    "the tape. Using GuiNeSS...");
      useGuinessResults = true;
      // TODO: The below is a stop gap for the larger problem where
      // tapes can terminate in usage branches due to specializing
      // without all of the required arguments.
      // Abandon any plans to use the guiness neck if we can detect
      // that guiness may have run using partial command line args (by
      // ending on a usage exit of the target program)
      auto lastInst = insts[insts.size() - 1].inst;
      if (lastInst->getParent()->getParent()->getName() == "usage") {
        spdlog::warn("Trashing guiness results because the last instruction is "
                     "in 'usage'.");
        useGuinessResults = false;
      }
    }
    // otherwise attempt to select between the two necks
    else {
      // Simply use neck-miner in cases where both candidates are valid
      // candidates
      spdlog::debug("Using neck-miner");
      useGuinessResults = false;
      /*
      spdlog::error("Comparing GuiNess vs Neck-miner necks");

      // flag for if guiness neck is before NM neck
      bool guinessBeforeNeckMiner =
          guinessInstWithStackIter < nmInstWithStackIter;
      // grab the syscalls for each candidate
      auto nmSysCalls = nmInstWithStackIter->syscallsSoFar;
      auto guinessSysCalls = guinessInstWithStackIter->syscallsSoFar;

      // find earlier and later syscall sets
      auto earlier = (guinessBeforeNeckMiner) ? guinessSysCalls : nmSysCalls;
      auto later = (guinessBeforeNeckMiner) ? nmSysCalls : guinessSysCalls;

      // difference the earlier syscalls om the later syscalls
      // to get the syscalls that take place between the two candidates
      std::vector<tape::SyscallStart const *> syscallDiff;
      std::set_difference(later.begin(), later.end(), earlier.begin(),
                          earlier.end(),
                          std::inserter(syscallDiff, syscallDiff.begin()));

      // syscallDiff contains the syscalls between the two candidates
      spdlog::error("guiness is {} neck-miner",
                    (guinessBeforeNeckMiner) ? "before" : "after");
      auto syscallCount = syscallDiff.size();
      // the score between the two candidates
      auto scoreBetween = guiness::getSyscallGoodness(syscallDiff);
      // NM score
      int nmTotalScore =
          guiness::getSyscallGoodness(nmInstWithStackIter->syscallsSoFar);
      // Guiness score
      // auto guinessTotalScore =
      // guiness::getSyscallGoodness(guinessInstWithStackIter->syscallsSoFar);

      // TODO set useGuinessResults based on some heuristics around
      // the two candidate necks from NM and GuiNeSS
      useGuinessResults = nmTotalScore < 0;
      */
    }

    if (useGuinessResults) {
      spdlog::debug("Stack trace at GuiNeSS neck:");
      guinessInstWithStackIter->debugLogStackTrace();
      auto inst = guinessInstWithStackIter->inst;
      size_t instIndex = 0;
      auto instIter = inst->getParent()->begin();
      while (inst != &*instIter) {
        instIndex++;
        instIter++;
      }

      spdlog::info("Overriding neck-miner with guiness results.");
      auto guinessBB = inst->getParent();
      finalInstIdx = instIndex;

      finalBasicBlockId = guiness::getBasicBlockAnnotation(guinessBB);
      if (!finalBasicBlockId) {
        throw std::logic_error{
            fmt::format("FinalBB {} in {} didn't have an ID, despite "
                        "containing the neck instruction?",
                        *guinessBB, *guinessBB->getParent())};
      }
      finalFuncName.emplace(guinessBB->getParent()->getName().str());
      finalBBName.emplace(
          neckid::getBBName(const_cast<llvm::BasicBlock *>(guinessBB)));
    }
  }

  if (finalBasicBlockId && finalInstIdx && finalBBName && finalFuncName) {
    const NeckMinerOutput results = {*finalFuncName, *finalBBName,
                                     *finalBasicBlockId, *finalInstIdx};
    FinalOutput.emplace(results);
  }
  finiPass(passID);
}

// Given a neck candidate, find the nearest choke point horizon that is
// the future execution boundary of that neck. If the returned set is
// empty, then there were no choke points in the CFG between the neck
// and the leaves of the function.
void NeckAnalysis::pushNeckToFutureChokePointHorizon(
    llvm::BasicBlock *neck,
    std::unordered_set<llvm::BasicBlock *> &chokePointHorizon) {

  std::deque<llvm::BasicBlock *> queue;
  ObservedBBs observed;

  chokePointHorizon.clear();

  // BFS from the neck the to choke point horizon, if any, and only in
  // this function. If the neck is already a choke point, this just
  // halts immediately and inserts the choke point into the horizon..

  queue.push_back(neck);
  observed.insert(neck);

  while (queue.size() > 0) {
    llvm::BasicBlock *visiting = queue.front();
    queue.pop_front();

    if (ChokePoints.count(visiting) > 0) {
      chokePointHorizon.insert(visiting);
      // NOTE: We never expand the choke point, but add it to the
      // horizon we're observing.
      continue;
    }

    // otherwise, we expand the visiting node and keep going.
    if (!llvm::succ_empty(visiting)) {
      for (auto itr = llvm::succ_begin(visiting);
           itr != llvm::succ_end(visiting); ++itr) {
        llvm::BasicBlock *succ = *itr;
        if (observed.count(succ) > 0) {
          continue;
        }
        observed.insert(succ);
        queue.push_back(succ);
      }
    }
  }
}

// Neck candidates which aren't choke points are pushed forward to
// the nearest future choke point horizon.
void NeckAnalysis::passPushNeckCandidatesToNearestFutureChokePointHorizon(
    int &passID) {
  std::unordered_set<llvm::BasicBlock *> NCCopy;
  std::unordered_set<llvm::BasicBlock *> chokePointHorizon;

  initPass(passID, __FUNCTION__);

  // 1. Make a copy of the currently known neck candidates.
  NCCopy.insert(NeckCandidates.begin(), NeckCandidates.end());

  // 2. For each copied neck candidate, Push it to the next choke point
  // horizon and then update the candidates accordingly.
  for (auto *nc : NCCopy) {

    if (ChokePoints.count(nc) > 0) {
      // The nc is already at a choke point, great!
      continue;
    }

    pushNeckToFutureChokePointHorizon(nc, chokePointHorizon);

    if (chokePointHorizon.size() > 0) {
      // Old neck candidate is no longer a neck candidate.
      NeckCandidates.erase(nc);
      addAnnotation(nc, makeAnnotation(REFUTE_BOUNDARY, passID));
      // Add in the new choke points as neck candidates.
      for (auto *cp : chokePointHorizon) {
        auto result = NeckCandidates.insert(cp);
        if (result.second) {
          addAnnotation(cp, makeAnnotation(ASSERT_BOUNDARY, passID));
        }
      }
      continue;
    }

    // TODO: If the neck candidate is not on a chokepoint and can't be
    // pushed to any future chokepoint (because the future of the
    // function was simply the return paths from it, I'm not entirely
    // sure what to do. We'll be conservative and drop it as a neck.
    NeckCandidates.erase(nc);
    addAnnotation(nc, makeAnnotation(REFUTE_BOUNDARY, passID));
  }

  finiPass(passID);
}

/// This is the complete dataflow of the toplevel neck miner algorithm.
bool NeckAnalysis::theNewAlgorithm(int &passID) {
  BCFGGraph undCFG;

  // ////////////////////////
  // Collect the initial set of reachable Neck Candidates from the Taint
  // Analysis.  These may or may not be legal upon discovery.
  // ////////////////////////

  // Side Effects:
  // UserBranchAndCompInstructions
  // PassAnnotation
  // TaintedBasicBlocks
  passAcquireAllTaintedBasicBlocks(passID);

  // Side Effects:
  // TaintedFunctions
  passAcquireAllTaintedFunctions(passID);

  // Side Effects:
  // PassAnnotation
  // NeckCandidates
  if (!passInitializeNeckCandidatesFromTaintAnalysis(passID)) {
    spdlog::info("No neck candidates found from data-analysis.");
    return false;
  }

  // Side Effects:
  // PassAnnotations
  // NeckCandidates
  passFlowNeckCandidatesInLoopsToEndOfLoop(passID);

  // Side Effects:
  // TaintedCalleeToCallerMap
  // ParticipatingFunctions
  passConstructParticipatingFunctionsFromTaintedFunctions(passID);

  // Side Effects
  // The internal LLVM CFGs for each ParticipatingFunction.
  // PassAnnotations
  // TaintedBasicBlocks
  // NeckCandidates
  passTailDuplicateParticipatingFunctions(passID);

  // Side Effects
  // PassAnnotations
  // TaintedLoops
  passMarkTaintedLoops(passID);

  passExtractAllArticulationPoints(passID);

  // TODO: Fix this to make entry BBs in all participating functions a
  // choke point.
  passSelectChokePoints(passID);

  // The IsFullDominator algorithm gets loops wrong, so we have a clean
  // up pass here to remove those errors.
  passRemoveLoopChokePoints(passID);

  // Here, push all neck candidates if they are not already in a choke
  // point, to the nearest future choke point(s). If more than one choke
  // point is discovered in the pushing they all become neck candidates.
  passPushNeckCandidatesToNearestFutureChokePointHorizon(passID);

  // If a neck candidate is a choke point that is also a "book keeping"
  // (a block that contains a single instruction which is a single jump
  // to a future block which is ALSO a choke point) then move the neck
  // candidate to that new block. Iterate this algorithm until no more
  // changes happen.
  passFlowNeckCandidatesThroughBookKeepingBBs(passID);

  passCollectLoopPassthroughMap(passID);

  passCollectGetOptAndFriendsMap(passID);

  passCollectAllParticipatingBBs(passID);

  passCollectAllBasicBlockAttributes(passID);

  passExecuteNeckSearch(passID);

  dumpPassAnnotations();

  return false;
}

/// Computes neck candidates and the definitive neck.
NeckAnalysis::NeckAnalysis(llvm::Module &M, const std::string &TaintConfigPath,
                           bool FunctionLocalPTAwoGlobals,
                           bool UseSimplifiedDFA,
                           const std::string &FunctionName,
                           const std::optional<std::string> &CombinedPath,
                           const std::optional<std::string> &TapePath,
                           bool Debug)
    : M(M), TA(M, TaintConfigPath, FunctionLocalPTAwoGlobals, UseSimplifiedDFA,
               Debug),
      NeckBasicBlock(nullptr), Main(M.getFunction(FunctionName)), Debug(Debug) {
  int passID = 0;

  CombinedModulePath = CombinedPath;
  GuiNeSSTapePath = TapePath;

  // Found in NeckCruft.cpp
  // theOriginalAlgorithm(passID);

  theNewAlgorithm(passID);
}

llvm::Module &NeckAnalysis::getModule() { return M; }

/// Returns the set of neck candidate.
std::unordered_set<llvm::BasicBlock *> NeckAnalysis::getNeckCandidates() {
  return NeckCandidates;
}

std::unordered_set<llvm::BasicBlock *> NeckAnalysis::getTaintedBasicBlocks() {
  return TaintedBasicBlocks;
}

std::unordered_set<llvm::BasicBlock *> NeckAnalysis::getArticulationPoints() {
  return ArticulationPoints;
}

std::unordered_set<llvm::BasicBlock *> NeckAnalysis::getChokePoints() {
  return ChokePoints;
}

std::unordered_map<llvm::BasicBlock *, AttributeMap> &
NeckAnalysis::getBasicBlockAttributeMap() {
  return BB_AttrMaps;
}

ParticipatingFunctionsSet &NeckAnalysis::getParticipatingFunctions() {
  return ParticipatingFunctions;
}

std::unordered_map<llvm::Function *, AttributeMap> &
NeckAnalysis::getForbiddenMemoCache() {
  return ForbiddenMemoCache;
}

std::unordered_set<llvm::BasicBlock *>
NeckAnalysis::getUserBranchAndCompInstructions() {
  return UserBranchAndCompInstructions;
}

std::unordered_map<llvm::BasicBlock *, std::deque<std::string>>
NeckAnalysis::getPassAnnotations() {
  return PassAnnotations;
}

/// Returns the definite neck or nullptr, if no neck could be found.
NeckMinerResults NeckAnalysis::getNeck() {
  return {NeckBasicBlock, NeckInstructionIndex};
}

BB_BFS_Q_Path &NeckAnalysis::getNeckPath() { return NeckPath; }

std::optional<const NeckMinerOutput> NeckAnalysis::getOutput() {
  return FinalOutput;
}

void NeckAnalysis::dumpModule(llvm::raw_ostream &OS) { OS << M; }

} // namespace neckid
