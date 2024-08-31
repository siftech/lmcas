// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallReadv &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing readv: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else {
    // Store the data that was read into global variables.
    std::vector<llvm::GlobalVariable *> dataGlobals;
    for (const auto &iov : tapeEntry.iovs) {
      auto dataConstant =
          llvm::ConstantDataArray::get(builder.getContext(), iov.data);
      auto dataGlobal = new llvm::GlobalVariable(
          *mod, dataConstant->getType(), true,
          llvm::GlobalValue::LinkageTypes::PrivateLinkage, dataConstant);
      dataGlobals.push_back(dataGlobal);
    }

    // Check that the arguments match what was in the tape.
    auto fd = checkFdArg("readv", 0, tapeEntry);
    auto iovs =
        checkNonnullPtrArg("readv", 1, "iov", nullptr, builder.getInt64Ty());
    auto iovcnt =
        checkArg("readv", 2, tapeEntry, "iovcnt", &tape::SyscallReadv::iovcnt);

    // Copy the data into the buffers.
    for (size_t i = 0; i < tapeEntry.iovs.size(); i++) {
      auto ptr = builder.CreateGEP(iovs, builder.getInt32(2 * i));

      // TODO: Insert assertions about the length.

      builder.CreateMemCpy(ptr, {}, dataGlobals[i], {}, tapeEntry.iovs[i].len);
    }

    // Return the same return code.
    return builder.getInt64(tapeEntry.return_code);
  }
}
