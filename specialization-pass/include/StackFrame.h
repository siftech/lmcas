// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__STACK_FRAME_H
#define TABACCO__STACK_FRAME_H

#include "FmtLlvm.h"

#include <absl/container/flat_hash_map.h>
#include <llvm/IR/Function.h>
#include <spdlog/spdlog.h>

/**
 * The data associated with a single function call.
 */
struct StackFrame {
  /**
   * The function being run in this stack frame.
   */
  llvm::Function &function;

  /**
   * The new function created from the function being run in this stack frame.
   */
  llvm::Function &newFunction;

  /**
   * The next instruction (from the original program) to be processed.
   */
  llvm::Instruction *insnPtr;

  /**
   * A map mapping values in the original program to the instructions that they
   * correspond to in the final program.
   */
  absl::flat_hash_map<llvm::Value *, llvm::Value *> locals;

  /*
   * A map mapping basic blocks in the original program to the basic blocks that
   * they correspond to in the final program
   * */
  absl::flat_hash_map<llvm::BasicBlock *, llvm::BasicBlock *> BBlocals;

  /**
   * The insertion point to continue at when the function returns.
   */
  std::optional<std::tuple<llvm::BasicBlock *, llvm::BasicBlock::iterator,
                           llvm::BasicBlock *, llvm::BasicBlock::iterator>>
      insertionPointsOnReturn;

  /**
   * Defines a new local, possibly replacing a previously-defined one.
   */
  void defineLocal(llvm::Value *originalValue, llvm::Value *newValue) {
    locals.insert_or_assign(originalValue, newValue);
  }

  /*
   * Defines a new local basic block
   */
  void defineBBLocal(llvm::BasicBlock *originalBB, llvm::BasicBlock *newBB) {
    auto findResult = this->BBlocals.find(originalBB);
    if (findResult != this->BBlocals.end()) {
      spdlog::error("A copy of the Basic Block {} has already been created in "
                    "the continuation!",
                    *originalBB);
      throw std::logic_error{"BUG"};
    } else {
      BBlocals.emplace(originalBB, newBB);
    }
  }

  /**
   * Translates a basic block from the original stack frame to this stack frame.
   */
  llvm::BasicBlock *translateBBValue(llvm::BasicBlock *originalValue) const {
    auto findResult = this->BBlocals.find(originalValue);
    if (findResult != this->BBlocals.end()) {
      return std::get<1>(*findResult);
    } else {
      spdlog::error("No translated Basic Block for {} was found!",
                    *originalValue);
      throw std::logic_error{"BUG"};
    }
  }

  /**
   * Translates a value from the original stack frame to this stack frame.
   */
  llvm::Value *translateValue(llvm::Instruction &instruction,
                              llvm::Value *originalValue) const {
    // The simple case is when the variable is in the locals; we can just look
    // it up in the map. This should always be the case for instructions and
    // arguments, since we can't refer to the ones from the original function!
    auto findResult = this->locals.find(originalValue);
    if (findResult != this->locals.end()) {
      return std::get<1>(*findResult);
    } else if (auto originalValueAsArgument =
                   llvm::dyn_cast<llvm::Argument>(originalValue)) {
      spdlog::error("Argument operand of instruction ({}) should have been "
                    "replaced: {}",
                    instruction, *originalValueAsArgument);
      throw std::logic_error{"BUG"};
    } else if (auto originalValueAsInstruction =
                   llvm::dyn_cast<llvm::Instruction>(originalValue)) {
      spdlog::error("Instruction operand of instruction ({}) should have been "
                    "replaced: {}",
                    instruction, *originalValueAsInstruction);
      throw std::logic_error{"BUG"};
    } else if (auto metadataAsValue =
                   llvm::dyn_cast<llvm::MetadataAsValue>(originalValue)) {
      // A... more annoying case is the fact that values can be wrapped up as
      // metadata. The @llvm.dbg.declare() function's first argument is one of
      // these!
      //
      // So basically how that works is, the arguments to the function are all
      // Values, but the last two arguments are actually metadata. Rather than
      // making instructions with metadata operands a thing, the LLVM authors
      // added llvm::MetadataAsValue, which wraps up a Metadata as a Value.
      //
      // That makes sense, but what about the first argument? It's a Value, but
      // for reasons that are unknown to me, rather than just using the value
      // itself as the argument, it's instead wrapped in llvm::ValueAsMetadata,
      // so that it can be wrapped in llvm::MetadataAsValue...
      //
      // In any case, we want to translate the value wrapped up in the metadata,
      // since we shouldn't be referencing the old function. We check if the
      // value has this structure (ValueAsMetadata inside MetadataAsValue), and
      // try to transform it if so.
      if (auto valueAsMetadataAsValue = llvm::dyn_cast<llvm::ValueAsMetadata>(
              metadataAsValue->getMetadata())) {
        auto wrappedValue = valueAsMetadataAsValue->getValue();
        auto transformedWrappedValue =
            this->translateValue(instruction, wrappedValue);
        // If the value was the same after being transformed (i.e. it was not
        // an instruction or argument; we haven't run across this yet at time
        // of writing, but it is representable), we just return the entire
        // original value, since re-wrapping it would just be a waste of time.
        if (wrappedValue == transformedWrappedValue) {
          return originalValue;
        } else {
          // If we did actually transform it, we then re-wrap it up, and return
          // the wrapped version.
          auto &mod = originalValue->getContext();
          return llvm::MetadataAsValue::get(
              mod, llvm::ValueAsMetadata::get(transformedWrappedValue));
        }
      } else {
        // (If it didn't have the structure we're looking for, we just return it
        // as-is; for now, we don't transform other metadata in any way.)
        return originalValue;
      }
    } else {
      return originalValue;
    }
  }
};

#endif
