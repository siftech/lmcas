// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include <sys/mman.h>

llvm::Value *SyscallVisitor::operator()(const tape::SyscallOpen &tapeEntry) {
  if ((int64_t)tapeEntry.return_code.value == -(int64_t)ENOENT) {
    // If the open failed because the file didn't exist, we can just hard-code
    // that.
    return builder.getInt64(-ENOENT);

  } else if (didSyscallFail(tapeEntry)) {
    throw std::runtime_error{
        fmt::format("TODO: Handle failing open: {}", tapeEntry),
    };

  } else {
    // We internalize every open call; we return a fictitious file descriptor
    // that contains exactly contents we want. We use memfd for this, since
    // it's impossible for an attacker to tamper with it (without already
    // having permissions sufficient to completely compromise the binary).
    //
    // There are some flags we can just ignore, so we mask them out now.
    unsigned int tape_flags = tapeEntry.flags;
    tape_flags &= ~O_RDWR;
    tape_flags &= ~O_CREAT;
    tape_flags &= ~O_EXCL;
    tape_flags &= ~O_NOCTTY;
    tape_flags &= ~O_DSYNC;
    tape_flags &= ~O_SYNC;
    tape_flags &= ~O_NOFOLLOW;
    tape_flags &= ~0100000; // This is O_LARGEFILE, but glibc defines it to be
                            // zero because... shenanigans.
    tape_flags &= ~O_TMPFILE;

    // Some flags can only be set on the memfd by fcntl. Technically we're
    // breaking atomicity by splitting them up into two calls, but since we
    // disallow multithreading, we're "usually fine."
    unsigned int fcntl_flags =
        tape_flags & (O_APPEND | O_NONBLOCK | O_ASYNC | O_DIRECT | O_NOATIME);

    // Some flags have memfd equivalents.
    unsigned int memfd_flags = 0;
    unsigned int open_flags_supported_by_memfd = 0;
    if (tape_flags & O_CLOEXEC) {
      memfd_flags |= MFD_CLOEXEC;
      open_flags_supported_by_memfd |= O_CLOEXEC;
    }

    // Some flags are just not supported yet.
    if (tape_flags & O_DIRECTORY) {
      throw std::runtime_error{fmt::format(
          "Cannot handle {}: O_DIRECTORY is not supported", tapeEntry)};
    }
    if (tape_flags & O_PATH) {
      throw std::runtime_error{
          fmt::format("Cannot handle {}: O_PATH is not supported", tapeEntry)};
    }
    if (tape_flags & O_WRONLY) {
      throw std::runtime_error{fmt::format(
          "Cannot handle {}: O_WRONLY is not supported", tapeEntry)};
    }

    // Check that all flags fall into one of the above classifications.
    unsigned int unsupported_flags =
        tape_flags & ~(fcntl_flags | open_flags_supported_by_memfd);
    if (unsupported_flags) {
      throw std::runtime_error{
          fmt::format("Cannot handle {}: unrecognized flags: {}", tapeEntry,
                      unsupported_flags)};
    }

    // Check the arguments actually provided to the syscall.
    auto pathname = checkStrArg("open", 0, tapeEntry, "pathname",
                                &tape::SyscallOpen::filename);
    checkArg("open", 1, tapeEntry, "flags", &tape::SyscallOpen::flags);
    checkArg("open", 2, tapeEntry, "mode", &tape::SyscallOpen::mode);

    // Create the file descriptor used to emulate the file.
    auto result =
        makeSyscallChecked("open", SYS_memfd_create, tapeEntry, pathname,
                           builder.getInt64(memfd_flags | MFD_ALLOW_SEALING));

    // If we had any flags that need to be emulated by fcntl(), do so.
    if (fcntl_flags) {
      auto oldFlags = makeSyscallUnchecked(
          SYS_fcntl, builder.getInt64(tapeEntry.return_code),
          builder.getInt64(F_GETFL));
      trapIf(builder.CreateICmpSLT(oldFlags, builder.getInt64(0)),
             "open: fcntl GETFL call failed (%lld)", oldFlags);
      auto result = makeSyscallUnchecked(
          SYS_fcntl, builder.getInt64(tapeEntry.return_code),
          builder.getInt64(F_SETFL),
          builder.CreateOr(oldFlags, builder.getInt64(fcntl_flags)));
      trapIf(builder.CreateICmpSLT(result, builder.getInt64(0)),
             "open: fcntl SETFL call failed (%lld)", result);
    }

    // If the file was O_RDONLY, disable writes.
    if ((tapeEntry.flags & O_ACCMODE) == O_RDONLY) {
      auto result = makeSyscallUnchecked(
          SYS_fcntl, builder.getInt64(tapeEntry.return_code),
          builder.getInt64(F_ADD_SEALS),
          builder.getInt64(F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE));
      trapIf(builder.CreateICmpSLT(result, builder.getInt64(0)),
             "open: fcntl ADD_SEALS call failed (%lld)", result);
    }

    return result;
  }
}
