// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include "Tape.h"
#include <sys/ioctl.h>
#include <sys/syscall.h>

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallRtSigprocmask &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing rt_sigprocmask: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }
  // Pass the syscall through, checking its arguments.
  auto how = checkArg("rt_sigprocmask", 0, tapeEntry, "how",
                      &tape::SyscallRtSigprocmask::how);

  // sigset's type as [i32 x 2] means we can treat it like a single i64.
  auto sigsetType = builder.getInt64Ty();
  llvm::Value *nsetPtr;
  if (tapeEntry.nset) {
    nsetPtr =
        checkNonnullPtrArg("rt_sigprocmask", 1, "nset", nullptr, sigsetType);
  } else {
    nsetPtr = getArgUnchecked(1);
  }

  llvm::Value *osetPtr;
  if (tapeEntry.oset) {
    osetPtr =
        checkNonnullPtrArg("rt_sigprocmask", 2, "oset", nullptr, sigsetType);
  } else {
    osetPtr = getArgUnchecked(2);
  }
  auto sigsetsize = checkArg("rt_sigprocmask", 3, tapeEntry, "sigsetsize",
                             &tape::SyscallRtSigprocmask::sigsetsize);
  return makeSyscallChecked("rt_sigprocmask", SYS_rt_sigprocmask, tapeEntry,
                            how, nsetPtr, osetPtr, sigsetsize);
}
