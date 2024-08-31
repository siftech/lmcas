// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__SYSCALL_WRAPPERS_H
#define TABACCO__SYSCALL_WRAPPERS_H

#include <llvm/IR/Function.h>
#include <optional>

/**
 * If the given function is a syscall wrapper, returns its arity. Otherwise,
 * returns nullopt.
 */
std::optional<int> isSyscallWrapper(llvm::Function &function);

#endif
