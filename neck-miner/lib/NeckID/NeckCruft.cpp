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

#include "spdlog/spdlog.h"

/// DO NOT CODE REVIEW ANYTHING IN THIS FILE.
///
/// The code in here contains most of the original UW-Wisc algorithm plus
/// a set of our initial (and deeply intertwined) enhancements to it--most
/// of which we don't use anymore. The functions and methods in this file
/// are things we're *pretty sure* we don't use but are keeping around for
/// comparing the new algorithm against the old.
///
/// NOTE: We do have to break a few connections, especially with the CFG
/// visualization, and ensure we never need this code or algorithm
/// anoymore before this stuff can be truely deleted.

namespace neckid {

bool NeckAnalysis::isBackEdge(llvm::BasicBlock *From, llvm::BasicBlock *To,
                              const llvm::DominatorTree *DT) {
  // check if any successor of From dominates To
  // in case of basename, there is a backedge from From to To
  // thus, To will be a successor of From
  // which means, DT->dominates(To, To) which will return True
  for (auto *Succ : llvm::successors(From)) { // NOLINT
    if (DT->dominates(Succ, To)) {
      return true;
    }
  }
  return false;
}

bool NeckAnalysis::dominatesSuccessors(llvm::BasicBlock *BB) {
  if (!BB->getTerminator()->getNumSuccessors()) {
    return false;
  }
  for (auto *BBSucc : llvm::successors(BB)) { // NOLINT
    if (!getDominatorTree(BB).dominates(BB, BBSucc)) {
      return false;
    }
    // if BB dominates BBSucc
    // make sure no edge from BBSucc to BB
    if (isBackEdge(BBSucc, BB, &getDominatorTree(BB))) {
      return false;
    }

    // TODO: check if there is an eventual cycle / closed path from succ to BB
    // isCyclic()
    // TODO: remove all basic blocks that are Loop latches which have a
    // direct/indirect backedge to the loop header
  }

  // or use this function directly.
  // return isFullDominator(BB, &DT);
  return true;
}

bool NeckAnalysis::hasSingleSuccessorThatDominatesSucessors(
    llvm::BasicBlock *BB) {
  if (BB->getTerminator()->getNumSuccessors() > 1) {
    return false;
  }
  // get the one successor
  for (auto *BBSucc : llvm::successors(BB)) { // NOLINT
    return dominatesSuccessors(BBSucc);
  }
  return false;
}

/// Breadth-first search starting from a function's entry point.
bool NeckAnalysis::isReachableFromFunctionsEntry(llvm::BasicBlock *Dst,
                                                 llvm::StringRef FunName) {
  auto *Fun = M.getFunction(FunName);
  return isReachableFromFunctionsEntry(Dst, Fun);
}

/// Breadth-first search starting from a function's entry point.
bool NeckAnalysis::isReachableFromFunctionsEntry(llvm::BasicBlock *Dst,
                                                 llvm::Function *Fun) {
  assert(Fun && !Fun->isDeclaration() &&
         "Expected a valid function definition!");
  return isReachable(&Fun->front(), Dst,
                     true /* check inter-procedural reachability */);
}

/// Breadth-first search.
bool NeckAnalysis::isReachable(llvm::BasicBlock *Src, llvm::BasicBlock *Dst,
                               bool InterProcSearch) {
  size_t Dummy = 0;
  return isReachable(Src, Dst, Dummy, InterProcSearch, nullptr);
}

/// Breadth-first search that computes distance.
/// Note: Dist captures only intra-procedural distances.
bool NeckAnalysis::isReachable(llvm::BasicBlock *Src, llvm::BasicBlock *Dst,
                               size_t &Dist, bool InterProcSearch) {
  return isReachable(Src, Dst, Dist, InterProcSearch, nullptr);
}

/// Breadth-first search that computes distance.
/// Note: Dist captures only intra-procedural distances.
/// If Dst is inter-procedurally reachable from Src, the callsite within Src's
/// function for which reachability has been shown is written to the
/// CallSiteBB parameter.
bool NeckAnalysis::isReachable(llvm::BasicBlock *Src, llvm::BasicBlock *Dst,
                               size_t &Dist,
                               [[maybe_unused]] bool InterProcSearch,
                               llvm::BasicBlock **CallSiteBB) {

  // Early exit if possible
  if (Src == Dst) {
    Dist = 0;
    return true;
  }
  // Check if an intraprocedural reachability check is sufficient
  if (Src->getParent() == Dst->getParent()) {
    InterProcSearch = false;
  }
  // Intraprocedural search
  if (!InterProcSearch) {
    // Mark all the vertices as not visited
    std::unordered_map<const llvm::BasicBlock *, bool> Visited;
    std::unordered_map<const llvm::BasicBlock *, size_t> Distances;
    size_t max = std::numeric_limits<size_t>::max();

    // mark all nodes as unvisited and a distance of infinity
    for (auto &BB : *Src->getParent()) {
      Visited[&BB] = false;
      Distances[&BB] = max;
    }

    // mark the start node as a distance of 0
    Visited[Src] = true;
    Distances[Src] = 0;

    // if the start node is the end node then return 0
    if (Src == Dst) {
      Dist = Distances[Src];
      return true;
    }

    // create queue and push the start node on
    std::deque<const llvm::BasicBlock *> Queue;
    Queue.push_back(Src);

    while (!Queue.empty()) {
      // dequeue a node from queue
      const llvm::BasicBlock *Cur = Queue.front();
      Queue.pop_front();

      // return true and distance if the current node is the destination
      if (Cur == Dst) {
        Dist = Distances[Cur];
        return true;
      }

      // for all children
      for (const llvm::BasicBlock *Succ : llvm::successors(Cur)) {
        // only process unvisited children
        if (Visited[Succ]) {
          continue;
        }
        // mark the child as visited
        Visited[Succ] = true;
        // assign the child the current distance plus one
        Distances[Succ] = Distances[Cur] + 1;
        // queue the child for processing
        Queue.push_back(Succ);
      }
    }
    // If BFS is complete without visiting Dst
    return false;
  }
  // Interprocedural search
  // Find all reachable call sites
  auto CallSites = getReachableCallSites(Src);
  // Keep track of callsites that we already visited
  std::unordered_set<const llvm::Instruction *> VisitedCallSites;
  // Use a worklist to find call chains that lead to Dst
  for (auto *CallSite : CallSites) {
    auto Callees = TA.getLLVMBasedICFG().getCalleesOfCallAt(CallSite);
    std::vector<const llvm::Function *> WorkList(Callees.begin(),
                                                 Callees.end());
    while (!WorkList.empty()) {
      auto *Callee = WorkList.back();
      WorkList.pop_back();
      if (Callee == Dst->getParent()) {
        if (CallSiteBB) {
          *CallSiteBB = CallSite->getParent();
        }
        return true;
      }
      auto Callers = TA.getLLVMBasedICFG().getCallsFromWithin(Callee);
      for (auto *Caller : Callers) {
        auto Callees = TA.getLLVMBasedICFG().getCalleesOfCallAt(Caller);
        // Only add to worklist if we did not analyze a callsite already
        if (VisitedCallSites.find(Caller) == VisitedCallSites.end()) {
          // WorkList.push_back(Caller);
          std::copy(Callees.begin(), Callees.end(),
                    std::back_inserter(WorkList));
        }
        VisitedCallSites.insert(Caller);
      }
    }
    VisitedCallSites.clear();
  }
  return false;
}

std::unordered_set<llvm::Instruction *>
NeckAnalysis::getReachableCallSites(llvm::BasicBlock *Src) {
  // Collect all basic blocks in the Src Function that call a function
  std::unordered_set<llvm::Instruction *> CallSiteBBs;
  for (auto &FunBB : *Src->getParent()) {
    for (auto &I : FunBB) {
      if (llvm::isa<llvm::CallBase>(&I)) {
        CallSiteBBs.insert(&I);
      }
    }
  }
  // Check which basic blocks are reachable from Src
  std::unordered_set<llvm::Instruction *> ReachableCallSiteBBs;
  for (auto *CallSiteBB : CallSiteBBs) {
    if (isReachable(Src, CallSiteBB->getParent(), false)) {
      ReachableCallSiteBBs.insert(CallSiteBB);
    }
  }
  return ReachableCallSiteBBs;
}

bool NeckAnalysis::succeedsLoop(llvm::BasicBlock *BB) {
  for (auto *Loop : getLoopInfo(BB)) {
    auto Blocks = Loop->getBlocks();
    for (auto *Block : Blocks) {
      size_t Dist = 0;
      // FIXME update once we have an inter-procedural search
      if (isReachable(Block, BB, Dist, false, nullptr)) {
        return true;
      }
    }
  }
  return false;
}

// NOTE: currently consider BB's in main()
void NeckAnalysis::passAnnotateGetoptAndFriends(int &passID) {
  int nc_before_pass = NeckCandidates.size();
  int num_goaf_calls = 0;
  // NOTE: We don't change NeckCandidates in this pass.
  int nc_after_pass = NeckCandidates.size();
  std::unordered_set<std::string> all_goaf;

  initPass(passID, __FUNCTION__);

  // TODO WARNING: Currently assumes that the shared libraries are NOT in the
  // whole program LL file. When they are added we'll have to do probably more
  // work in deterining the linkage habits of the getopt family functions..

  // 0. Define set: getopt(), getopt_long, getopt_long_only()
  // TODO: Read from command line or a config file too.
  all_goaf.insert("getopt");
  all_goaf.insert("getopt_long");
  all_goaf.insert("getopt_long_only");
  all_goaf.insert("rpl_getopt");
  all_goaf.insert("rpl_getopt_long");
  all_goaf.insert("rpl_getopt_long_only");

  // 1. Filter the goaf functions into internal and external linkage sets.
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

  // 2. Determine if any current NeckCandidate happen to also call a GOAF.
  for (auto *BB : NeckCandidates) {
    for (auto itr = BB->begin(); itr != BB->end(); ++itr) {
      llvm::CallInst *inst = llvm::dyn_cast<llvm::CallInst>(itr);
      if (!inst) {
        continue;
      }

      llvm::Function *func = inst->getCalledFunction();
      // TODO inspect this closer
      if (!func) {
        // Skip if indirect function call
        continue;
      }

      std::string name = func->getName().str();
      if (external_goaf.count(name) == 0) {
        // Skip if internal GOAF call
        continue;
      }

      // Finally, we found an instruction that called an external getopt
      // and family function. Count the unique ones we find.
      auto ret = GetOptAndFriends.insert(
          const_cast<llvm::BasicBlock *>(inst->getParent())); // NOLINT
      if (ret.second) {
        num_goaf_calls++;
      }
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

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Number of Neck Candidates calling a GOAF: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, num_goaf_calls, nc_after_pass);

  finiPass(passID);
}

// Remove all neck candidates that are inter-procedurally (transitively)
// reachable from main and instead add their origin in main, i.e., the basic
// block that kicks off the call chain that leads to a neck candidate.
void NeckAnalysis::passContractIntraproceduralNeckCandidatesIntoMain(
    int &passID) {
  std::unordered_set<llvm::BasicBlock *> ToErase, ToInsert;
  int nc_in_main = 0;
  int nc_outside_main = 0;
  int nc_contracted = 0;
  int nc_before_pass = NeckCandidates.size();
  int nc_after_pass = 0;

  initPass(passID, __FUNCTION__);

  assert(Main && "Expected to find 'main' function!");

  for (auto *NeckCandidate : NeckCandidates) {
    // Basic blocks within the program's entry point function are fine
    if (NeckCandidate->getParent() == Main) {
      nc_in_main++;
      continue;
    }

    nc_outside_main++;

    // Replace inter-procedurally reachable neck candidates with their
    // respective call-site basic block in the program's entry point function
    llvm::BasicBlock *CallSiteBB = nullptr;
    size_t Dummy;
    if (isReachable(&Main->front(), NeckCandidate, Dummy, true, &CallSiteBB)) {
      ToErase.insert(NeckCandidate);
      ToInsert.insert(CallSiteBB);
      nc_contracted++;
    }
  }

  for (auto Erase : ToErase) {
    int num = NeckCandidates.erase(Erase);
    if (num > 0) {
      addAnnotation(Erase, makeAnnotation(REFUTE_BOUNDARY, passID));
    }
  }

  for (auto Insert : ToInsert) {
    auto ret = NeckCandidates.insert(Insert);
    if (ret.second) {
      addAnnotation(Insert, makeAnnotation(ASSERT_BOUNDARY, passID));
    }
  }

  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Neck Candidates in main(): {}\n"
               "  Neck Candidates outside of main(): {}\n"
               "  Neck Candidates contracted into main(): {}\n"
               "  Neck Candidates after pass: {}\n",
               nc_before_pass, nc_in_main, nc_outside_main, nc_contracted,
               nc_after_pass);

  if (Debug) {
    spdlog::debug("Neck candidates after intra-procedural 'main' reduction:");
    spdlog::debug(getBBListAsString(NeckCandidates));
  }

  finiPass(passID);
}

/// Next, compensate for poor LLVM IR generation in case we have
/// single-instruction basic blocks that comprises an unconditional jump to the
/// next basic block.
/// TODO: No, it actually doesn't! It just does a single step. So, if there
/// is a chain of unconditional jump BBs, this will do the wrong thing. FIXME.
void NeckAnalysis::passFlowNeckCandidatesThroughUnconditionalBranches(
    int &passID) {
  std::unordered_set<llvm::BasicBlock *> ToErase, ToInsert;
  int nc_before_pass = NeckCandidates.size();
  int nc_flow_steps = 0;
  int nc_after_pass = 0;

  initPass(passID, __FUNCTION__);

  for (auto *NeckCandidate : NeckCandidates) {
    // TODO: Why are we checking for 1 instruction in the BB...
    if (NeckCandidate->size() == 1) {
      // TODO: ... but then _looping_ across the _single_ instruction?
      // TODO: Are there other things not counted in size()?
      for (auto &Inst : *NeckCandidate) {
        if (auto *Br = llvm::dyn_cast<llvm::BranchInst>(&Inst)) {
          if (!Br->isConditional()) {
            ToInsert.insert(Br->getSuccessor(0));
            ToErase.insert(NeckCandidate);
            nc_flow_steps++;
          }
        }
      }
    }
  }

  for (auto Erase : ToErase) {
    int num = NeckCandidates.erase(Erase);
    if (num > 0) {
      addAnnotation(Erase, makeAnnotation(REFUTE_BOUNDARY, passID));
    }
  }

  for (auto Insert : ToInsert) {
    auto ret = NeckCandidates.insert(Insert);
    if (ret.second) {
      addAnnotation(Insert, makeAnnotation(ASSERT_BOUNDARY, passID));
    }
  }
  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Number of Flow Steps: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, nc_flow_steps, nc_after_pass);

  if (Debug) {
    spdlog::debug("  Neck candidates after IR cleanup:");
    spdlog::debug(getBBListAsString(NeckCandidates));
  }

  finiPass(passID);
}

/// Remove all basic blocks that do not dominate their successors
/// Due to some funny LLVM IR construct (cf. kill), we can have a case in which
/// a BB does not dominate its single successor, but its single success does.
/// In that case, we add this single success that dominates its successor to
/// the set of potential neck candidates.
void NeckAnalysis::passRemoveNeckCandidatesThatDontDominateSuccessors(
    int &passID) {
  std::unordered_set<llvm::BasicBlock *> TransitiveDominators;
  int nc_before_pass = NeckCandidates.size();
  int nc_transitive_dominators = 0;
  int nc_after_pass = 0;

  initPass(passID, __FUNCTION__);

  for (auto *BB : NeckCandidates) {
    if (!dominatesSuccessors(BB) &&
        hasSingleSuccessorThatDominatesSucessors(BB)) {
      nc_transitive_dominators++;
      // add single successor
      // TODO: Why is this a loop? Other than API convenience?
      for (auto *BBSucc : llvm::successors(BB)) {
        TransitiveDominators.insert(BBSucc);
      }
    }
  }

  eraseIf(NeckCandidates, [this, passID](auto *BB) {
    bool no_dom = !dominatesSuccessors(BB);
    if (no_dom) {
      addAnnotation(BB, makeAnnotation(REFUTE_BOUNDARY, passID));
    }
    return no_dom;
  });

  for (auto Insert : TransitiveDominators) {
    auto ret = NeckCandidates.insert(Insert);
    if (ret.second) {
      addAnnotation(Insert, makeAnnotation(ASSERT_BOUNDARY, passID));
    }
  }

  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Transitive Dominators: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, nc_transitive_dominators, nc_after_pass);

  if (Debug) {
    spdlog::debug("  Neck candidates after handling dominators:");
    spdlog::debug(getBBListAsString(NeckCandidates));
  }

  finiPass(passID);
}

/// Remove all basic blocks that do not succeed a loop
void NeckAnalysis::passRemoveNeckCandidatesThatDoNotSucceedALoop(int &passID) {
  int nc_before_pass = NeckCandidates.size();
  int nc_succeeds_loop = 0;
  int nc_no_succeeds_loop = 0;
  int nc_after_pass = 0;

  initPass(passID, __FUNCTION__);

  eraseIf(NeckCandidates,
          [this, &nc_no_succeeds_loop, &nc_succeeds_loop, passID](auto *BB) {
            bool succeeds_loop = succeedsLoop(BB);
            succeeds_loop ? nc_succeeds_loop++ : nc_no_succeeds_loop++;
            // Remove if we _don't_ succeed the loop!
            if (!succeeds_loop) {
              addAnnotation(BB, makeAnnotation(REFUTE_BOUNDARY, passID));
            }
            return !succeeds_loop;
          });

  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Neck Candidates that don't succeed a loop: {}\n"
               "  Neck Candidates that do succeed a loop: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, nc_no_succeeds_loop, nc_succeeds_loop,
               nc_after_pass);

  if (Debug) {
    spdlog::debug("Neck candidates after handling loop succession:");
    spdlog::debug(getBBListAsString(NeckCandidates));
  }

  finiPass(passID);
}

/// Remove all basic blocks that are not reachable from main
/// NOTE: This probably removes indirect control flows, which may be in fact
/// too conservative, but probably ok for now.
void NeckAnalysis::passRemoveNeckCandidatesNotReachableFromMain(int &passID) {
  int nc_before_pass = NeckCandidates.size();
  int nc_reachable = 0;
  int nc_not_reachable = 0;
  int nc_after_pass = 0;

  initPass(passID, __FUNCTION__);

  eraseIf(NeckCandidates,
          [this, &nc_reachable, &nc_not_reachable, passID](auto *BB) {
            bool reachable = isReachableFromFunctionsEntry(BB, Main);
            reachable ? nc_reachable++ : nc_not_reachable++;
            // Remove if NOT reachable!
            if (!reachable) {
              addAnnotation(BB, makeAnnotation(REFUTE_BOUNDARY, passID));
            }
            return !reachable;
          });
  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Neck Candidates reachable from main: {}\n"
               "  Neck Candidates not reachable from main: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, nc_reachable, nc_not_reachable, nc_after_pass);

  if (Debug) {
    spdlog::debug("Neck candidates after handling reachability from 'main':");
    spdlog::debug(getBBListAsString(NeckCandidates));
  }

  finiPass(passID);
}

void NeckAnalysis::passRemoveNeckCandidatesThatPreceedLastGetoptLoop(
    int &passID) {
  int nc_before_pass = NeckCandidates.size();
  int nc_before_getopts = 0;
  int nc_after_pass = 0;
  size_t distance = 0;
  size_t max_distance = 0;
  std::unordered_set<llvm::BasicBlock *> Descendants;

  initPass(passID, __FUNCTION__);

  if (GetOptAndFriends.empty()) {
    spdlog::info("Did not find any GetOptAndFriends");
  } else {
    // Find common descendants of GetOptAndFriends
    findCommonDescendants(GetOptAndFriends, Descendants);
    // Remove candidates that are not in the common descendant
    eraseIf(NeckCandidates,
            [Descendants, &nc_before_getopts](auto *NeckCandidate) {
              if (Descendants.count(NeckCandidate) == 0) {
                nc_before_getopts++;
                return true;
              }
              return false;
            });
  }

  nc_after_pass = NeckCandidates.size();

  spdlog::info("  Neck Candidates before pass: {}\n"
               "  Distance to farthest getopt* callsite in main: {}\n"
               "  Size of GetOptAndFriends: {}\n"
               "  Neck candidates before a getopt*: {}\n"
               "  Neck Candidates after pass: {}",
               nc_before_pass, max_distance, GetOptAndFriends.size(),
               nc_before_getopts, nc_after_pass);

  finiPass(passID);
}

llvm::BasicBlock *
NeckAnalysis::closestNeckCandidateReachableFromEntry() { // NOLINT
  size_t ShortestDist = std::numeric_limits<size_t>::max();
  llvm::BasicBlock *Closest = nullptr;
  // FIXME (this is not particularly efficient)
  for (auto *NeckCandidate : NeckCandidates) {
    size_t Dist = 0;
    assert(Main && !Main->isDeclaration() &&
           "Expected to find a 'main' function!");
    // FIXME update once we have an inter-procedural search
    if (isReachable(&Main->getEntryBlock(), NeckCandidate, Dist, false,
                    nullptr)) {
      if (Dist < ShortestDist) {
        ShortestDist = Dist;
        Closest = NeckCandidate;
      }
    }
  }
  return Closest;
}

/// Compute the neck candidate that is closed to the program's entry point
void NeckAnalysis::passPickClosestNeckCandidate(int &passID) {
  int nc_before_pass = NeckCandidates.size();

  initPass(passID, __FUNCTION__);

  // Compute the neck candidate that is closed to the program's entry point
  NeckBasicBlock = closestNeckCandidateReachableFromEntry();
  NeckInstructionIndex = 0;

  if (NeckBasicBlock) {
    addAnnotation(NeckBasicBlock, makeAnnotation(ASSERT_NECK, passID));
  }

  spdlog::info("  Neck Candidates before pass: {}", nc_before_pass);

  if (!NeckBasicBlock) {
    spdlog::info("  NO NECK FOUND!");
  } else {
    spdlog::info("  Picked Neck: {}@{}",
                 (*NeckBasicBlock).getParent()->getName(),
                 getBBIdsString(NeckBasicBlock));
  }

  finiPass(passID);
}

void NeckAnalysis::markNeck(const std::string &FunName) {
  if (!NeckBasicBlock) {
    return;
  }
  // Create artificial marker function
  llvm::LLVMContext &CTX = M.getContext();
  llvm::FunctionType *MarkerFunTy =
      llvm::FunctionType::get(llvm::Type::getVoidTy(CTX), false);
  llvm::Function *MarkerFun = llvm::Function::Create(
      MarkerFunTy, llvm::Function::ExternalLinkage, FunName, M);
  llvm::BasicBlock *BB = llvm::BasicBlock::Create(CTX, "entry", MarkerFun);
  llvm::IRBuilder<> Builder(CTX);
  Builder.SetInsertPoint(BB);
  Builder.CreateRetVoid();
  // Add call to the artifical marker function in the basic block that has
  // been identified as neck
  Builder.SetInsertPoint(&NeckBasicBlock->front());
  Builder.CreateCall(MarkerFun);
}

bool NeckAnalysis::theOriginalAlgorithm(int &passID) {

  // Not crufty.
  passAcquireAllTaintedBasicBlocks(passID);

  // Not crufty.
  if (!passInitializeNeckCandidatesFromTaintAnalysis(passID)) {
    spdlog::info("No neck candidates found from data-analysis.");
    return false;
  }

  // Crufty
  passContractIntraproceduralNeckCandidatesIntoMain(passID);

  // Crufty
  passAnnotateGetoptAndFriends(passID);

  // Not crufty.
  passFlowNeckCandidatesInLoopsToEndOfLoop(passID);

  // Crufty
  passFlowNeckCandidatesThroughUnconditionalBranches(passID);

  // Crufty
  passRemoveNeckCandidatesThatDontDominateSuccessors(passID);

  // Crufty
  passRemoveNeckCandidatesThatDoNotSucceedALoop(passID);

  // Crufty
  passRemoveNeckCandidatesNotReachableFromMain(passID);

  // Crufty
  passRemoveNeckCandidatesThatPreceedLastGetoptLoop(passID);

  // Crufty
  passPickClosestNeckCandidate(passID);

  // Not crufty.
  dumpPassAnnotations();

  return true;
}

} // namespace neckid
