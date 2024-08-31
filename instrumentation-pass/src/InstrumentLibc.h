// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO___LIBC_SPECIFIC_H
#define TABACCO___LIBC_SPECIFIC_H

#include <llvm/IR/Module.h>

namespace lmcas_instrumentation_pass {

/** Removes all functions whose names match the regex __syscall[0-6](.[0-9]+)?
 *
 * Returns whether it succeeded.
 *
 * These functions are syscall wrappers that are internal to musl. Since
 * they're declared static, llvm-link won't easily let us override them, so we
 * need to replace them here instead with calls to a version marked as external.
 * This external reference then gets resolved to the syscall wrappers present
 * in the instrumentation runtime.
 */
bool libcReplaceDunderSyscall(llvm::Module &mod);

} // namespace lmcas_instrumentation_pass

#endif // TABACCO___LIBC_SPECIFIC 1
