// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__UNROLL_TAPE_H
#define TABACCO__UNROLL_TAPE_H

#include "StackFrame.h"
#include "Tape.h"
#include <absl/container/flat_hash_set.h>
#include <llvm/IR/IRBuilder.h>
#include <regex>

/**
 * "Unrolls" the tape into the given IRBuilder. Returns a stack of maps, mapping
 * instructions from basic blocks run in the tape to instructions that were
 * built with the builder.
 *
 * Instructions from main will be at the front of the vector, instructions from
 * the function that was executing when the neck was reached will be at the back
 * of the vector. The vector will always be non-empty.
 */
std::vector<StackFrame>
unrollTape(tape::Tape const &tape,
           std::vector<std::regex> const &safeExternalFunctionRegexes,
           llvm::Value *argc, llvm::Value *argv, llvm::Function &mainFunction,
           llvm::Function &newMainFunction,
           absl::flat_hash_set<llvm::Instruction *> neckCallMarkers,
           llvm::IRBuilder<> &allocaBuilder, llvm::IRBuilder<> &builder);

#endif
