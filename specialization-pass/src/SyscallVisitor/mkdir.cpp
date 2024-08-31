// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallMkdir &tapeEntry) {
  if ((int64_t)tapeEntry.return_code.value == -(int64_t)EEXIST) {
    // All calls to mkdir should be recorded as "error, directory exists"
    // as we interrupt mkdir calls and do the mkdir but return eexists always.
    // Check the arguments actually provided to the syscall.
    auto pathname = checkStrArg("mkdir", 0, tapeEntry, "pathname",
                                &tape::SyscallMkdir::pathname);
    auto mode =
        checkArg("mkdir", 1, tapeEntry, "mode", &tape::SyscallMkdir::mode);

    // Run syscall, checking that it succeeds or returns EEXIST
    auto expectedReturnSuccess = builder.getInt64(0);
    auto expectedReturnEExists = builder.getInt64(tapeEntry.return_code);
    auto syscallReturn = makeSyscallUnchecked(SYS_mkdir, pathname, mode);
    auto condOr = builder.CreateNot(builder.CreateOr(
        builder.CreateICmpEQ(syscallReturn, expectedReturnSuccess),
        builder.CreateICmpEQ(syscallReturn, expectedReturnEExists)));
    trapIf(condOr,
           fmt::format("mkdir : return code (%#lx) did not match either "
                       "tape-recorded value ({}) or value of success ({})",
                       tapeEntry.return_code.value, 0),
           syscallReturn);
    return expectedReturnEExists;
  } else if (didSyscallFail(tapeEntry)) {
    spdlog::error("mkdir failed with different error than expected {}",
                  tapeEntry);
    throw std::logic_error{"bug: unexpected mkdir error."};
  } else {
    spdlog::error("mkdir should be failing here, as we create the directory "
                  "and return an error from instrumentation-parent: {}",
                  tapeEntry);
    throw std::logic_error{"bug: successful mkdir slipped through."};
  }
}
