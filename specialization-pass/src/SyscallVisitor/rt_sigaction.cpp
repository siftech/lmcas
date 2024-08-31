// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include "Tape.h"
#include <signal.h>
#include <sys/syscall.h>

llvm::Value *
SyscallVisitor::operator()(const tape::SyscallRtSigaction &tapeEntry) {
  auto sigactionType =
      llvm::StructType::getTypeByName(builder.getContext(), "struct.sigaction");
  auto sig = checkArg("rt_sigaction", 0, tapeEntry, "sig",
                      &tape::SyscallRtSigaction::sig);
  auto checkAct = [tapeEntry, this](tape::Sigaction tapeVal,
                                    llvm::Value *llvmVal,
                                    const std::string &argName) {
    checkSigactionStructPtr(tapeVal, llvmVal, tapeEntry.sigsetsize, argName);
  };
  // oact is only recorded after the syscall, and it can be written to by the
  // syscall.
  auto oact = getPtrArgUnchecked(2, sigactionType);
  // act, on the other hand, does not change between before and after the
  // syscall.
  auto act = checkNullablePtrArg("rt_sigaction", 1, "act", tapeEntry.act,
                                 checkAct, sigactionType);
  auto sigsetsize = checkArg("rt_sigaction", 3, tapeEntry, "sigsetsize",
                             &tape::SyscallRtSigaction::sigsetsize);

  auto int32Ty = builder.getInt32Ty();
  auto int64Ty = builder.getInt64Ty();
  auto sigHandlerTapeVal = tapeEntry.sighandler.value;

  if (sigHandlerTapeVal != 0 &&
      sigHandlerTapeVal != reinterpret_cast<uint64_t>(SIG_DFL) &&
      sigHandlerTapeVal != reinterpret_cast<uint64_t>(SIG_IGN)) {
    // Lookup signal handler
    auto sighandlerFunctionPtr =
        annotatedBasicBlocks.at(tapeEntry.sighandler)->getParent();

    // write value to sa_handler field
    auto ptr = builder.CreatePointerCast(
        act, llvm::PointerType::get(sigactionType, 0));
    auto fieldPtr = builder.CreateStructGEP(sigactionType, ptr, 0);

    // Create the expected function type.
    // void (i32, %struct.siginfo_t*, i8*)
    llvm::Type *args[] = {builder.getInt32Ty()};
    auto functionType =
        llvm::FunctionType::get(builder.getVoidTy(), args, false);
    // Cast both the function value and the value we're writing to it
    auto castedFieldPtr = builder.CreateBitOrPointerCast(
        fieldPtr,
        llvm::PointerType::get(llvm::PointerType::get(functionType, 0), 0));
    auto castedSighandlerFunctionPtr = builder.CreateBitOrPointerCast(
        sighandlerFunctionPtr, llvm::PointerType::get(functionType, 0));
    // Store function pointer we've made to the sa_handler position.
    auto store =
        builder.CreateStore(castedSighandlerFunctionPtr, castedFieldPtr);

    // call the syscall to set that function
    auto retval =
        makeSyscallUnchecked(SYS_rt_sigaction, sig, act, oact, sigsetsize);

    // oact can be written to after the syscall, and we also record it after the
    // syscall.
    checkNullablePtrArg("rt_sigaction", 2, "oact", tapeEntry.oact, checkAct,
                        sigactionType);
    return retval;
  } else {
    auto retval =
        makeSyscallUnchecked(SYS_rt_sigaction, sig, act, oact, sigsetsize);
    // oact can be written to after the syscall, and we also record it after the
    // syscall.
    checkNullablePtrArg("rt_sigaction", 2, "oact", tapeEntry.oact, checkAct,
                        sigactionType);
    return retval;
  }
}
