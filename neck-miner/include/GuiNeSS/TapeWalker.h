// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef GUINESS__TAPE_WALKER_H
#define GUINESS__TAPE_WALKER_H

#include "Config.h"
#include "Tape.h"
#include <absl/container/flat_hash_map.h>
#include <llvm/IR/BasicBlock.h>
#include <optional>
#include <spdlog/spdlog.h>

namespace guiness {

struct InstructionWithStack {
  // TODO: Should these be ref-counted linked lists, for memory use reasons?
  std::vector<llvm::CallInst *> stack;
  std::vector<tape::SyscallStart const *> syscallsSoFar;
  llvm::Instruction *inst;

  void debugLogStackTrace();
};

/**
 * A type for walking over the tape. This is also not a proper C++ iterator.
 */
class TapeIterator {
  tape::Tape::const_iterator here;
  tape::Tape::const_iterator const end;

public:
  TapeIterator(tape::Tape const &tape) : here(tape.begin()), end(tape.end()) {}

  /**
   * Returns whether the tape is at its end.
   */
  auto empty() const -> bool;

  /**
   * Returns the length remaining in the tape.
   */
  auto num_remaining() const -> size_t;

  /**
   * Reads a tape entry without advancing the iterator. This must not be at the
   * end.
   */
  auto peek(std::string_view entryToFind) const -> tape::TapeEntry const &;

  /**
   * Reads a tape entry, advancing the iterator. This must not be at the end.
   */
  auto next(std::string_view entryToFind) -> tape::TapeEntry const &;

  /**
   * Reads a tape entry without advancing the iterator. If it was of type T,
   * return it; else, throw an exception. This must not be at the end.
   */
  template <typename T>
  auto peek(std::string_view typeName) const -> T const & {
    const auto &tapeEntry = this->peek(typeName);
    if (!std::holds_alternative<T>(tapeEntry)) {
      throw std::runtime_error{fmt::format(
          "Tape mismatch: expected a {}, found {}", typeName, tapeEntry)};
    }
    return std::get<T>(tapeEntry);
  }

  /**
   * Reads a tape entry, advancing the iterator. If it was of type T, return it;
   * else, throw an exception. This must not be at the end.
   */
  template <typename T> auto next(std::string_view typeName) -> T const & {
    const auto &out = this->peek<T>(typeName);
    ++this->here;
    spdlog::trace("Advancing past {} ({} remaining)", out,
                  this->end - this->here);
    return out;
  }
};

/**
 * An iterator-like type for walking through every instruction encountered by
 * the tape.
 *
 * TODO: This basically follows the Rust iterator interface because the C++17
 * one is essentially ridiculous; consider whether it should be changed to use
 * the C++20 one.
 */
class TapeWalker {
  Config const &config;
  TapeIterator tape;

  std::vector<llvm::CallInst *> stack;
  std::vector<tape::SyscallStart const *> syscallsSoFar;
  llvm::Instruction *nextInst;
  absl::flat_hash_map<uint64_t, llvm::BasicBlock *> annotatedBasicBlocks;

public:
  TapeWalker(Config const &config, llvm::Module &mod, tape::Tape const &tape);
  auto next() -> std::optional<InstructionWithStack>;
  auto collect() -> std::vector<InstructionWithStack>;
};

} // namespace guiness

#endif
