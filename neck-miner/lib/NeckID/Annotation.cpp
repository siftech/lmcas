#include "NeckID/NeckID/Annotation.h"

namespace neckid {
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

  // llvm::outs() << "Invalid metadata node for LmcasBasicBlockID: "
  //             << *llvm::cast<llvm::Metadata>(metadataNode);

  return std::nullopt;
}
} // namespace neckid
