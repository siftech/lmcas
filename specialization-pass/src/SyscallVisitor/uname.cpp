// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallUname &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing uname: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  } else {
    auto utsnamePtr = checkNonnullPtrArg("uname", 0, "utsname");

    // write sizeof(utsname) bytes from ours
    // Store the data that was originally written into a global variable.
    auto dataConstant =
        llvm::ConstantDataArray::get(builder.getContext(), tapeEntry.data);
    auto dataGlobal = new llvm::GlobalVariable(
        *mod, dataConstant->getType(), true,
        llvm::GlobalValue::LinkageTypes::PrivateLinkage, dataConstant);

    // Copy the data into the buffer, up to the length
    // of the utsname struct (390, for amd64)
    // one can check with sizeof(struct utsname)
    // Note that you should be checking the struct definitions found in the
    // kernel headers, as glibc has changed which syscall the wrapper function
    // is over the years.
    builder.CreateMemCpy(utsnamePtr, {}, dataGlobal, {}, 390);

    // Return the original return code
    return builder.getInt64(tapeEntry.return_code);
  }
}
