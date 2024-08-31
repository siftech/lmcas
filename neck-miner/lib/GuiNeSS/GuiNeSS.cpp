// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "GuiNeSS/GuiNeSS.h"
#include "FmtLlvm.h"
#include "GuiNeSS/Annotation.h"
#include "GuiNeSS/TapeWalker.h"
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Instructions.h>
#include <spdlog/spdlog.h>

namespace guiness {

auto findNeckCandidates(Config const &config, llvm::Module &mod,
                        tape::Tape const &tape)
    -> std::vector<InstructionWithStack> {
  // First, find everywhere a neck could possibly be. We overapproximate this by
  // taking the set of all instructions before the program exits.
  spdlog::info("Playing back tape...");
  auto insts = TapeWalker{config, mod, tape}.collect();
  spdlog::info("Tape covers {} instructions", insts.size());

  // Next, find the basic blocks that could possibly contain the neck.
  auto candidateBBs = findCandidateBBs(insts);
  spdlog::info("Found {} candidate BBs", candidateBBs.size());

  // Remove the instructions that could not possibly contain the neck, because
  // they're in BBs that could not be the neck or whose stack-frame call
  // instructions could not be the neck.
  insts.erase(
      std::remove_if(insts.begin(), insts.end(),
                     [&candidateBBs = std::as_const(candidateBBs)](
                         InstructionWithStack const &inst) {
                       if (!candidateBBs.contains(inst.inst->getParent()))
                         return true;

                       for (auto const &stackInst : inst.stack)
                         if (!candidateBBs.contains(stackInst->getParent()))
                           return true;

                       return false;
                     }),
      insts.end());
  spdlog::info("{} instructions were in candidate BBs", insts.size());

  // Sort the instructions by goodness. This is a really primitive heuristic so
  // far.
  std::stable_sort(insts.begin(), insts.end(),
                   [&](auto const &lhs, auto const &rhs) {
                     return getSyscallGoodness(lhs.syscallsSoFar) <
                            getSyscallGoodness(rhs.syscallsSoFar);
                   });

  return insts;
}

auto findBestNeck(Config const &config, llvm::Module &mod,
                  tape::Tape const &tape)
    -> std::optional<InstructionWithStack> {
  auto insts = findNeckCandidates(config, mod, tape);
  if (insts.empty())
    return std::nullopt;
  return std::move(insts[insts.size() - 1]);
}

auto findCandidateBBs(std::vector<InstructionWithStack> const &insts)
    -> absl::flat_hash_set<llvm::BasicBlock *> {

  // Find all mentioned basic blocks, as well as all BBs.
  absl::flat_hash_set<llvm::BasicBlock *> mentionedBBs;
  for (auto const &inst : insts) {
    mentionedBBs.emplace(inst.inst->getParent());
    for (auto const &call : inst.stack)
      mentionedBBs.emplace(call->getParent());
  }
  spdlog::info("Found {} mentioned BBs", mentionedBBs.size());

  // Find which BBs don't reach themselves.
  absl::flat_hash_set<llvm::BasicBlock *> nonLoopBBs;
  for (auto bb : mentionedBBs) {
    auto reachable = findReachableBBs(bb);
    if (!reachable.contains(bb))
      nonLoopBBs.insert(bb);
  }
  spdlog::info("Found {} non-looping BBs", mentionedBBs.size());

  // Filter for BBs in libc.
  absl::flat_hash_set<llvm::BasicBlock *> nonLibcNonLoopBBs;
  for (auto bb : nonLoopBBs) {
    auto annotation = getBasicBlockAnnotation(bb);
    if (annotation.value() >= (1 << 30))
      nonLibcNonLoopBBs.emplace(bb);
  }

  // Filter for BBs that aren't in varargs functions.
  absl::flat_hash_set<llvm::BasicBlock *> nonVarargsBBs;
  for (auto bb : nonLibcNonLoopBBs) {
    if (!bb->getParent()->isVarArg())
      nonVarargsBBs.emplace(bb);
  }

  // Find which instructions' basic blocks, as well as their entire stack, don't
  // reach themselves.
  absl::flat_hash_set<llvm::BasicBlock *> out;
  for (auto const &inst : insts) {
    bool ok = true;
    if (!nonVarargsBBs.contains(inst.inst->getParent())) {
      ok = false;
    }
    for (auto const &call : inst.stack) {
      if (!nonVarargsBBs.contains(inst.inst->getParent())) {
        ok = false;
        goto end;
      }
    }

  end:
    if (ok)
      out.emplace(inst.inst->getParent());
  }

  return out;
}

auto findReachableBBs(llvm::BasicBlock *initialBB)
    -> absl::flat_hash_set<llvm::BasicBlock *> {
  absl::flat_hash_set<llvm::BasicBlock *> black, gray;

  auto initialTerminator = initialBB->getTerminator();
  auto initialSuccessorCount = initialTerminator->getNumSuccessors();
  for (size_t i = 0; i < initialSuccessorCount; i++)
    gray.insert(initialTerminator->getSuccessor(i));

  while (!gray.empty()) {
    absl::flat_hash_set<llvm::BasicBlock *> newGray;
    for (auto bb : gray) {
      if (black.contains(bb))
        continue;

      auto terminator = bb->getTerminator();
      auto successorCount = terminator->getNumSuccessors();
      for (size_t i = 0; i < successorCount; i++)
        newGray.insert(terminator->getSuccessor(i));
    }
    for (auto bb : gray) {
      newGray.erase(bb);
      black.insert(bb);
    }
    gray = std::move(newGray);
  }
  spdlog::trace("{} BBs are reachable from {}", black.size(), *initialBB);
  return black;
}

auto getSyscallGoodness(std::vector<tape::SyscallStart const *> const &syscalls)
    -> int {
  int out = 0;
  for (auto const *syscall : syscalls) {
    // TODO: expand the list!
    if (std::holds_alternative<tape::SyscallIoctl>(*syscall))
      // TODO potentially OBE due to changes in instrumentation parent. Revisit.
      // TODO check if this is an okay ioctl and don't subtract in that case
      out--;
    else if (std::holds_alternative<tape::SyscallSocket>(*syscall))
      out--;
    else if (std::holds_alternative<tape::SyscallClose>(*syscall) ||
             std::holds_alternative<tape::SyscallOpen>(*syscall) ||
             std::holds_alternative<tape::SyscallOpenat>(*syscall))
      // We should check if it's a named config file, maybe? And give a much
      // bigger boost if so.
      out++;
    else if (std::holds_alternative<tape::SyscallRead>(*syscall))
      // TODO: Boost by amount of content read? And maybe only for named config
      // files?
      out++;
  }
  return out;
}

} // namespace guiness
