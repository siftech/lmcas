// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__FIND_NECK_H
#define TABACCO__FIND_NECK_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

namespace lmcas_instrumentation_pass {

/**
 * Returns each of the instructions that are logically immediately after some
 * marked neck position.
 */
auto findNeckMarkers(llvm::Module &mod, llvm::StringRef neckMarkerName)
    -> std::vector<llvm::Instruction *>;

} // namespace lmcas_instrumentation_pass

#endif
