// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallFstat &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing fstat: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }

  // Get the arguments, checking that the file descriptor matches the one in the
  // tape.
  checkFdArg("fstat", 0, tapeEntry);
  auto statbuf = checkNonnullPtrArg("fstat", 1, "statbuf");

  // Store the saved value to the argument pointer and return the original
  // return code.
  makeStore(tapeEntry.data, statbuf);
  return builder.getInt64(tapeEntry.return_code);
}
