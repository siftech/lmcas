// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "GuiNeSS/SyscallWrappers.h"

#include <regex>

namespace guiness {

/**
 * If the given function is a syscall wrapper, returns its arity. Otherwise,
 * returns nullopt.
 */
std::optional<int> isSyscallWrapper(llvm::Function &function) {
  // If the function is __syscall_cp, it's a syscall wrapper.
  std::regex syscallCpFunctionNameRegex("__syscall_cp(.[0-9]+)?");
  const auto name = function.getName().str();
  if (std::regex_match(name, syscallCpFunctionNameRegex))
    return 6;

  // If the function's name doesn't match the regex, it's not a syscall
  // wrapper, so skip it.
  std::regex internalSyscallFunctionNameRegex("__syscall[0-6](.[0-9]+)?");
  if (!std::regex_match(name, internalSyscallFunctionNameRegex))
    return std::nullopt;

  // At this point, we know we've found a syscall wrapper function!

  // Compute the arity from the name.
  char arityChar = name[9];
  int arity = arityChar - '0';

  // Store both in the vector.
  return arity;
}

} // namespace guiness
