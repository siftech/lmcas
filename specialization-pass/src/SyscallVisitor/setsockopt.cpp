// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallSetsockopt &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing setsockopt: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }
  // Pass the syscall through, checking its arguments.
  auto fd = checkFdArg("setsockopt", 0, tapeEntry);
  auto level = checkArg("setsockopt", 1, tapeEntry, "level",
                        &tape::SyscallSetsockopt::level);
  auto optname = checkArg("setsockopt", 2, tapeEntry, "level",
                          &tape::SyscallSetsockopt::optname);

  auto optval = checkStrArg("setsockopt", 3, tapeEntry, "optval",
                            &tape::SyscallSetsockopt::optval);

  auto optlen = checkArg("setsockopt", 4, tapeEntry, "optlen",
                         &tape::SyscallSetsockopt::optlen);
  return makeSyscallChecked("setsockopt", SYS_setsockopt, tapeEntry, fd, level,
                            optname, optval, optlen);
}
