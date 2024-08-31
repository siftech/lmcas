// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallPrlimit &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: prlimit failed: {}", tapeEntry);
    throw "TODO";
  } else if (tapeEntry.pid == 0) {
    // if the PID is 0, we can pass it directly through since 0 refers to
    // self-pid.

    auto pid =
        checkArg("prlimit", 0, tapeEntry, "pid", &tape::SyscallPrlimit::pid);
    auto resource = checkArg("prlimit", 1, tapeEntry, "resource",
                             &tape::SyscallPrlimit::resource);
    auto newlimit = getArgUnchecked(2);
    auto oldlimit = getArgUnchecked(3);
    auto checkReturnValue = makeSyscallChecked(
        "prlimit", SYS_prlimit64, tapeEntry, pid, resource, newlimit, oldlimit);
    if (tapeEntry.newlimit.has_value()) {
      spdlog::info("running newlimit part");
      checkRlimStructPtr(tapeEntry.newlimit.value(),
                         checkNonnullPtrArg("prlimit", 2, "newlimit", nullptr,
                                            builder.getInt64Ty()));
    }
    if (tapeEntry.oldlimit.has_value()) {
      spdlog::info("running oldlimit part");
      checkRlimStructPtr(tapeEntry.oldlimit.value(),
                         checkNonnullPtrArg("prlimit", 3, "oldlimit", nullptr,
                                            builder.getInt64Ty()));
    }
    return checkReturnValue;
  } else {
    spdlog::error("Bad usage of prlimit: {}", tapeEntry);
    throw "TODO";
  }
}
