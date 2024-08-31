// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallLseek &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing lseek: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }

  // If the file *description* (not descriptor!) was closed before the neck, we
  // can ignore the lseek, since any side effects from it would show up in the
  // recorded values.
  //
  // TODO: Right now we just assume the file description will be closed; we
  // should check this!

  // Return the original return code.
  return builder.getInt64(tapeEntry.return_code);
}
