// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallGeteuid &tapeEntry) {
  return makeSyscallWithWarning("geteuid", SYS_geteuid, tapeEntry);
}