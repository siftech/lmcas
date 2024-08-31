// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__SYSCALL_VISITOR_H
#define TABACCO__SYSCALL_VISITOR_H

#include "Tape.h"
#include <absl/container/flat_hash_map.h>
#include <llvm/IR/IRBuilder.h>
#include <spdlog/spdlog.h>

class SyscallVisitor {
private:
  /**
   * Parameters.
   */

  llvm::IRBuilder<> &builder;
  llvm::CallInst &instruction;
  std::function<llvm::Value *(llvm::Value *)> translateValue;
  const absl::flat_hash_map<uint64_t, llvm::BasicBlock *> &annotatedBasicBlocks;

  /**
   * Derived fields.
   */

  /**
   * A flag for whether we're in release mode. In release mode, we don't put any
   * warnings into the generated binary that we otherwise might (e.g. that the
   * UID differs), to avoid making a "chatty" final binary.
   */
  bool isRelease = getenv("LMCAS_RELEASE") != nullptr;
  llvm::Module *mod = builder.GetInsertBlock()->getModule();

public:
  SyscallVisitor(llvm::IRBuilder<> &builder, llvm::CallInst &instruction,
                 std::function<llvm::Value *(llvm::Value *)> translateValue,
                 const absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
                     &annotatedBasicBlocks)
      : builder(builder), instruction(instruction),
        translateValue(translateValue),
        annotatedBasicBlocks(annotatedBasicBlocks) {}

private:
  /**
   * Basic debugging-related IR-building helpers.
   */

  /**
   * Appends an fprintf(stderr, ...) call to the program, prefixed with
   * "TaBaCCo: " for better identifiability in the output and followed by a
   * newline.
   */
  template <typename... PrintfArgs>
  void appendPrintf(const std::string &formatStr, PrintfArgs... args) {
    auto stderrGlobal = mod->getGlobalVariable("stderr");
    auto stderrValue =
        builder.CreateLoad(stderrGlobal->getValueType(), stderrGlobal);

    builder.CreateCall(
        mod->getFunction("fprintf"),
        {stderrValue,
         builder.CreateGlobalStringPtr(fmt::format("TaBaCCo: {}\n", formatStr)),
         args...});
  }

  /**
   * Appends an fprintf(stderr, ...) call to the program unless the
   * LMCAS_RELEASE environment variable exists.
   */
  template <typename... PrintfArgs>
  void appendPrintfUnlessRelease(const std::string &formatStr,
                                 PrintfArgs... args) {
    if (!this->isRelease)
      appendPrintf(formatStr, args...);
  }

  /**
   * Prints an error the the given syscall is unhandled, and appends code to
   * the program to do the same.
   */
  template <typename Entry> llvm::Value *todo(const Entry &tapeEntry) {
    if (!this->isRelease)
      spdlog::error("TODO: Handle syscall {}", tapeEntry);
    appendPrintfUnlessRelease(
        "%s\n", builder.CreateGlobalStringPtr(
                    fmt::format("TODO: Handle syscall {}", tapeEntry)));
    return builder.getInt64(-ENOSYS);
  }

  /**
   * If the condition is true at runtime, prints a formatted warning (as with
   * appendPrintfUnlessRelease).
   */
  template <typename... PrintfArgs>
  void warnIf(llvm::Value *cond, const std::string &formatStr,
              PrintfArgs &&...formatArgs) {
    auto func = builder.GetInsertBlock()->getParent();

    auto warnBB = llvm::BasicBlock::Create(builder.getContext(), "", func);
    auto afterBB = llvm::BasicBlock::Create(builder.getContext(), "", func);

    builder.CreateCondBr(cond, warnBB, afterBB);

    builder.SetInsertPoint(warnBB);
    appendPrintfUnlessRelease(formatStr, formatArgs...);
    builder.CreateBr(afterBB);

    builder.SetInsertPoint(afterBB);
  }

  /**
   * Functions to check for violations of our expectations.
   */

  /**
   * If the condition is true at runtime, prints a formatted warning (as with
   * appendPrintfUnlessRelease) and crashes.
   */
  template <typename... PrintfArgs>
  void trapIf(llvm::Value *cond, const std::string &formatStr,
              PrintfArgs &&...formatArgs) {
    auto func = builder.GetInsertBlock()->getParent();

    auto failBB = llvm::BasicBlock::Create(builder.getContext(), "", func);
    auto afterBB = llvm::BasicBlock::Create(builder.getContext(), "", func);

    builder.CreateCondBr(cond, failBB, afterBB);

    builder.SetInsertPoint(failBB);
    appendPrintfUnlessRelease(formatStr, formatArgs...);
    builder.CreateCall(
        llvm::Intrinsic::getDeclaration(mod, llvm::Intrinsic::trap));
    builder.CreateUnreachable();

    builder.SetInsertPoint(afterBB);
  }

  /**
   * Returns the value of the given argument _to the syscall_, without
   * validating it in any way. Always returns an i64.
   */
  llvm::Value *getArgUnchecked(unsigned int argIndex) {
    if (argIndex >= 6) {
      spdlog::error("BUG: getArgUnchecked({}) is out of bounds", argIndex);
      throw std::logic_error{"BUG"};
    }
    return translateValue(instruction.getArgOperand(1 + argIndex));
  }

  /**
   * Returns the value of the given argument to the syscall, without validating
   * it in any way. Returns a pointer to pointeeType, which defaults to i8 if
   * not supplied.
   */
  llvm::Value *getPtrArgUnchecked(unsigned int argIndex,
                                  llvm::Type *pointeeType = nullptr) {
    if (!pointeeType)
      pointeeType = builder.getInt8Ty();
    return builder.CreateIntToPtr(getArgUnchecked(argIndex),
                                  llvm::PointerType::get(pointeeType, 0));
  }

  /**
   * Checks that the value of an argument to the syscall matches the given
   * value, trapping if not. Returns the given value as a constant.
   */
  template <typename Entry>
  llvm::ConstantInt *checkArg(const std::string &syscallName,
                              unsigned int argIndex, const Entry &tapeEntry,
                              const std::string &argName, uint64_t value) {
    auto argValue = getArgUnchecked(argIndex);
    auto expectedValue = builder.getInt64(value);
    trapIf(builder.CreateICmpNE(argValue, expectedValue),
           fmt::format("{}: {} (%#lx) did not match tape-recorded value ({})",
                       syscallName, argName, value),
           argValue);
    return expectedValue;
  }

  /**
   * Checks that the value of an argument to the syscall matches the value
   * present in the tape with the given field, trapping if not. Returns the
   * value in the tape as a constant.
   */
  template <typename Entry, typename FieldType>
  llvm::ConstantInt *checkArg(const std::string &syscallName,
                              unsigned int argIndex, const Entry &tapeEntry,
                              const std::string &fieldName,
                              FieldType Entry::*field) {
    return checkArg(syscallName, argIndex, tapeEntry, fieldName,
                    static_cast<uint64_t>(tapeEntry.*field));
  }

  /**
   * Checks that the value of an argument to the syscall matches the value
   * present in the tape with the given field (which defaults to fd), trapping
   * if not. Returns the value in the tape as a constant as an i64.
   *
   * This is necessary to ensure that sign-extension doesn't do something
   * broken during comparison; it might make sense to generalize this to make
   * checkArg just take signedness into account in the future (though at the
   * moment this causes problems with passing literals).
   */
  template <typename Entry>
  llvm::Value *checkFdArg(const std::string &syscallName, unsigned int argIndex,
                          const Entry &tapeEntry,
                          const std::string &fieldName = "fd",
                          int32_t Entry::*field = &Entry::fd) {
    auto fdAsI64 = getArgUnchecked(argIndex);
    auto fdAsI32 = builder.CreateTrunc(fdAsI64, builder.getInt32Ty());

    auto expectedFd = builder.getInt32(tapeEntry.*field);
    trapIf(builder.CreateICmpNE(fdAsI32, expectedFd),
           fmt::format("{}: {} (%d) did not match tape-recorded value ({})",
                       syscallName, fieldName, tapeEntry.*field),
           fdAsI32);
    return builder.getInt64(tapeEntry.*field);
  }

  /**
   * Checks that the value of an argument to the syscall is non-null and
   * matches the given value by strcmp, trapping if not. Returns the given
   * value as a constant.
   */
  template <typename Entry>
  llvm::Constant *checkStrArg(const std::string &syscallName,
                              unsigned int argIndex, const Entry &tapeEntry,
                              const std::string &argName,
                              const std::string &value) {
    auto argValue = getPtrArgUnchecked(argIndex);
    auto expectedValue = builder.CreateGlobalStringPtr(value);

    // Check that the argument was non-null.
    trapIf(builder.CreateIsNull(argValue),
           fmt::format("{}: {} was unexpectedly null", syscallName, argName));

    // Check that the value matched.
    auto strcmpResult = builder.CreateCall(mod->getFunction("strcmp"),
                                           {argValue, expectedValue});
    trapIf(builder.CreateICmpNE(strcmpResult, builder.getInt32(0)),
           fmt::format("{}: {} (%s) did not match tape-recorded value ({})",
                       syscallName, argName, value),
           argValue);

    // Return the expected string constant.
    return expectedValue;
  }

  /**
   * Checks that the value of an argument to the syscall is non-null and
   * matches the value present in the tape with the given field by strcmp,
   * trapping if not. Returns the value in the tape as a constant.
   */
  template <typename Entry, typename FieldType>
  llvm::Constant *checkStrArg(const std::string &syscallName,
                              unsigned int argIndex, const Entry &tapeEntry,
                              const std::string &fieldName,
                              FieldType Entry::*field) {
    return checkStrArg(syscallName, argIndex, tapeEntry, fieldName,
                       tapeEntry.*field);
  }

  /**
   * Checks that the value of the fields in the sigaction struct
   * matches the values present in the tape recording, trapping if
   * not.
   */
  void checkSigactionStructPtr(struct tape::Sigaction sigactionValFromTape,
                               llvm::Value *ptr, tape::u64_as_string sigsetsize,
                               const std::string &argName) {
    auto sigactionType = llvm::StructType::getTypeByName(builder.getContext(),
                                                         "struct.k_sigaction");
    if (sigactionType == nullptr) {
      throw std::logic_error(
          "Could not find struct.k_sigaction. TODO: Use struct literal types.");
    } else {
      ptr = builder.CreatePointerCast(ptr,
                                      llvm::PointerType::get(sigactionType, 0));
      // Check fields of sigaction:
      // sa_handler could be a macro value (checkable), or a function pointer
      // (cant check that rn)
      auto saHandlerCur = builder.CreateStructGEP(sigactionType, ptr, 0);
      auto castedSaHandlerCur = builder.CreateBitOrPointerCast(
          saHandlerCur, llvm::PointerType::get(builder.getInt64Ty(), 0));
      auto saHandlerCurValue =
          builder.CreateLoad(builder.getInt64Ty(), castedSaHandlerCur);
      uint64_t saHandlerFromTape = sigactionValFromTape.sa_handler_.value;
      if ((saHandlerFromTape == reinterpret_cast<uint64_t>(SIG_IGN)) ||
          (saHandlerFromTape == reinterpret_cast<uint64_t>(SIG_DFL))) {
        auto tapeVal = builder.getInt64(saHandlerFromTape);
        auto cmp = builder.CreateICmpNE(tapeVal, saHandlerCurValue);

        // Trap if value is not equal to tape value.
        trapIf(cmp,
               fmt::format("rt_sigaction: {} entry sa_handler (%#lx) did "
                           "not match tape-recorded value ({}) ",
                           argName, saHandlerFromTape),
               saHandlerCur);
      } else {
        // Throw warning to mention that we need to implement a function
        // pointer check for sa_restorer.
        spdlog::warn("TODO: handle checking of function pointers in sigaction "
                     "for field sa_handler {}.",
                     sigactionValFromTape);
      }

      // Throw warning to mention that we need to implement a function
      // pointer check for sa_restorer.
      spdlog::warn("TODO: handle checking of function pointers in sigaction "
                   "for field sa_restorer {}.",
                   sigactionValFromTape);

      // check sa_flags
      auto saFlagsCur = builder.CreateStructGEP(sigactionType, ptr, 1);
      auto saFlagsCurValue =
          builder.CreateLoad(builder.getInt64Ty(), saFlagsCur);
      auto saFlagsFromTape = sigactionValFromTape.sa_flags;
      trapIf(builder.CreateICmpNE(saFlagsCurValue,
                                  builder.getInt64(saFlagsFromTape)),
             fmt::format("sigaction: {} entry sa_flags (%#lx) did not "
                         "match tape-recorded value ({})",
                         argName, saFlagsFromTape),
             saFlagsCurValue);

      // check sa_mask
      auto saMaskCur = builder.CreateStructGEP(sigactionType, ptr, 3);
      auto castedSaMaskPtr = builder.CreatePointerCast(
          saMaskCur, llvm::PointerType::get(builder.getInt64Ty(), 0));
      auto saMaskCurValue =
          builder.CreateAlignedLoad(builder.getInt64Ty(), castedSaMaskPtr,
                                    llvm::MaybeAlign(4), "saMaskCurValue");
      // At the present moment saMask is just a single u64
      // If we try a different architecture than x86_64, this will change.
      auto saMaskFromTape = sigactionValFromTape.sa_mask.val[0];
      auto tapeVal = builder.getInt64(saMaskFromTape);
      trapIf(builder.CreateICmpNE(saMaskCurValue, tapeVal),
             fmt::format("rt_sigaction: {} entry sa_mask (%#lx) did not "
                         "match tape-recorded value ({})",
                         argName, saMaskFromTape),
             saMaskCurValue);
    }
  }

  /**
   * Checks that the value of the fields in the rlim struct
   * matches the values present in the tape recording, trapping if
   * not.
   */
  void checkRlimStructPtr(struct tape::Rlimit rlimValFromTape,
                          llvm::Value *ptr) {
    auto rlimitType = llvm::StructType::get(
        builder.getContext(), {builder.getInt64Ty(), builder.getInt64Ty()});
    ptr = builder.CreatePointerCast(ptr, llvm::PointerType::get(rlimitType, 0));
    // Check fields of rlim:
    // rlim_cur
    auto expectedRlimCur = builder.getInt64(rlimValFromTape.rlim_cur);
    auto rlimCur = builder.CreateStructGEP(rlimitType, ptr, 0);
    auto rlimCurValue = builder.CreateLoad(builder.getInt64Ty(), rlimCur);
    trapIf(builder.CreateICmpNE(rlimCurValue, expectedRlimCur),
           fmt::format("prlimit: rlim entry rlim_cur (%#lx) did not match"
                       "tape-recorded value ({})",
                       static_cast<uint64_t>(rlimValFromTape.rlim_cur)),
           rlimCurValue);
    // rlim_max
    auto expectedRlimMax = builder.getInt64(rlimValFromTape.rlim_max);
    auto rlimMax = builder.CreateStructGEP(rlimitType, ptr, 1);
    auto rlimMaxValue = builder.CreateLoad(builder.getInt64Ty(), rlimMax);
    trapIf(builder.CreateICmpNE(rlimMaxValue, expectedRlimMax),
           fmt::format("prlimit: rlim entry rlim_max (%#lx) did not match "
                       "tape-recorded value ({})",
                       static_cast<uint64_t>(rlimValFromTape.rlim_max)),
           rlimMaxValue);
  }

  /**
   * Checks that an argument, which could be nullable, is null when the tape
   * entry element is null (trapping otherwise), or if it isn't null, emits IR
   * that compares the tapeElem and llvm value, trapping if they're not equal.
   */
  template <typename tapeEntryElemType, typename T>
  llvm::Value *
  checkNullablePtrArg(const std::string &syscallName, unsigned int argIndex,
                      const std::string &argName,
                      std::optional<tapeEntryElemType> tapeElemOpt,
                      T checkPointerContents, llvm::Type *pointeeType) {

    if (tapeElemOpt) {
      auto value = getPtrArgUnchecked(argIndex, pointeeType);
      checkPointerContents(tapeElemOpt.value(), value, argName);
      return value;
    } else {
      auto argValue = getArgUnchecked(argIndex);
      trapIf(builder.CreateICmpNE(argValue, builder.getInt64(0)),
             fmt::format("{}: entry {} (%#lx) did not match "
                         "tape-recorded value (0)",
                         syscallName, argName),
             argValue);
      return argValue;
    }
  }

  /**
   * Checks that an argument to the syscall is not null, trapping if it is
   * (unless nullAllowedIf specifies otherwise). Returns a pointer to
   * pointeeType, which defaults to i8 if not supplied.
   */
  llvm::Value *checkNonnullPtrArg(const std::string &syscallName,
                                  unsigned int argIndex,
                                  const std::string &argName,
                                  llvm::Value *nullAllowedIf = nullptr,
                                  llvm::Type *pointeeType = nullptr) {
    if (!nullAllowedIf)
      nullAllowedIf = builder.getFalse();

    auto value = getPtrArgUnchecked(argIndex, pointeeType);

    auto valueIsBad = builder.CreateAnd(builder.CreateIsNull(value),
                                        builder.CreateNot(nullAllowedIf));
    trapIf(valueIsBad,
           fmt::format("{}: {} was unexpectedly null", syscallName, argName));
    return value;
  }

  /**
   * Checks that the given syscall return code matches the value recorded in
   * the tape, trapping if it is not.
   *
   * You may want makeSyscallChecked instead.
   */
  template <typename Entry>
  llvm::ConstantInt *checkSyscallReturnCode(
      const std::string &syscallName, llvm::Value *syscallReturn,
      const Entry &tapeEntry,
      tape::u64_as_string Entry::*returnCodeField = &Entry::return_code) {
    auto expectedReturn = builder.getInt64(tapeEntry.*returnCodeField);
    trapIf(builder.CreateICmpNE(syscallReturn, expectedReturn),
           fmt::format(
               "{}: return code (%#lx) did not match tape-recorded value ({})",
               syscallName, static_cast<uint64_t>(tapeEntry.*returnCodeField)),
           syscallReturn);
    return expectedReturn;
  }

  /**
   * Checks that the given syscall return code matches the value recorded in
   * the tape, printing a warning if it is not. Always returns the value in the
   * tape as a constant.
   *
   * You may want makeSyscallWithWarning instead.
   */
  template <typename Entry>
  llvm::ConstantInt *warnSyscallReturnCode(
      const std::string &syscallName, llvm::Value *syscallReturn,
      const Entry &tapeEntry,
      tape::u64_as_string Entry::*returnCodeField = &Entry::return_code) {
    auto expectedReturn = builder.getInt64(tapeEntry.*returnCodeField);
    warnIf(builder.CreateICmpNE(syscallReturn, expectedReturn),
           fmt::format(
               "{}: return code (%#lx) did not match tape-recorded value ({})",
               syscallName, tapeEntry.*returnCodeField),
           syscallReturn);
    return expectedReturn;
  }

  /**
   * Helpers for performing syscalls
   */

  /**
   * Inserts code to perform the syscall. If "passing a syscall through,"
   * consider using makeSyscallChecked or makeSyscallWithWarning instead.
   */
  template <typename... Args>
  llvm::Instruction *makeSyscallUnchecked(uint64_t syscallNum, Args... args) {
    static_assert(
        sizeof...(args) <= 6,
        "cannot call makeSyscallUnchecked with more than 6 arguments");

    // Get the appropriate syscall wrapper.
    auto functionName = fmt::format("__syscall{}", sizeof...(args));
    auto function = mod->getFunction(functionName);
    if (!function) {
      spdlog::error("BUG: could not find syscall wrapper {}", functionName);
      throw std::logic_error{"BUG"};
    }

    // Create the array of arguments to the syscall wrapper.
    std::array<llvm::Value *, 1 + sizeof...(args)> wrapperArgs{
        builder.getInt64(syscallNum),
        (llvm::isa<llvm::PointerType>(args->getType())
             ? builder.CreatePtrToInt(args, builder.getInt64Ty())
             : args)...,
    };

    // Create a call to the syscall wrapper.
    return builder.CreateCall(function, wrapperArgs);
  }

  /**
   * Inserts code to perform the syscall, trapping if its return value differed
   * from the one recorded in the tape.
   */
  template <typename Entry, typename... Args>
  llvm::Value *makeSyscallChecked(const std::string &syscallName,
                                  uint64_t syscallNum, const Entry &tapeEntry,
                                  Args... args) {
    static_assert(sizeof...(args) <= 6,
                  "cannot call makeSyscallChecked with more than 6 arguments");
    return checkSyscallReturnCode(
        syscallName, makeSyscallUnchecked(syscallNum, args...), tapeEntry);
  }

  /**
   * Inserts code to perform the syscall, warning if its return value differed
   * from the one recorded in the tape. Always returns the return value
   * recorded in the tape.
   */
  template <typename Entry, typename... Args>
  llvm::Value *makeSyscallWithWarning(const std::string &syscallName,
                                      uint64_t syscallNum,
                                      const Entry &tapeEntry, Args... args) {
    static_assert(
        sizeof...(args) < 6,
        "cannot call makeSyscallWithWarning with more than 6 arguments");
    return warnSyscallReturnCode(
        syscallName, makeSyscallUnchecked(syscallNum, args...), tapeEntry);
  }

  /**
   * Helpers for initializing structures.
   */

  void makeStore(const timespec &val, llvm::Value *ptr);
  void makeStore(const struct stat &val, llvm::Value *ptr);

  llvm::Value *emit_stat_store(unsigned int index, llvm::Value *address,
                               llvm::ConstantInt *entryVal,
                               llvm::StructType *statType,
                               const std::string &label) {
    auto ptrCast =
        builder.CreateIntToPtr(address, llvm::PointerType::get(statType, 0));
    auto loc = builder.CreateStructGEP(statType, ptrCast, index,
                                       llvm::StringRef(label + "loc"));
    auto store = builder.CreateStore(entryVal, loc);
    return store;
  }

  llvm::Value *emit_stat_timespec_store(unsigned int indexInStat,
                                        unsigned int indexInTimespec,
                                        llvm::Value *address,
                                        llvm::ConstantInt *entryVal,
                                        llvm::StructType *statType,
                                        llvm::StructType *timespecType,
                                        const std::string &label) {
    auto ptrCastStatAddr =
        builder.CreateIntToPtr(address, llvm::PointerType::get(statType, 0));
    auto locStat = builder.CreateStructGEP(
        statType, ptrCastStatAddr, indexInStat, llvm::StringRef(label + "loc"));
    auto locTimespec = builder.CreateStructGEP(
        timespecType, locStat, indexInTimespec, llvm::StringRef(label + "loc"));
    auto store = builder.CreateStore(entryVal, locTimespec);
    return store;
  }

  /**
   * Syscall handlers.
   */

public:
  llvm::Value *operator()(const tape::SyscallRead &tapeEntry);
  llvm::Value *operator()(const tape::SyscallWrite &tapeEntry);
  llvm::Value *operator()(const tape::SyscallOpen &tapeEntry);
  llvm::Value *operator()(const tape::SyscallStat &tapeEntry);
  llvm::Value *operator()(const tape::SyscallFstat &tapeEntry);
  llvm::Value *operator()(const tape::SyscallClose &tapeEntry);
  llvm::Value *operator()(const tape::SyscallLseek &tapeEntry);
  llvm::Value *operator()(const tape::SyscallRtSigaction &tapeEntry);
  llvm::Value *operator()(const tape::SyscallRtSigprocmask &tapeEntry);
  llvm::Value *operator()(const tape::SyscallMmap &tapeEntry);
  llvm::Value *operator()(const tape::SyscallMprotect &tapeEntry);
  llvm::Value *operator()(const tape::SyscallMunmap &tapeEntry);
  llvm::Value *operator()(const tape::SyscallBrk &tapeEntry);
  llvm::Value *operator()(const tape::SyscallIoctl &tapeEntry);
  llvm::Value *operator()(const tape::SyscallPread &tapeEntry);
  llvm::Value *operator()(const tape::SyscallReadv &tapeEntry);
  llvm::Value *operator()(const tape::SyscallWritev &tapeEntry);
  llvm::Value *operator()(const tape::SyscallPipe &tapeEntry);
  llvm::Value *operator()(const tape::SyscallGetpid &tapeEntry);
  llvm::Value *operator()(const tape::SyscallSocket &tapeEntry);
  llvm::Value *operator()(const tape::SyscallConnect &tapeEntry);
  llvm::Value *operator()(const tape::SyscallBind &tapeEntry);
  llvm::Value *operator()(const tape::SyscallListen &tapeEntry);
  llvm::Value *operator()(const tape::SyscallSetsockopt &tapeEntry);
  llvm::Value *operator()(const tape::SyscallUname &tapeEntry);
  llvm::Value *operator()(const tape::SyscallOpenat &tapeEntry);
  llvm::Value *operator()(const tape::SyscallFcntl &tapeEntry);
  llvm::Value *operator()(const tape::SyscallMkdir &tapeEntry);
  llvm::Value *operator()(const tape::SyscallGetuid &tapeEntry);
  llvm::Value *operator()(const tape::SyscallGeteuid &tapeEntry);
  llvm::Value *operator()(const tape::SyscallGetgid &tapeEntry);
  llvm::Value *operator()(const tape::SyscallGetppid &tapeEntry);
  llvm::Value *operator()(const tape::SyscallGetgroups &tapeEntry);
  llvm::Value *operator()(const tape::SyscallSchedGetaffinity &tapeEntry);
  llvm::Value *operator()(const tape::SyscallClockGettime &tapeEntry);
  llvm::Value *operator()(const tape::SyscallClockGetres &tapeEntry);
  llvm::Value *operator()(const tape::SyscallPrlimit &tapeEntry);
  llvm::Value *operator()(const tape::SyscallEpollCreate1 &tapeEntry);
  llvm::Value *operator()(const tape::SyscallUmask &tapeEntry);
};

/**
 * Other helpers.
 */

/**
 * Returns whether a tape entry's recorded syscall return code corresponds to
 * an error.
 */
template <typename Entry>
bool didSyscallFail(
    const Entry &entry,
    tape::u64_as_string Entry::*returnCodeField = &Entry::return_code) {
  return entry.*returnCodeField > 0xfffffffffffff000;
}

#endif
