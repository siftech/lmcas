// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallPipe &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing pipe: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }

  // Pass the syscall through, checking its arguments.
  auto pipefd =
      checkNonnullPtrArg("pipe", 0, "pipefd", nullptr, builder.getInt32Ty());
  auto syscallResult = makeSyscallChecked("pipe", SYS_pipe, tapeEntry, pipefd);

  // Read the file descriptors out of the pipefd array.
  auto readFdPtr =
      builder.CreateGEP(builder.getInt32Ty(), pipefd, builder.getInt32(0));
  auto writeFdPtr =
      builder.CreateGEP(builder.getInt32Ty(), pipefd, builder.getInt32(1));
  auto readFd = builder.CreateLoad(builder.getInt32Ty(), readFdPtr);
  auto writeFd = builder.CreateLoad(builder.getInt32Ty(), writeFdPtr);

  // Check that the file descriptors matched what we recorded in the tape.
  trapIf(
      builder.CreateICmpNE(readFd, builder.getInt32(tapeEntry.pipefds.at(0))),
      fmt::format("pipe: read fd (%d) did not match tape-recorded value ({})",
                  tapeEntry.pipefds.at(0)),
      readFd);
  trapIf(
      builder.CreateICmpNE(writeFd, builder.getInt32(tapeEntry.pipefds.at(1))),
      fmt::format("pipe: write fd (%d) did not match tape-recorded value ({})",
                  tapeEntry.pipefds.at(1)),
      writeFd);

  // Return the original return code.
  return builder.getInt64(tapeEntry.return_code);
}
