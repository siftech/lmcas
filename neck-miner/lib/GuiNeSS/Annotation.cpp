// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "GuiNeSS/Annotation.h"

#include "FmtLlvm.h"
#include <llvm/IR/Module.h>
#include <spdlog/spdlog.h>
#include <string>

namespace guiness {

std::optional<uint64_t>
getBasicBlockAnnotation(const llvm::BasicBlock *basicBlock) {
  // Get the metadata node.
  if (auto terminator = basicBlock->getTerminator()) {
    if (auto metadataNode = terminator->getMetadata("LmcasBasicBlockID")) {
      // Check that it's of the form we want (one operand, which is a string).
      if (metadataNode->getNumOperands() == 1) {
        if (auto idAsString =
                llvm::dyn_cast<llvm::MDString>(metadataNode->getOperand(0))) {

          // Parse the string to a uint64_t.
          try {
            return std::stoull(idAsString->getString().str());
          } catch (std::invalid_argument exn) {
            // fallthrough
          }
        }
      }

      // If an error happened for any reason, just print the node. It's probably
      // not worth having more specific error-handling, since this isn't likely
      // to be a "user-servicable" error anyway.
      spdlog::warn("Invalid metadata node for LmcasBasicBlockID: {}",
                   *llvm::cast<llvm::Metadata>(metadataNode));
    }
  }
  return std::nullopt;
}

absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
findAnnotatedBasicBlocks(llvm::Module &mod) {
  // This just calls getBasicBlockAnnotation on every basic block in the program
  // and accumulates the results into a map.
  absl::flat_hash_map<uint64_t, llvm::BasicBlock *> out;
  for (auto &function : mod)
    for (auto &basicBlock : function)
      if (auto id = getBasicBlockAnnotation(&basicBlock))
        out.insert_or_assign(id.value(), &basicBlock);
  return out;
}

} // namespace guiness
