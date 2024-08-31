// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "Annotation.h"

#include "FmtLlvm.h"
#include <spdlog/spdlog.h>
#include <string>

namespace tabacco {

std::optional<uint64_t> getBasicBlockID(llvm::BasicBlock const &basicBlock) {
  // Get the metadata node.
  auto metadataNode =
      basicBlock.getTerminator()->getMetadata("LmcasBasicBlockID");
  if (!metadataNode)
    return std::nullopt;

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
  // not worth having more specific error-handling, since this isn't likely to
  // be a "user-servicable" error anyway.
  spdlog::warn("Invalid metadata node for LmcasBasicBlockID: {}",
               *llvm::cast<llvm::Metadata>(metadataNode));
  return std::nullopt;
}

} // namespace tabacco
