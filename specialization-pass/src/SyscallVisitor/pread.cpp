// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallPread &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing pread: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else if (tapeEntry.count.value == 0) {
    // If the count argument was zero, we just validate that the file descriptor
    // and count were what we expected, and return the original return code.
    // This is because the buf pointer is permitted to be null if count is zero.
    checkFdArg("pread", 0, tapeEntry);
    checkArg("pread", 2, tapeEntry, "count", &tape::SyscallPread::count);
    return builder.getInt64(tapeEntry.return_code);

  } else {
    // Store the data that was originally read into a global variable.
    auto dataConstant =
        llvm::ConstantDataArray::get(builder.getContext(), tapeEntry.data);
    auto dataGlobal = new llvm::GlobalVariable(
        *mod, dataConstant->getType(), true,
        llvm::GlobalValue::LinkageTypes::PrivateLinkage, dataConstant);

    // Check that the arguments match what was in the tape.
    auto fd = checkFdArg("pread", 0, tapeEntry);
    auto buf = checkNonnullPtrArg("pread", 1, "buf");
    auto count =
        checkArg("pread", 2, tapeEntry, "count", &tape::SyscallPread::count);
    checkArg("pread", 3, tapeEntry, "pos", &tape::SyscallPread::pos);

    // Copy the data into the buffer, up to the length the kernel claimed
    // originally to have stored.
    builder.CreateMemCpy(buf, {}, dataGlobal, {}, tapeEntry.return_code);

    // Return the same return code.
    return builder.getInt64(tapeEntry.return_code);
  }
}
