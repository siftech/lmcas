// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include <sys/ioctl.h>

llvm::Value *SyscallVisitor::operator()(const tape::SyscallIoctl &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    // ioctl was called, and failed for another reason
    spdlog::error("TODO: Handle failing ioctl with unhandled error: {}",
                  tapeEntry);
    throw std::runtime_error{"TODO"};

  } else if (tapeEntry.request.value == FIONBIO) {
    // allow through for FIONBIO
    auto fd = checkFdArg("ioctl", 0, tapeEntry);
    auto request = checkArg("ioctl", 1, tapeEntry, "request",
                            &tape::SyscallIoctl::request);
    auto arg0 = getArgUnchecked(2);

    llvm::Value *arg0asI32;
    uint64_t tapeArg0;
    if (tapeEntry.arg0_contents) {
      auto arg0Ptr =
          checkNonnullPtrArg("ioctl", 2, "arg0", nullptr, builder.getInt32Ty());
      arg0asI32 = builder.CreateLoad(builder.getInt32Ty(), arg0Ptr);
      tapeArg0 = tapeEntry.arg0_contents.value();
    } else {
      arg0asI32 = builder.CreateTrunc(arg0, builder.getInt32Ty());
      tapeArg0 = 0;
    }
    trapIf(
        builder.CreateICmpNE(arg0asI32, builder.getInt32(tapeArg0)),
        fmt::format("ioctl: arg0 (%#lx) did not match tape-recorded value ({})",
                    tapeArg0),
        arg0asI32);
    return makeSyscallChecked("ioctl", SYS_ioctl, tapeEntry, fd, request, arg0);

  } else if (tapeEntry.request.value == TIOCGWINSZ) {
    // Allow through TIOCGWINSZ, if the instrumentation-parent allowed it.
    auto fd = checkFdArg("ioctl", 0, tapeEntry);
    auto request = checkArg("ioctl", 1, tapeEntry, "request",
                            &tape::SyscallIoctl::request);
    auto arg0 = getArgUnchecked(2);
    return makeSyscallUnchecked(SYS_ioctl, fd, request, arg0);

  } else {
    spdlog::error("TODO: Handle ioctl with unsupported operation: {}",
                  tapeEntry);
    throw std::runtime_error{"TODO"};
  }
}
