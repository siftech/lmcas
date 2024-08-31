// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallSchedGetaffinity &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing sched_getaffinity: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else {
    checkArg("sched_getaffinity", 0, tapeEntry, "pid",
             &tape::SyscallSchedGetaffinity::pid);
    checkArg("sched_getaffinity", 1, tapeEntry, "len",
             &tape::SyscallSchedGetaffinity::len);

    auto affinityPtr = checkNonnullPtrArg("sched_getaffinity", 2, "affinity");

    // Store the data that was originally written into a global variable.
    auto dataConstant =
        llvm::ConstantDataArray::get(builder.getContext(), tapeEntry.affinity);
    auto dataGlobal = new llvm::GlobalVariable(
        *mod, dataConstant->getType(), true,
        llvm::GlobalValue::LinkageTypes::PrivateLinkage, dataConstant);

    // Copy the data into the buffer, up to the length
    // provided
    builder.CreateMemCpy(affinityPtr, {}, dataGlobal, {}, tapeEntry.len);

    // note that
    // return value of a successful sched_getffinity call is nonzero,
    // it returns the number of bytes placed copied into the mask buffer.
    return builder.getInt64(tapeEntry.return_code);
  }
}
