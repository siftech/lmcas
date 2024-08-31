// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallWrite &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing write: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else {
    auto dataConstant =
        llvm::ConstantDataArray::get(builder.getContext(), tapeEntry.data);
    auto dataGlobal = new llvm::GlobalVariable(
        *mod, dataConstant->getType(), true,
        llvm::GlobalValue::LinkageTypes::PrivateLinkage, dataConstant);
    return makeSyscallWithWarning("write", SYS_write, tapeEntry,
                                  builder.getInt64(tapeEntry.fd), dataGlobal,
                                  builder.getInt64(tapeEntry.data.size()));
  }
}
