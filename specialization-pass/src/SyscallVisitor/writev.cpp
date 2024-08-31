// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallWritev &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing writev: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  } else {
    // Check that the arguments match what was in the tape.
    auto fd = checkFdArg("writev", 0, tapeEntry);
    auto iovs =
        checkNonnullPtrArg("writev", 1, "iov", nullptr, builder.getInt64Ty());
    auto iovcnt = checkArg("writev", 2, tapeEntry, "iovcnt",
                           &tape::SyscallWritev::iovcnt);
    // Run the syscall, checking the return argument.
    return makeSyscallChecked("writev", SYS_writev, tapeEntry, fd, iovs,
                              iovcnt);
  }
}
