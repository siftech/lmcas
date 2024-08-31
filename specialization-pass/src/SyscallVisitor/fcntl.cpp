// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallFcntl &tapeEntry) {
  // fcntl has a bunch of different cases, we are at the moment maintaining a
  // subset that we've seen in the wild.
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing fcntl: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else if ((tapeEntry.command == F_SETFD) || (tapeEntry.command == F_GETFD) ||
             (tapeEntry.command == F_SETFL) || (tapeEntry.command == F_GETFL)) {
    // Let the syscall through as-is after checking that the arguments match
    // what was recorded in the tape.
    auto fd = checkFdArg("fcntl", 0, tapeEntry);
    auto command = checkArg("fcntl", 1, tapeEntry, "command",
                            &tape::SyscallFcntl::command);
    auto arg = checkArg("fcntl", 2, tapeEntry, "arg", &tape::SyscallFcntl::arg);
    return makeSyscallChecked("fcntl", SYS_fcntl, tapeEntry, fd, command, arg);

  } else {
    spdlog::error("Unsupported form of fcntl: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }
}
