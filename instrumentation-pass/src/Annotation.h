// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__ANNOTATION_H
#define TABACCO__ANNOTATION_H

#include <llvm/IR/BasicBlock.h>
#include <optional>

namespace lmcas_instrumentation_pass {

/**
 * Returns the LmcasBasicBlockID annotation on this basic block, if one
 * existed.
 */
std::optional<uint64_t> getBasicBlockID(llvm::BasicBlock const &basicBlock);

} // namespace lmcas_instrumentation_pass

#endif
