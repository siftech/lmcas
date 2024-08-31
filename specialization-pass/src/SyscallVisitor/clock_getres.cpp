// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallClockGetres &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing clock_getres: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else if (tapeEntry.data) {
    // Get the arguments, checking that the clock_id argument was equal to the
    // one in the tape.
    checkArg("clock_getres", 0, tapeEntry, "clock_id",
             &tape::SyscallClockGetres::which_clock);
    auto res = checkNonnullPtrArg("clock_getres", 1, "res");

    // Store the saved value to the argument pointer and return the original
    // return code.
    makeStore(tapeEntry.data.value(), res);
    return builder.getInt64(tapeEntry.return_code);

  } else {
    // It's apparently legal to call clock_getres with a null pointer. In this
    // case, we just check that the arguments matched and return the original
    // return code.
    checkArg("clock_getres", 0, tapeEntry, "clock_id",
             &tape::SyscallClockGetres::which_clock);
    checkArg("clock_getres", 1, tapeEntry, "res", 0);
    return builder.getInt64(tapeEntry.return_code);
  }
}
