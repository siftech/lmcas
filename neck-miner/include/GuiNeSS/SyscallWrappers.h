// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef GUINESS__SYSCALL_WRAPPERS_H
#define GUINESS__SYSCALL_WRAPPERS_H

#include <llvm/IR/Function.h>
#include <optional>

namespace guiness {

std::optional<int> isSyscallWrapper(llvm::Function &function);

} // namespace guiness

#endif
