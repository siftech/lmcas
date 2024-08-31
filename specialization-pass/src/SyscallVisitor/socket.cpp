// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include <asm-generic/errno.h>

llvm::Value *SyscallVisitor::operator()(const tape::SyscallSocket &tapeEntry) {
  uint64_t errorEAFOut = -static_cast<int64_t>(EAFNOSUPPORT);
  if (tapeEntry.return_code == errorEAFOut) {
    // Socket was called, and failed due to being in an unsupported addr family
    return builder.getInt64(errorEAFOut);
  } else if (!didSyscallFail(tapeEntry)) {
    // Syscall didn't fail, make the syscall in a checked manner
    // Pass the syscall through, checking its arguments.
    auto family = checkArg("socket", 0, tapeEntry, "family",
                           &tape::SyscallSocket::family);
    auto type = checkArg("socket", 1, tapeEntry, "type",
                         &tape::SyscallSocket::type_socket);
    auto protocol = checkArg("socket", 2, tapeEntry, "protocol",
                             &tape::SyscallSocket::protocol);
    return makeSyscallChecked("socket", SYS_socket, tapeEntry, family, type,
                              protocol);
  } else {
    // Syscall failed, but with an error condition we havent
    // checked yet.
    spdlog::error("TODO: Handle failing socket with unhandled error: {}",
                  tapeEntry);
    throw std::runtime_error{"TODO"};
  }
}
