// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef GUINESS__GUINESS_H
#define GUINESS__GUINESS_H

#include "Config.h"
#include "Tape.h"
#include "TapeWalker.h"
#include <llvm/IR/Module.h>

namespace guiness {

/**
 * Finds the instructions that could be candidates to be the neck, sorted from
 * worst to best.
 */
auto findNeckCandidates(Config const &config, llvm::Module &mod,
                        tape::Tape const &tape)
    -> std::vector<InstructionWithStack>;

/**
 * Finds the instruction that is the best candidate to be the neck. The neck
 * should be inserted *before* this instruction.
 */
auto findBestNeck(Config const &config, llvm::Module &mod,
                  tape::Tape const &tape)
    -> std::optional<InstructionWithStack>;

/**
 * Returns the basic blocks that:
 *
 * 1. contain an instruction from the tape
 * 2. have the property that all reachable basic blocks are dominated by this
 *    basic block
 *
 * TODO: taildup
 */
auto findCandidateBBs(std::vector<InstructionWithStack> const &insts)
    -> absl::flat_hash_set<llvm::BasicBlock *>;

/**
 * Returns all basic blocks reachable from the given one.
 *
 * TODO: Be more efficient; perhaps return an iterator?
 */
auto findReachableBBs(llvm::BasicBlock *)
    -> absl::flat_hash_set<llvm::BasicBlock *>;

/**
 * Returns a heuristic for how "good" having been past this set of syscalls is.
 */
auto getSyscallGoodness(std::vector<tape::SyscallStart const *> const &syscalls)
    -> int;

} // namespace guiness

#endif
