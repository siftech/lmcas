// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallClockGettime &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing clock_gettime: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }

  // Get the arguments, checking that the clock_id argument was equal to the
  // one in the tape.
  checkArg("clock_gettime", 0, tapeEntry, "clock_id",
           &tape::SyscallClockGettime::which_clock);
  auto res = checkNonnullPtrArg("clock_gettime", 1, "res");

  // Store the saved value to the argument pointer and return the original
  // return code.
  makeStore(tapeEntry.data, res);
  return builder.getInt64(tapeEntry.return_code);
}
