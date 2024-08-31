// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallStat &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    // In the case of an error recorded on the tape, we just check the arguments
    // and return the error.
    checkStrArg("stat", 0, tapeEntry, "pathname", &tape::SyscallStat::filename);
    checkNonnullPtrArg("stat", 1, "statbuf");
    return builder.getInt64(tapeEntry.return_code);

  } else {
    // Get the arguments, checking that the file descriptor matches the one in
    // the tape.
    auto pathname = checkStrArg("stat", 0, tapeEntry, "pathname",
                                &tape::SyscallStat::filename);
    auto statbuf = checkNonnullPtrArg("stat", 1, "statbuf");

    // Store the saved value to the argument pointer and return the original
    // return code.
    makeStore(tapeEntry.data, statbuf);
    return builder.getInt64(tapeEntry.return_code);
  }
}
