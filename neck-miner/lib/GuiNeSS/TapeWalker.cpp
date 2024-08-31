// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "GuiNeSS/TapeWalker.h"
#include "FmtLlvm.h"
#include "GuiNeSS/Annotation.h"
#include "GuiNeSS/SyscallWrappers.h"
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Module.h>
#include <spdlog/spdlog.h>

namespace guiness {

void InstructionWithStack::debugLogStackTrace() {
  for (auto const &callInst : this->stack) {
    spdlog::debug("  [{}, {}]{}", *callInst->getParent()->getParent(),
                  *callInst->getParent(),
                  *static_cast<llvm::Instruction *>(callInst));
  }
  spdlog::debug("  [{}, {}]{}", *this->inst->getParent()->getParent(),
                *this->inst->getParent(), *this->inst);
}

auto TapeIterator::empty() const -> bool { return this->here == this->end; }
auto TapeIterator::num_remaining() const -> size_t {
  return this->end - this->here;
}

auto TapeIterator::peek(std::string_view entryToFind) const
    -> tape::TapeEntry const & {
  spdlog::trace("Expecting a {}", entryToFind);
  if (this->empty()) {
    throw std::runtime_error{
        fmt::format("Tried to advance tape to find a {} while at end of tape",
                    entryToFind)};
  }
  return *this->here;
}

auto TapeIterator::next(std::string_view entryToFind)
    -> tape::TapeEntry const & {
  auto const &out = this->peek(entryToFind);
  ++this->here;
  return out;
}

// TODO: Lots of code copying from the visitor in the specialization pass.
struct TapeVisitor : public llvm::InstVisitor<TapeVisitor> {
  Config const &config;
  TapeIterator &tape;
  std::vector<llvm::CallInst *> &stack;
  std::vector<tape::SyscallStart const *> &syscallsSoFar;
  llvm::Instruction *&nextInst;
  absl::flat_hash_map<uint64_t, llvm::BasicBlock *> const &annotatedBasicBlocks;
  bool bailedOut;

  /**
   * Advances to the next instruction. This only works for non-terminator
   * instructions.
   */
  void visitInstruction(llvm::Instruction &inst) {
    if (auto next = inst.getNextNode()) {
      this->nextInst = next;
    } else {
      throw std::logic_error{
          fmt::format("Invalid IR: Found a basic block ({}) not terminated by "
                      "a terminator, but instead by {}",
                      *inst.getParent(), inst),
      };
    }
  }

  /**
   * A fallback method to error out on terminators that we don't yet handle.
   */
  void visitTerminator(llvm::Instruction &terminator) {
    throw std::runtime_error{
        fmt::format("TODO: Terminator not yet handled: {}", terminator)};
  }

  /**
   * Handle branches.
   */
  void visitBranchInst(llvm::BranchInst &inst) {
    llvm::BasicBlock *destination;
    if (inst.isConditional()) {
      // First, we read an entry off the tape and check that it's for the kind
      // of terminator that we are.
      auto const &entry = this->tape.next<tape::CondBr>("CondBr");

      // We can then determine the destination based on what got recorded in
      // the tape entry.
      destination = inst.getSuccessor(entry.taken ? 0 : 1);
    } else {
      // In an unconditional branch, we already know the next basic block to be
      // executed, so there isn't an entry in the tape for it.
      destination = inst.getSuccessor(0);
    }

    // Once we know the destination, we can do the "typical terminator logic."
    // This is in a method because it's shared by every control-flow construct,
    // but it's not terribly complicated.
    handleJumpTo(destination);
  }

  /**
   * Switch instructions look substantially similar to conditional branches.
   */
  void visitSwitchInst(llvm::SwitchInst &inst) {
    // We again read the expected an entry off the tape.
    auto const &entry = this->tape.next<tape::Switch>("Switch");

    // To look up what the destination is, we need to convert the integer
    // constant to an LLVM ConstantInt.
    //
    // TODO: Does something need to be done here w.r.t. signedness?
    auto value =
        llvm::ConstantInt::get(inst.getCondition()->getType(), entry.value);

    // We can then find the case that is taken, and jump to it!
    auto takenCase = inst.findCaseValue(llvm::cast<llvm::ConstantInt>(value));
    handleJumpTo(takenCase->getCaseSuccessor());
  }

  void handleJumpTo(llvm::BasicBlock *destination) {
    // Once we know we're jumping somewhere, we read the corresponding
    // BasicBlockStart entry off the tape and check the ID it contains with the
    // destination we computed from the terminator. This provides an extra
    // sanity check against unintended control flow.
    auto const &entry = this->tape.next<tape::BasicBlockStart>(
        "BasicBlockStart being jumped to");
    auto id = getBasicBlockAnnotation(destination);
    if (!id) {
      throw std::runtime_error{fmt::format(
          "Control went somewhere ({}) the annotation pass did not!",
          *destination)};
    } else if (entry.basic_block_id != *id) {
      if (!annotatedBasicBlocks.contains(entry.basic_block_id)) {
        throw std::logic_error{
            fmt::format("handleJumpTo: entry had an unknown basic block ID: {}",
                        entry.basic_block_id)};
      }
      auto basicBlockForEntry = annotatedBasicBlocks.at(entry.basic_block_id);
      throw std::runtime_error{fmt::format(
          "Tape mismatch: expected {} ({} in {}), found {} ({} in {})",
          tape::BasicBlockStart{*id}, *destination, *destination->getParent(),
          entry, *basicBlockForEntry, *basicBlockForEntry->getParent())};
    } else {
      this->nextInst = &destination->front();
    }
  }

  /**
   * A fallback method to error out on call-like instructions that we don't yet
   * handle.
   */
  void visitCallBase(llvm::CallBase &inst) {
    throw std::runtime_error{fmt::format(
        "TODO: handle {}", *static_cast<llvm::Instruction *>(&inst))};
  }

  /**
   * The handler for normal function calls.
   */
  void visitCallInst(llvm::CallInst &inst) {
    // If this is inline assembly (which for some reason, is represented as a
    // call), bail out.
    if (inst.isInlineAsm()) {
      spdlog::warn("Found inline assembly {}; bailing out",
                   *inst.getCalledOperand());
      this->bailedOut = true;
      return;
    }

    // Check for and discard the CallInfo tape entry.
    auto callInfo = this->tape.next<tape::CallInfo>("CallInfo");
    if (!callInfo.start) {
      throw std::logic_error{
          fmt::format("Expected a start CallInfo, got {}", callInfo)};
    }

    // Check that there aren't bundle operands, since those are semantically
    // meaningful and we don't handle them yet.
    if (inst.hasOperandBundles()) {
      throw std::runtime_error{
          fmt::format("TODO: handle operand bundles on {}",
                      *static_cast<llvm::Instruction *>(&inst))};
    }

    // Warn about attributes we don't handle yet.
    for (auto attributeSet : inst.getAttributes()) {
      for (auto attribute : attributeSet) {
        auto attrStr = attribute.getAsString();
        if (!this->config.isIgnorableFunctionAttribute(attrStr)) {

          spdlog::warn("TODO: handle attribute {} on {}", attrStr,
                       *static_cast<llvm::Instruction *>(&inst));
        }
      }
    }

    // We next need to get the callee; how we do so depends on whether the
    // call is indirect or not. If it is direct, we can simply get the called
    // function.
    llvm::Function *calledFunction = inst.getCalledFunction();
    if (!calledFunction) {
      // If it was indirect, we can instead peek at the tape to see if it has
      // a BasicBlockStart.
      //
      // TODO: This completely doesn't work on an indirect call to an external
      // function...
      auto const &tapeEntry = this->tape.peek<tape::BasicBlockStart>(
          "BasicBlockStart being indirectly called");

      if (!annotatedBasicBlocks.contains(tapeEntry.basic_block_id)) {
        throw std::logic_error{fmt::format(
            "visitCallInst: entry had an unknown basic block ID: {}",
            tapeEntry.basic_block_id)};
      }
      auto destination = annotatedBasicBlocks.at(tapeEntry.basic_block_id);
      if (&destination->getParent()->getEntryBlock() != destination) {
        throw std::logic_error{fmt::format(
            "BUG: {} (in {}) was not the entry block, but immediately "
            "followed the start of a function call",
            *destination, *destination->getParent())};
      }

      calledFunction = destination->getParent();
    }

    if (this->config.isSafeExternalFunction(calledFunction->getName().str())) {
      // If the function is on our list of safe external functions, we just
      // treat this call instruction like any other instruction, since we aren't
      // going to try to translate it.
      this->visitCallInstForSafeExternalFunction(inst);

    } else if (calledFunction->isDeclaration()) {
      // The callee might not be defined in the module (for example, if it's
      // one of the libc functions written in assembly); in that case, we bail
      // out. (If the function is in fact safe, e.g. memcpy), a solution might
      // be to put it in the safeExternalFunctionRegexes list.
      throw std::runtime_error{fmt::format(
          "Call to function not defined in this module: {}", *calledFunction)};

    } else if (auto arity = isSyscallWrapper(*calledFunction)) {
      // Check if the tape just ends here. If we got an error during the
      // syscall, it might. In that case, just bail out.
      if (this->tape.empty()) {
        spdlog::warn("Found early end to tape ({}); bailing out",
                     calledFunction->getName());
        this->bailedOut = true;
        return;
      }

      // We could also be calling a syscall wrapper; in that case, we treat it
      // as a normal instruction.
      this->visitInstruction(inst);

      // We drop the SyscallStart for the syscall wrapper.
      this->syscallsSoFar.push_back(
          &this->tape.next<tape::SyscallStart>("SyscallStart"));

      // We also drop the end CallInfo that follows the syscall wrapper
      // returning.
      auto callInfo = this->tape.next<tape::CallInfo>("CallInfo");
      if (callInfo.start) {
        throw std::logic_error{
            fmt::format("Expected an end CallInfo, got {}", callInfo)};
      }

    } else {
      // Finally, we're doing a normal, real function call to a normal, real
      // function.

      // Store this call instruction to the stack.
      this->stack.push_back(&inst);

      // Jump to the entry block of the called function.
      handleJumpTo(&calledFunction->getEntryBlock());
    }
  }

  void visitCallInstForSafeExternalFunction(llvm::CallInst &inst) {
    llvm::Function *calledFunction = inst.getCalledFunction();

    spdlog::debug("Treating function {} as a safe external function",
                  *calledFunction);
    this->visitInstruction(inst);

    if (calledFunction->isDeclaration()) {
      // If the function is external, we can just drop the end CallInfo.
      auto callInfo = this->tape.next<tape::CallInfo>("CallInfo");
      if (callInfo.start) {
        throw std::logic_error{
            fmt::format("Expected an end CallInfo, got {}", callInfo)};
      }
    } else {
      // If the function was secretly internal (TODO: Have a name for this?
      // "internal safe external function" is awful...), we "fast-forward"
      // the tape until the function returns. Since we have the call
      // entries, we just keep a counter of how many calls deeper than the
      // call to the safe external function we are. When we see an end
      // CallInfo entry and the counter is zero, we're done.
      size_t callDepth = 0;
      while (true) {
        if (this->tape.empty()) {
          throw std::logic_error{"Tried to advance tape while at end of tape"};
        }

        auto const &nextEntry =
            this->tape.next("internal safe external function entry to skip");
        if (std::holds_alternative<tape::CallInfo>(nextEntry)) {
          if (std::get<tape::CallInfo>(nextEntry).start)
            callDepth++;
          else if (callDepth == 0)
            break;
          else
            callDepth--;
        } else if (std::holds_alternative<tape::BasicBlockStart>(nextEntry)) {
          auto entry = std::get<tape::BasicBlockStart>(nextEntry);
          if (!annotatedBasicBlocks.contains(entry.basic_block_id)) {
            throw std::logic_error{
                fmt::format("visitCallInst: while skipping, entry had an "
                            "unknown basic block ID: {}",
                            entry.basic_block_id)};
          }
          auto destination = annotatedBasicBlocks.at(entry.basic_block_id);
          spdlog::trace("[{}] Skipping tape entry {} ({} in {})", callDepth,
                        nextEntry, *destination, *destination->getParent());
        } else {
          spdlog::trace("[{}] Skipping tape entry {}", callDepth, nextEntry);
        }
      }
    }
  }

  /**
   * We can more-or-less safely treat most of the intrinsics the same way as any
   * other instruction.
   */
  void visitIntrinsicInst(llvm::IntrinsicInst &inst) { visitInstruction(inst); }

  /**
   * Finally, we handle function returns.
   */
  void visitReturnInst(llvm::ReturnInst &inst) {
    // The Ret tape entry doesn't actually have any data in it, so although
    // we're reading it from the tape, that's only to check that it's the next
    // entry and discard it.
    this->tape.next<tape::Ret>("Ret");

    // Pop the stack.
    if (!this->stack.size()) {
      spdlog::warn(
          "Found a return instruction while the stack is empty. Bailing out!");
      this->bailedOut = true;
      return;
    }
    auto callInst = std::move(this->stack.back());
    this->stack.pop_back();

    // Eat the end CallInfo that we're returning to.
    auto callInfo = this->tape.next<tape::CallInfo>("CallInfo");
    if (callInfo.start) {
      throw std::logic_error{
          fmt::format("Expected an end CallInfo, got {}", callInfo)};
    }

    // Finally, we increment the instruction pointer to skip past the call
    // instruction.
    if (auto next = callInst->getNextNode()) {
      this->nextInst = next;
    } else {
      throw std::logic_error{fmt::format(
          "Invalid IR on return: Found a basic block ({}) not terminated by a "
          "terminator, but instead by {}",
          *callInst->getParent(), *static_cast<llvm::Instruction *>(callInst))};
    }
  }
};

TapeWalker::TapeWalker(Config const &config, llvm::Module &mod,
                       tape::Tape const &tape)
    : config(config), tape(tape) {
  this->annotatedBasicBlocks = findAnnotatedBasicBlocks(mod);

  if (auto mainFunction = mod.getFunction("main")) {
    // Set nextInst to be the first instruction in main.
    auto &mainEntry = mainFunction->getEntryBlock();
    this->nextInst = &*mainEntry.begin();

    // Check that the first tape entry was for the entry to main's first basic
    // block.
    if (auto mainEntryID = getBasicBlockAnnotation(&mainEntry)) {
      auto const &entry =
          this->tape.next<tape::BasicBlockStart>("BasicBlockStart for main");
      if (*mainEntryID != entry.basic_block_id) {
        throw std::logic_error{fmt::format(
            "main's entry block's annotation did not match the first "
            "tape entry ({} vs {})",
            *mainEntryID, entry.basic_block_id)};
      }
    } else {
      throw std::logic_error{
          fmt::format("main's entry block did not have an annotation")};
    }
  } else {
    throw std::runtime_error{"Missing main function"};
  }
}

auto TapeWalker::next() -> std::optional<InstructionWithStack> {
  // As a hack, check if we're about to enter the _Exit function. The hack is
  // that we're hardcoding the name, rather than instrumenting the function
  // itself specially; if the program declares a static _Exit function, the libc
  // one might get renamed and we'll go off the rails.
  //
  // TODO: dehack
  if (this->nextInst->getParent()->getParent()->getName() == "_Exit")
    return std::nullopt;

  // Even more hackily... we blow up for some reason in libc_exit_fini. To avoid
  // doing so, we bail out on a call to the exit function too.
  //
  // TODO: This is _definitely_ a hack around a bona fide bug.
  if (this->nextInst->getParent()->getParent()->getName() == "exit")
    return std::nullopt;

  // TODO: Yep, there's another bug here. See above for why this is a hack, etc.
  if (this->nextInst->getParent()->getParent()->getName() == "ioctl")
    return std::nullopt;

  auto out = InstructionWithStack{
      .stack = this->stack,
      .syscallsSoFar = this->syscallsSoFar,
      .inst = this->nextInst,
  };

  TapeVisitor visitor{
      .config = this->config,
      .tape = this->tape,
      .stack = this->stack,
      .syscallsSoFar = this->syscallsSoFar,
      .nextInst = this->nextInst,
      .annotatedBasicBlocks = this->annotatedBasicBlocks,
      .bailedOut = false,
  };
  try {
    // spdlog::trace("About to visit {}", *this->nextInst);
    visitor.visit(this->nextInst);
  } catch (...) {
    spdlog::error("Program stack trace:");
    for (auto const &callInst : this->stack)
      spdlog::error("  [{}, {}]{}", *callInst->getParent()->getParent(),
                    *callInst->getParent(),
                    *static_cast<llvm::Instruction *>(callInst));
    spdlog::error("  [{}, {}]{}", *this->nextInst->getParent()->getParent(),
                  *this->nextInst->getParent(), *this->nextInst);
    std::rethrow_exception(std::current_exception());
  }
  if (visitor.bailedOut)
    return std::nullopt;

  return out;
}

auto TapeWalker::collect() -> std::vector<InstructionWithStack> {
  std::vector<InstructionWithStack> out;
  while (auto instWithStack = this->next())
    out.push_back(*instWithStack);
  return out;
}

} // namespace guiness
