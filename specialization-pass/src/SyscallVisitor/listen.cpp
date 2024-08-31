// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallListen &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing listen: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }
  // Pass the syscall through, checking its arguments.
  auto fd = checkFdArg("listen", 0, tapeEntry);
  auto backlog = checkArg("listen", 1, tapeEntry, "backlog",
                          &tape::SyscallListen::backlog);
  return makeSyscallChecked("listen", SYS_listen, tapeEntry, fd, backlog);
}
