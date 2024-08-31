// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef GUINESS__ANNOTATION_H
#define GUINESS__ANNOTATION_H

#include <absl/container/flat_hash_map.h>
#include <llvm/IR/BasicBlock.h>
#include <optional>

namespace guiness {

/**
 * Returns the ID inserted by the annotation pass, if one existed.
 */
std::optional<uint64_t>
getBasicBlockAnnotation(const llvm::BasicBlock *basicBlock);

/**
 * Searches through all the basic blocks in the module for those that were
 * traversed by the annotation pass and constructs a map to conveniently access
 * them by ID.
 */
absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
findAnnotatedBasicBlocks(llvm::Module &mod);

} // namespace guiness

#endif
