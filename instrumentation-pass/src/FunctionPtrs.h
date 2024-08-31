// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__FUNCTION_PTRS_H
#define TABACCO__FUNCTION_PTRS_H

#include <llvm/IR/Module.h>

namespace lmcas_instrumentation_pass {

/**
 * Creates a table of function pointers in the module under the name
 * lmcas_function_pointer_table.
 */
void makeFunctionPointersTable(llvm::Module &mod);

} // namespace lmcas_instrumentation_pass

#endif
