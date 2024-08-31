// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "SyscallHandlers.h"
#include "FmtLlvm.h"
#include "SyscallVisitor.h"

#include <absl/container/flat_hash_map.h>

llvm::Value *SyscallHandlers::performSyscall(
    llvm::CallInst &instruction, const tape::SyscallStart &tapeEntry,
    std::function<llvm::Value *(llvm::Value *)> translateValue,
    const absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
        &annotatedBasicBlocks) {
  spdlog::debug("Call to syscall wrapper: {}",
                *llvm::cast<llvm::Instruction>(&instruction));
  auto returnValue =
      std::visit(SyscallVisitor{this->builder, instruction, translateValue,
                                annotatedBasicBlocks},
                 tapeEntry);
  if (returnValue->getType() != builder.getInt64Ty()) {
    spdlog::error("BUG: syscall emulation for {} returned a non-i64 value: {}",
                  tapeEntry, *returnValue);
    throw std::logic_error{"BUG"};
  }
  return returnValue;
}
