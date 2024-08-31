// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__MAKE_DISPATCH_MAIN_H
#define TABACCO__MAKE_DISPATCH_MAIN_H

#include "Spec.h"
#include <llvm/IR/Function.h>

/**
 * Makes a new main function that dispatches to one of the given unrolled tape
 * main functions according to the arguments.
 */
llvm::Function *
makeDispatchMain(llvm::Module &mod, llvm::Twine const &name,
                 std::vector<spec::OptionConfig> const &optionConfigs,
                 std::vector<llvm::Function *> const &tapeUnrollings);

#endif
