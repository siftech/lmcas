// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__FIND_NECK_H
#define TABACCO__FIND_NECK_H

#include <absl/container/flat_hash_set.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

namespace tabacco {

/**
 * Returns each of the instructions that are logically immediately after some
 * marked neck position.
 */
auto findNeckMarkers(llvm::Module &mod, llvm::StringRef neckMarkerName)
    -> absl::flat_hash_set<llvm::Instruction *>;

} // namespace tabacco

#endif
