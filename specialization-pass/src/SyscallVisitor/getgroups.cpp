// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallGetgroups &tapeEntry) {
  if (didSyscallFail(tapeEntry)) {
    spdlog::error("TODO: Handle failing getgroups: {}", tapeEntry);
    throw std::runtime_error{"TODO"};

  } else if (tapeEntry.data.size() == 0) {

    // If zero is passed as size, the effect is to get the number of
    // supplementary group IDs for the process.
    //
    // In this case, we check that the size parameter was the same, but ignore
    // the list parameter.
    checkArg("getgroups", 0, tapeEntry, "size", 0);

    return makeSyscallWithWarning("getgroups", SYS_getgroups, tapeEntry,
                                  builder.getInt64(0), builder.getInt64(0));

  } else {

    // Otherwise, we store the list of supplementary group IDs we recorded in
    // the tape.
    //
    // TODO: Warn if the list of groups differed from the one recorded in the
    // tape.
    //
    // We check that the size matches the one originally passed in, and get the
    // pointer (as an i32 pointer).
    checkArg("getgroups", 0, tapeEntry, "size", tapeEntry.data.size());
    auto list = checkNonnullPtrArg("getgroups", 1, "list", nullptr,
                                   builder.getInt32Ty());

    // We then can emit stores for each group stored in the tape.
    for (size_t i = 0; i < tapeEntry.data.size(); i++) {
      auto ptr =
          builder.CreateGEP(builder.getInt32Ty(), list, builder.getInt32(i));
      builder.CreateStore(builder.getInt32(tapeEntry.data[i]), ptr);
    }

    // Finally, we can return the original return code.
    return builder.getInt64(tapeEntry.return_code);
  }
}
