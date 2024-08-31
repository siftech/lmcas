// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__SYSCALL_HANDLERS_H
#define TABACCO__SYSCALL_HANDLERS_H

#include "Tape.h"
#include <llvm/IR/IRBuilder.h>

#include <absl/container/flat_hash_map.h>

class SyscallHandlers {
public:
  SyscallHandlers(llvm::IRBuilder<> &builder) : builder(builder) {}

  llvm::Value *
  performSyscall(llvm::CallInst &instruction,
                 const tape::SyscallStart &tapeEntry,
                 std::function<llvm::Value *(llvm::Value *)> translateValue,
                 const absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
                     &annotatedBasicBlocks);

private:
  llvm::IRBuilder<> &builder;
};

#endif
