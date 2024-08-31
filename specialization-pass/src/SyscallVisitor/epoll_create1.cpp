// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallEpollCreate1 &tapeEntry) {
  // Just allowing it through
  return makeSyscallUnchecked(SYS_epoll_create1,
                              builder.getInt64(tapeEntry.flags));
}
