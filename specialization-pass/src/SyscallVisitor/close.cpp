// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallClose &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing close: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }

  // Pass the syscall through, checking its arguments.
  auto fd = checkFdArg("close", 0, tapeEntry);
  return makeSyscallChecked("close", SYS_close, tapeEntry, fd);
}
