// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <optional>
#include <regex>

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>

#include <spdlog/spdlog.h>

namespace lmcas_instrumentation_pass {

/**
 * Finds the syscall wrappers that were *not* the ones from the instrumentation
 * runtime, and returns them, along with the arity of syscalls they handle
 * (i.e., __syscall0 has arity 0, not 1).
 */
static std::vector<std::tuple<std::reference_wrapper<llvm::Function>, int>>
findInternalSyscallWrapper(llvm::Module &mod) {
  std::vector<std::tuple<std::reference_wrapper<llvm::Function>, int>>
      internalSyscallWrappers;

  std::regex internalSyscallFunctionNameRegex("__syscall([0-6]|_cp)(.[0-9]+)?");
  for (auto &func : mod) {
    const auto &name = func.getName().str();

    // If the function's name doesn't match the regex, it's not a syscall
    // wrapper, so skip it.
    if (!std::regex_match(name, internalSyscallFunctionNameRegex))
      continue;

    // If the function doesn't have internal linkage, set internal linkage
    if (!func.hasInternalLinkage()) {
      spdlog::warn("Function {} does not have internal linkage!", name);
      spdlog::warn("Setting internal linkage for syscall: {}", name);
      func.setLinkage(llvm::GlobalValue::LinkageTypes::InternalLinkage);
    }

    // At this point, we know we've found a syscall wrapper function!

    int arity;
    // Compute the arity from the name.
    if (name == "__syscall_cp") {
      arity = 7;
    } else {
      char arityChar = name[9];
      arity = arityChar - '0';
    }

    // Store both in the vector.
    internalSyscallWrappers.push_back({func, arity});
  }

  return internalSyscallWrappers;
}

/**
 * Creates external functions referencing the instrumentation runtime's syscall
 * wrappers, and returns them and their names. (The names are necessary because
 * these functions get renamed by LLVM to avoid name conflicts with the
 * libc-internal ones.)
 */
static std::array<std::tuple<llvm::Function *, std::string>, 8>
getRuntimeSyscallWrappers(llvm::Module &mod) {
  auto &context = mod.getContext();
  auto i64 = llvm::Type::getInt64Ty(context);

  // To get the right number of args, we construct an array of the maximum
  // number of arguments, then slice it down to get an ArrayRef with the right
  // number.
  std::array<llvm::Type *, 7> args = {i64, i64, i64, i64, i64, i64, i64};

  std::array<std::tuple<llvm::Function *, std::string>, 8>
      runtimeSyscallWrappers;

  for (size_t i = 0; i < 7; i++) {
    // Make the function's type. Note that we have one more argument than i;
    // the first argument is the syscall number.
    auto argsRef = llvm::ArrayRef(args).slice(6 - i);
    auto functionType = llvm::FunctionType::get(i64, argsRef, false);

    // Make the function's name.
    auto name = fmt::format("__syscall{}", i);

    // Make the function.
    auto func = llvm::Function::Create(
        functionType, llvm::GlobalValue::ExternalLinkage, name, mod);

    // Store the function and its original name.
    runtimeSyscallWrappers[i] = std::make_tuple(func, std::move(name));
  }

  // Create external function for syscall_cp
  auto argsRef = llvm::ArrayRef(args);
  auto functionType = llvm::FunctionType::get(i64, argsRef, false);
  auto func = llvm::Function::Create(
      functionType, llvm::GlobalValue::ExternalLinkage, "__syscall_cp", mod);
  runtimeSyscallWrappers[7] = std::make_tuple(func, "__syscall_cp");

  return runtimeSyscallWrappers;
}

bool libcReplaceDunderSyscall(llvm::Module &mod) {
  // Find the libc-internal syscall wrappers.
  auto internalSyscallWrappers = findInternalSyscallWrapper(mod);
  spdlog::info("Found {} internal syscall wrappers",
               internalSyscallWrappers.size());

  // Get our syscall wrappers.
  auto runtimeSyscallWrappers = getRuntimeSyscallWrappers(mod);

  spdlog::info("There are {} runtime syscall wrappers ",
               runtimeSyscallWrappers.size());

  // Replace calls to the libc-internal syscall wrappers with calls to the
  // instrumentation runtime's wrappers.
  for (auto &funcAndArity : internalSyscallWrappers) {
    auto [internalWrapper, arity] = funcAndArity;
    auto runtimeWrapper = std::get<0>(runtimeSyscallWrappers[arity]);
    spdlog::info("Replacing syscall {}", runtimeWrapper->getName().str());
    internalWrapper.get().replaceAllUsesWith(runtimeWrapper);
  }

  // Delete all the libc-internal syscall wrappers now that they're no longer
  // being used.
  for (auto &funcAndArity : std::move(internalSyscallWrappers))
    std::get<0>(funcAndArity).get().eraseFromParent();

  // Rename the runtime-provided syscall wrappers to their original names.
  for (auto &funcAndName : std::move(runtimeSyscallWrappers)) {
    auto [func, name] = funcAndName;
    func->setName(std::move(name));
  }

  spdlog::info("Finished replacing syscall wrappers");
  return true;
}

} // namespace lmcas_instrumentation_pass
