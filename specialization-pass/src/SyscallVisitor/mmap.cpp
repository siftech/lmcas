// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include <sys/mman.h>

llvm::Value *SyscallVisitor::operator()(const tape::SyscallMmap &tapeEntry) {
  // mmap has a bunch of different cases, depending on the "shape" of the mmap
  // call.
  if (didSyscallFail(tapeEntry)) {
    // If the mmap call failed, we can just bake that in.
    return builder.getInt64(tapeEntry.return_code);

  } else if (tapeEntry.flags == (MAP_PRIVATE | MAP_ANONYMOUS)) {
    // In the simplest case, we are just allocating memory at any address. To
    // avoid problems with differing locations (and in particular, their
    // alignments), we promote the call to a MAP_FIXED_NOREPLACE at the address
    // the kernel chose in the instrumented execution.
    auto addr = checkArg("mmap", 0, tapeEntry, "addr", tapeEntry.return_code);
    auto length =
        checkArg("mmap", 1, tapeEntry, "length", &tape::SyscallMmap::len);
    auto prot =
        checkArg("mmap", 2, tapeEntry, "prot", &tape::SyscallMmap::prot);
    auto flags =
        checkArg("mmap", 3, tapeEntry, "flags", &tape::SyscallMmap::flags);
    auto fd = checkFdArg("mmap", 4, tapeEntry);
    auto offset =
        checkArg("mmap", 5, tapeEntry, "off", &tape::SyscallMmap::off);

    return makeSyscallChecked("mmap", SYS_mmap, tapeEntry, addr, length, prot,
                              flags, fd, offset);

  } else {
    spdlog::error("Unsupported form of mmap: {}", tapeEntry);
    throw std::runtime_error{"TODO"};
  }
}
