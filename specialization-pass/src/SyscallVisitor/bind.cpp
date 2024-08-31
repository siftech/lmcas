// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *SyscallVisitor::operator()(const tape::SyscallBind &tapeEntry) {
  uint64_t einval_out = -static_cast<int64_t>(EINVAL);
  if (tapeEntry.return_code == einval_out) {
    // bind was called, and failed due to being in an unsupported addr family
    return builder.getInt64(einval_out);
  } else if (didSyscallFail(tapeEntry)) {
    // bind was called, and failed for another reason
    spdlog::error("TODO: Handle failing bind with unhandled error: {}",
                  tapeEntry);
    throw std::runtime_error{"TODO"};
  } else {
    // bind was called and succeeded.
    // Pass the syscall through, checking its arguments.
    auto sockaddrType = llvm::StructType::getTypeByName(builder.getContext(),
                                                        "struct.sockaddr");
    auto fd = checkFdArg("bind", 0, tapeEntry);
    auto sockaddr =
        checkNonnullPtrArg("bind", 1, "sockaddr", nullptr, sockaddrType);
    auto addrlen =
        checkArg("bind", 2, tapeEntry, "addrlen", &tape::SyscallBind::addrlen);
    return makeSyscallChecked("bind", SYS_bind, tapeEntry, fd, sockaddr,
                              addrlen);
  }
}
