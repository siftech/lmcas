// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallRead &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing read: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else if (tapeEntry.count.value == 0) {
    // If the count argument was zero, we just validate that the file descriptor
    // and count were what we expected, and return the original return code.
    // This is because the buf pointer is permitted to be null if count is zero.
    checkFdArg("read", 0, tapeEntry);
    checkArg("read", 2, tapeEntry, "count", &tape::SyscallRead::count);
    return builder.getInt64(tapeEntry.return_code);

  } else {
    // Store the data that was originally read into a global variable.
    auto dataConstant =
        llvm::ConstantDataArray::get(builder.getContext(), tapeEntry.data);
    auto dataGlobal = new llvm::GlobalVariable(
        *mod, dataConstant->getType(), true,
        llvm::GlobalValue::LinkageTypes::PrivateLinkage, dataConstant);

    // Check that the arguments match what was in the tape.
    auto fd = checkFdArg("read", 0, tapeEntry);
    auto buf = checkNonnullPtrArg("read", 1, "buf");
    auto count =
        checkArg("read", 2, tapeEntry, "count", &tape::SyscallRead::count);

    // Copy the data into the buffer, up to the length the kernel claimed
    // originally to have stored.
    builder.CreateMemCpy(buf, {}, dataGlobal, {}, tapeEntry.return_code);

    // Return the same return code.
    return builder.getInt64(tapeEntry.return_code);
  }
}
