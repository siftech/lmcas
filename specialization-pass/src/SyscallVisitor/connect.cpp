// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallConnect &tapeEntry) {
  uint64_t enoent_out = -static_cast<int64_t>(ENOENT);
  auto fd = checkFdArg("connect", 0, tapeEntry);
  auto sockaddr = checkNonnullPtrArg("connect", 1, "sockaddr");
  auto addrlen = checkArg("connect", 2, tapeEntry, "addrlen",
                          &tape::SyscallConnect::addrlen);
  if (tapeEntry.return_code == enoent_out) {
    // connect was called, and failed with ENOENT, which we mocked earlier for
    // cases involving AF_UNIX.
    return builder.getInt64(enoent_out);
  } else if (didSyscallFail(tapeEntry)) {
    // connect was called, and failed for another reason
    spdlog::error("TODO: Handle failing connect with unhandled error: {}",
                  tapeEntry);
    throw std::runtime_error{"TODO"};
  } else {
    // connect was called and succeeded.
    spdlog::error("TODO: Handle connect succeeding: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }
}
