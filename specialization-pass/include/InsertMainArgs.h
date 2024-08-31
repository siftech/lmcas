// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__INSERT_MAIN_ARGS_H
#define TABACCO__INSERT_MAIN_ARGS_H

#include <absl/container/flat_hash_map.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/IRBuilder.h>

/**
 * Replaces argv[0] and the environment variables with the values specified in
 * the debloating specification.
 *
 * Returns a tuple of two instruction pointers:
 *   - The first is the new argc value.
 *   - The first is the new argv value.
 */
std::tuple<llvm::Value *, llvm::Value *>
insertMainArgs(llvm::Module &mod, llvm::Argument *realArgc,
               llvm::Argument *realArgv,
               const std::vector<std::string> &specArgs,
               const absl::flat_hash_map<std::string, std::string> &specEnv,
               llvm::IRBuilder<> &builder);

#endif
