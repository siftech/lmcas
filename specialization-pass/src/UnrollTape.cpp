// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "UnrollTape.h"
#include "FmtLlvm.h"
#include "SyscallHandlers.h"
#include "SyscallWrappers.h"
#include "UpdateInstructionUtils.h"
#include <absl/container/flat_hash_map.h>
#include <llvm/IR/InstVisitor.h>
#include <sstream>

/**
 * The set of function attributes that we can safely handle without doing
 * anything special.
 */
const absl::flat_hash_set<std::string> ignorableFunctionAttributes = {
    // The various optimization levels don't result in semantic changes.
    "optsize",

    // We're insensitive to function termination (since we have the tape, we
    // know which functions terminated!), so we can ignore these attributes.
    "nounwind",
    "willreturn",

    // We don't compute values directly, so these attributes can be ignored too.
    "readnone",
    "signext",
    "zeroext",

    // We don't perform the optimizations these attributes inhibit, so we can
    // ignore them.
    "nobuiltin",
    "\"no-builtins\"",
    "strictfp",
    "\"strictfp\"",
};

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

/**
 * Reads a tape entry from tapeIterator without advancing it. If it was of
 * type T, return it; else, throw an exception. tapeIterator must not be at
 * the end.
 */
template <typename T>
const T &peekTapeEntry(const tape::Tape::const_iterator &tapeIterator,
                       const tape::Tape::const_iterator &tapeIteratorEnd,
                       std::string_view typeName) {
  spdlog::trace("Expecting a {}", typeName);
  if (tapeIterator == tapeIteratorEnd) {
    spdlog::error("Tried to advance tape while at end of tape");
    throw std::runtime_error{"Tried to advance tape while at end of tape"};
  }

  const auto &tapeEntry = *tapeIterator;
  if (!std::holds_alternative<T>(tapeEntry)) {
    spdlog::error("Tape mismatch: expected a {}, found {}", typeName,
                  tapeEntry);
    throw std::runtime_error{"Tape mismatch"};
  }
  return std::get<T>(tapeEntry);
}

/**
 * Reads a tape entry from tapeIterator, advancing it. If it was of type T,
 * return it; else, throw an exception. tapeIterator must not be at the end.
 */
template <typename T>
const T &nextTapeEntry(tape::Tape::const_iterator &tapeIterator,
                       const tape::Tape::const_iterator &tapeIteratorEnd,
                       std::string_view typeName) {
  const auto &out = peekTapeEntry<T>(tapeIterator, tapeIteratorEnd, typeName);
  ++tapeIterator;
  spdlog::trace("Advancing past {} ({} remaining)", out,
                tapeIteratorEnd - tapeIterator);
  return out;
}

/**
 * The body of the main loop run when laying out the tape.
 */
static void
stepInstruction(const std::vector<std::regex> &safeExternalFunctionRegexes,
                const absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
                    &annotatedBasicBlocks,
                SyscallHandlers &syscallHandlers,
                tape::Tape::const_iterator &tapeIterator,
                const tape::Tape::const_iterator &tapeIteratorEnd,
                llvm::IRBuilder<> &builder, llvm::IRBuilder<> &allocaBuilder,
                std::vector<StackFrame> &stack);

std::vector<StackFrame>
unrollTape(const tape::Tape &tape,
           const std::vector<std::regex> &safeExternalFunctionRegexes,
           llvm::Value *argc, llvm::Value *argv, llvm::Function &mainFunction,
           llvm::Function &newMainFunction,
           absl::flat_hash_set<llvm::Instruction *> neckCallMarkers,
           llvm::IRBuilder<> &allocaBuilder, llvm::IRBuilder<> &builder) {
  // Since the tape identifies basic blocks by their IDs, we want to make it
  // efficient to look them up by ID, so we collect that map.
  auto annotatedBasicBlocks =
      findAnnotatedBasicBlocks(*builder.GetInsertBlock()->getModule());

  // We also have an object dedicated to storing information about the state
  // used to simulate syscalls.
  SyscallHandlers syscallHandlers(builder);

  // The final big piece of bookkeeping information we need is the stack; this
  // will keep track of what we've unrolled in each stack frame.
  //
  // We start with a stack frame oriented at the start of the main function. We
  // then insert instructions for the arguments that the main function takes.
  //
  // TODO: envp as a third argument to main is not in POSIX, but is also not
  // uncommon.
  std::vector<StackFrame> stack{{mainFunction,
                                 newMainFunction,
                                 &mainFunction.getEntryBlock().front(),
                                 {},
                                 {},
                                 std::nullopt}};
  switch (std::distance(mainFunction.arg_begin(), mainFunction.arg_end())) {
  case 2:
    stack[0].defineLocal(mainFunction.getArg(1), argv);
    // A one-argument main is also technically not allowed in C11, and is kinda
    // weird, but is easy to support and commonly supported "by coincidence."
  case 1:
    stack[0].defineLocal(mainFunction.getArg(0), argc);
  case 0:
    break;

  default:
    throw std::runtime_error{"TODO: Unsupported number of arguments to main"};
  }

  // The initial entry on the tape is a BasicBlockStart for main's entry basic
  // block; since we don't per se jump to it, stepInstruction won't consume it
  // automatically. We can consume it here instead.
  auto tapeIterator = tape.begin();
  auto tapeIteratorEnd = tape.end();
  const auto &firstTapeEntry = nextTapeEntry<tape::BasicBlockStart>(
      tapeIterator, tapeIteratorEnd, "BasicBlockStart");
  if (auto mainEntryBasicBlockID =
          getBasicBlockAnnotation(&mainFunction.getEntryBlock())) {
    if (*mainEntryBasicBlockID != firstTapeEntry.basic_block_id) {
      spdlog::error("main's entry block's annotation did not match the first "
                    "tape entry ({} vs {})",
                    *mainEntryBasicBlockID, firstTapeEntry.basic_block_id);
      throw std::logic_error{"BUG"};
    }
  } else {
    spdlog::error("main's entry block did not have an annotation");
    throw std::logic_error{"BUG"};
  }

  // We then continuously walk through the tape until we reach the neck.
  try {
    while (!neckCallMarkers.contains(stack.back().insnPtr))
      stepInstruction(safeExternalFunctionRegexes, annotatedBasicBlocks,
                      syscallHandlers, tapeIterator, tapeIteratorEnd, builder,
                      allocaBuilder, stack);
  } catch (...) {
    spdlog::error("Program Stack Trace:");
    for (const auto &stackFrame : stack)
      spdlog::error("  {}", stackFrame.function);
    std::rethrow_exception(std::current_exception());
  }

  // TODO: HACK: Unlike pretty much every real ISA, our stack frames don't have
  // the address to return to, they have the address of the call instruction. As
  // a result, when we put the half-built frames back together after finding the
  // neck, we skip every instruction pointer on the stack. This has the neat
  // effect of skipping over a call to _lmcas_neck, in the case where that
  // is used.
  //
  // However, when the neck is marked at a program point rather than at a
  // function call, we don't want to skip the immediately following instruction,
  // but want to not process the call instructions in the parent frames.
  //
  // The right refactor is probably to pass the call instruction information we
  // need down to the child ("it's a link register"), and not skip any
  // instructions. However, an easy trick that works for now is to insert a
  // no-op instruction here, which will then get skipped.
  stack.back().insnPtr = llvm::CallInst::Create(
      llvm::Intrinsic::getDeclaration(stack.back().function.getParent(),
                                      llvm::Intrinsic::donothing),
      "", stack.back().insnPtr);

  // Once we've stopped execution, we extract the locals of each stack frame and
  // return them.
  for (auto &stackFrame : stack) {
    // We need to check for varargs functions; we can't construct an artificial
    // va_list, so we can't pass one to the neck continuation.
    if (stackFrame.function.isVarArg()) {
      throw std::runtime_error{fmt::format(
          "TODO: cannot have a varargs function ({}) on the stack at the neck",
          stackFrame.function)};
    }
  }

  return stack;
}

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

/**
 * A visitor for implementing stepInstruction.
 *
 * Preconditions:
 *
 * - builder must be in a valid state to insert instructions
 * - stack must not be empty
 * - tapeIterator must not at the end
 */
struct StepInstructionVisitor
    : public llvm::InstVisitor<StepInstructionVisitor> {
  // These are each exactly the arguments to stepInstruction.
  const std::vector<std::regex> &safeExternalFunctionRegexes;
  const absl::flat_hash_map<uint64_t, llvm::BasicBlock *> &annotatedBasicBlocks;
  SyscallHandlers &syscallHandlers;
  tape::Tape::const_iterator &tapeIterator;
  const tape::Tape::const_iterator &tapeIteratorEnd;
  llvm::IRBuilder<> &builder, &allocaBuilder;
  std::vector<StackFrame> &stack;

  StepInstructionVisitor(
      const std::vector<std::regex> &safeExternalFunctionRegexes,
      const absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
          &annotatedBasicBlocks,
      SyscallHandlers &syscallHandlers,
      tape::Tape::const_iterator &tapeIterator,
      const tape::Tape::const_iterator &tapeIteratorEnd,
      llvm::IRBuilder<> &builder, llvm::IRBuilder<> &allocaBuilder,
      std::vector<StackFrame> &stack)
      : safeExternalFunctionRegexes(safeExternalFunctionRegexes),
        annotatedBasicBlocks(annotatedBasicBlocks),
        syscallHandlers(syscallHandlers), tapeIterator(tapeIterator),
        tapeIteratorEnd(tapeIteratorEnd), builder(builder),
        allocaBuilder(allocaBuilder), stack(stack) {}

  /**
   * Reads a tape entry from tapeIterator without advancing it. If it was of
   * type T, return it; else, throw an exception. tapeIterator must not be at
   * the end.
   */
  template <typename T> const T &peekTapeEntry(std::string_view typeName) {
    return ::peekTapeEntry<T>(this->tapeIterator, this->tapeIteratorEnd,
                              typeName);
  }

  /**
   * Reads a tape entry from tapeIterator, advancing it. If it was of type T,
   * return it; else, throw an exception. tapeIterator must not be at the end.
   */
  template <typename T> const T &nextTapeEntry(std::string_view typeName) {
    return ::nextTapeEntry<T>(this->tapeIterator, this->tapeIteratorEnd,
                              typeName);
  }

  // We handle the default case of an instruction that is neither a call nor a
  // terminator by cloning it, replacing its arguments in accordance with the
  // locals for the current function, and appending it to the builder.
  void visitInstruction(llvm::Instruction &instruction) {
    // Clone the instruction.
    auto newInstruction = instruction.clone();

    // Update the instruction's operands.
    translateInsnOperands(stack.back(), newInstruction, &instruction);

    // Add the instruction to the basic block.
    builder.Insert(newInstruction);

    // Put the value into the locals.
    stack.back().defineLocal(&instruction, newInstruction);

    // Continue to the next instruction in the basic block.
    nextInsn(instruction);
  }

  llvm::Value *translateValue(llvm::Instruction &instruction,
                              llvm::Value *originalValue) {
    return stack.back().translateValue(instruction, originalValue);
  }

  // Another common task is advancing the "instruction pointer" from the
  // current (non-terminator!) instruction to a next one.
  void nextInsn(llvm::Instruction &instruction) {
    if (auto next = instruction.getNextNode()) {
      stack.back().insnPtr = next;
    } else {
      spdlog::error("Invalid IR: Found a basic block ({}) not terminated by a "
                    "terminator, but instead by {}",
                    *instruction.getParent(), instruction);
      throw std::logic_error{"Invalid IR"};
    }
  }

  // Allocas need to be handled specially, by hoisting ones with constant
  // arguments to the start of the function.
  //
  // This is necessary to get reasonable code out of LLVM; it doesn't hoist
  // allocas on its own, and generates code that adjusts the stack for each
  // block of allocas. As a result, in the region of main corresponding to the
  // unrolled tape, the stack pointer is constantly being adjusted, which makes
  // the assembly approximately unreadable to a human (well, at least to this
  // human).
  //
  // See https://github.com/ziglang/zig/issues/7689 for another example of
  // people being sad from this.
  void visitAllocaInst(llvm::AllocaInst &instruction) {
    if (llvm::isa<llvm::ConstantInt>(instruction.getArraySize()) &&
        !instruction.isUsedWithInAlloca()) {
      // Create the new alloca, add it to the entry block, and put the mapping
      // into the locals.
      //
      // We don't need to fix up the argument, since we just determined it to
      // be a constant. TODO: Are there any other arguments that might need to
      // be fixed up? Metadata?
      auto newInstruction = instruction.clone();
      allocaBuilder.Insert(newInstruction);
      stack.back().defineLocal(&instruction, newInstruction);

      // Continue to the next instruction in the basic block.
      nextInsn(instruction);
    } else {
      // If the alloca doesn't have a constant size, or is used as an inalloca
      // argument (https://llvm.org/docs/InAlloca.html), it might not be
      // semantics-preserving to move it, so we treat it like any other
      // instruction.
      //
      // This *shouldn't* result in awful unreadable machine code, mostly
      // because these situations are rare.
      visitInstruction(instruction);
    }
  }

  // We handle terminators individually. To ensure that we have full coverage,
  // first we have a blanket case for those we don't yet handle.
  void visitTerminator(llvm::Instruction &terminator) {
    throw std::runtime_error{
        fmt::format("TODO: Terminator not yet handled: {}", terminator)};
  }

  // Otherwise, for each terminator, the logic is pretty similar, of course with
  // per-terminator differences.
  //
  // Branches are a slightly special case, since the destination is determined
  // differently depending on whether they're conditional or not. Looking at
  // them should be instructive as a first example, though.
  void visitBranchInst(llvm::BranchInst &instruction) {
    llvm::BasicBlock *destination;
    if (instruction.isConditional()) {
      // First, we read an entry off the tape and check that it's for the kind
      // of terminator that we are.
      const auto &entry = this->nextTapeEntry<tape::CondBr>("CondBr");

      // We can then determine the destination based on what got recorded in
      // the tape entry.
      destination = instruction.getSuccessor(entry.taken ? 0 : 1);
    } else {
      // In an unconditional branch, we already know the next basic block to be
      // executed, so there isn't an entry in the tape for it.
      destination = instruction.getSuccessor(0);
    }

    // Once we know the destination, we can do the "typical terminator logic."
    // This is in a method because it's shared by every control-flow construct,
    // but it's not terribly complicated.
    handleJumpTo(destination);
  }

  void handleJumpTo(llvm::BasicBlock *destination) {
    // Once we know we're jumping somewhere, we read the corresponding
    // BasicBlockStart entry off the tape and check the ID it contains with the
    // destination we computed from the terminator. This provides an extra
    // sanity check against unintended control flow.
    const auto &entry =
        this->nextTapeEntry<tape::BasicBlockStart>("BasicBlockStart");
    auto id = getBasicBlockAnnotation(destination);
    if (!id) {
      spdlog::error("Control went somewhere ({}) the annotation pass did not!",
                    *destination);
      throw std::runtime_error{"Tape mismatch"};
    } else if (entry.basic_block_id != *id) {
      if (!annotatedBasicBlocks.contains(entry.basic_block_id)) {
        spdlog::error("handleJumpTo: entry had an unknown basic block ID: {}",
                      entry.basic_block_id);
        throw std::logic_error{"BUG"};
      }
      auto basicBlockForEntry = annotatedBasicBlocks.at(entry.basic_block_id);
      spdlog::error(
          "Tape mismatch: expected {} ({} in {}), found {} ({} in {})",
          tape::BasicBlockStart{*id}, *destination, *destination->getParent(),
          entry, *basicBlockForEntry, *basicBlockForEntry->getParent());
      throw std::runtime_error{"Tape mismatch"};
    }

    // We keep the previous basic block around for the time being, so we can use
    // it to resolve phi nodes.
    //
    // If currentFrame.insnPtr is nullptr (as it is on when starting a call), we
    // make oldBasicBlock also be nullptr. This is fine, since phi nodes can't
    // appear at the start of a function.
    auto &currentFrame = stack.back();
    auto oldBasicBlock =
        currentFrame.insnPtr ? currentFrame.insnPtr->getParent() : nullptr;

    // Once we've validated where we're going, we can set the instruction
    // pointer there.
    currentFrame.insnPtr = &destination->front();

    // Finally, we resolve all the phi nodes. They're always at the start of a
    // basic block, so we can easily wipe them out now.
    while (auto phiNode = llvm::dyn_cast<llvm::PHINode>(currentFrame.insnPtr)) {
      auto selectedValue = phiNode->getIncomingValueForBlock(oldBasicBlock);
      currentFrame.defineLocal(
          phiNode, translateValue(*currentFrame.insnPtr, selectedValue));
      if (auto next = currentFrame.insnPtr->getNextNode()) {
        currentFrame.insnPtr = next;
      } else {
        spdlog::error("Invalid IR on jump: Found a basic block ({}) not "
                      "terminated by a terminator, but instead by {}",
                      *currentFrame.insnPtr->getParent(),
                      *currentFrame.insnPtr);
        throw std::logic_error{"Invalid IR"};
      }
    }
  }

  // Switch instructions look substantially similar to conditional branches.
  void visitSwitchInst(llvm::SwitchInst &instruction) {
    // We again read the expected an entry off the tape.
    const auto &entry = this->nextTapeEntry<tape::Switch>("Switch");

    // To look up what the destination is, we need to convert the integer
    // constant to an LLVM ConstantInt.
    //
    // TODO: Does something need to be done here w.r.t. signedness?
    auto value = llvm::ConstantInt::get(instruction.getCondition()->getType(),
                                        entry.value);

    // We can then find the case that is taken, and jump to it!
    auto takenCase =
        instruction.findCaseValue(llvm::cast<llvm::ConstantInt>(value));
    handleJumpTo(takenCase->getCaseSuccessor());
  }

  // Our last special case is call instructions. These need to be handled
  // carefully, so we default to erroring out if we encounter one we can't
  // handle.
  void visitCallBase(llvm::CallBase &instruction) {
    throw std::runtime_error{fmt::format(
        "TODO: handle {}", *static_cast<llvm::Instruction *>(&instruction))};
  }

  /**
   * Appends a call to puts with the given string to the tape, if the body of
   * this function isn't commented out.
   */
  void appendTracePuts(const std::string &msg) {
    // auto mod = builder.GetInsertBlock()->getParent()->getParent();
    // builder.CreateCall(mod->getFunction("puts"),
    // {builder.CreateGlobalStringPtr(msg)});
  }

  // The bulk of the work for normal function calls happens in, logically
  // enough, the case for call instructions.
  void visitCallInst(llvm::CallInst &instruction) {
    // If this is inline assembly (which for some reason, is represented as a
    // call), bail out.
    if (instruction.isInlineAsm()) {
      spdlog::error("Encountered inline assembly: {}",
                    *instruction.getCalledOperand());
      throw std::runtime_error{"Inline asm"};
    }

    // Check for and discard the CallInfo tape entry.
    auto callInfo = this->nextTapeEntry<tape::CallInfo>("CallInfo");
    if (!callInfo.start) {
      spdlog::error("Expected a start CallInfo, got {}", callInfo);
      throw std::logic_error{"BUG"};
    }

    // Check that there aren't bundle operands, since those are semantically
    // meaningful and we don't handle them yet.
    if (instruction.hasOperandBundles()) {
      throw std::runtime_error{
          fmt::format("TODO: handle operand bundles on {}",
                      *static_cast<llvm::Instruction *>(&instruction))};
    }

    // Warn about attributes we don't handle yet.
    for (auto attributeSet : instruction.getAttributes()) {
      for (auto attribute : attributeSet) {
        auto attrStr = attribute.getAsString();

        // If it's in our ignorable set... ignore it!
        if (ignorableFunctionAttributes.find(attrStr) !=
            ignorableFunctionAttributes.end())
          continue;

        spdlog::warn("TODO: handle attribute {} on {}", attrStr,
                     *static_cast<llvm::Instruction *>(&instruction));
      }
    }

    // We next need to get the callee; how we do so depends on whether the call
    // is indirect or not. If it is direct, we can simply get the called
    // function.
    llvm::Function *calledFunction = instruction.getCalledFunction();
    if (!calledFunction) {
      // If it was indirect, we can instead peek at the tape to see if it has a
      // BasicBlockStart.
      //
      // TODO: This completely doesn't work on an indirect call to an external
      // function...
      const auto &tapeEntry =
          this->peekTapeEntry<tape::BasicBlockStart>("BasicBlockStart");

      if (!annotatedBasicBlocks.contains(tapeEntry.basic_block_id)) {
        spdlog::error("visitCallInst: entry had an unknown basic block ID: {}",
                      tapeEntry.basic_block_id);
        throw std::logic_error{"BUG"};
      }
      auto destination = annotatedBasicBlocks.at(tapeEntry.basic_block_id);
      if (&destination->getParent()->getEntryBlock() != destination) {
        spdlog::error("BUG: {} (in {}) was not an entry block, but immediately "
                      "followed the start of a function call",
                      *destination, *destination->getParent());
        throw std::logic_error{"BUG"};
      }

      calledFunction = destination->getParent();
    }

    // If the function is on our list of safe external functions, we just treat
    // this call instruction like any other instruction, since we aren't going
    // to try to translate it.
    if (isSafeExternalFunction(calledFunction->getName().str())) {
      spdlog::debug("Treating function {} as a safe external function",
                    *calledFunction);
      appendTracePuts(
          fmt::format("I {}", *llvm::cast<llvm::Instruction>(&instruction)));
      visitInstruction(instruction);

      if (calledFunction->isDeclaration()) {
        // If the function is external, we can just eat the end CallInfo.
        auto callInfo = this->nextTapeEntry<tape::CallInfo>("CallInfo");
        if (callInfo.start) {
          spdlog::error("Expected an end CallInfo, got {}", callInfo);
          throw std::logic_error{"BUG"};
        }
      } else {
        // If the function was secretly internal (TODO: Have a name for this?
        // "internal safe external function" is awful...), we "fast-forward" the
        // tape until the function returns. Since we have the call entries, we
        // just keep a counter of how many calls deeper than the call to the
        // safe external function we are. When we see an end CallInfo entry and
        // the counter is zero, we're done.
        size_t callDepth = 0;
        while (true) {
          if (tapeIterator == tapeIteratorEnd) {
            spdlog::error("Tried to advance tape while at end of tape");
            throw std::logic_error{
                "Tried to advance tape while at end of tape"};
          }

          const auto &nextEntry = *tapeIterator++;
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
              spdlog::error("visitCallInst: while skipping, entry had an "
                            "unknown basic block ID: {}",
                            entry.basic_block_id);
              throw std::logic_error{"BUG"};
            }
            auto destination = annotatedBasicBlocks.at(entry.basic_block_id);
            spdlog::debug("[{}] Skipping tape entry {} ({} in {})", callDepth,
                          nextEntry, *destination, *destination->getParent());
          } else {
            spdlog::debug("[{}] Skipping tape entry {}", callDepth, nextEntry);
          }
        }
      }
    } else if (calledFunction->isDeclaration()) {
      // The callee might not be defined in the module (for example, if it's one
      // of the libc functions written in assembly); in that case, we bail out.
      // (If the function is in fact safe, e.g. memcpy), a solution might be to
      // put it in the safeExternalFunctionRegexes list.
      throw std::runtime_error{fmt::format(
          "Call to function not defined in this module: {}", *calledFunction)};
    } else if (auto arity = isSyscallWrapper(*calledFunction)) {
      // We could also be calling a syscall wrapper; in that case, we dispatch
      // to the appropriate syscall handler.
      const auto &tapeEntry =
          this->nextTapeEntry<tape::SyscallStart>("SyscallStart");
      auto returnValue = syscallHandlers.performSyscall(
          instruction, tapeEntry,
          [this, &instruction](llvm::Value *originalValue) {
            return translateValue(instruction, originalValue);
          },
          annotatedBasicBlocks);
      stack.back().defineLocal(&instruction, returnValue);
      nextInsn(instruction);

      // We eat the end CallInfo that follows the syscall wrapper returning.
      auto callInfo = this->nextTapeEntry<tape::CallInfo>("CallInfo");
      if (callInfo.start) {
        spdlog::error("Expected an end CallInfo, got {}", callInfo);
        throw std::logic_error{"BUG"};
      }
    } else {
      spdlog::trace("Calling {}", *calledFunction);

      // If the instruction was a call to a real function, we can proceed by
      // making a "taped" copy of it and calling that.
      auto newFunction = llvm::Function::Create(
          calledFunction->getFunctionType(),
          llvm::GlobalValue::LinkageTypes::InternalLinkage,
          fmt::format("_tabacco_callee_{}", calledFunction->getName()),
          calledFunction->getParent());
      newFunction->setSection("tabacco_pre_neck");
      llvm::BasicBlock::Create(newFunction->getContext(), "", newFunction);

      // Create a call to the new function.
      auto newCall = llvm::cast<llvm::CallInst>(instruction.clone());
      newCall->setCalledOperand(builder.CreateBitCast(
          newFunction, instruction.getCalledOperand()->getType()));
      for (unsigned i = 0; i < instruction.arg_size(); i++) {
        auto arg = instruction.getArgOperand(i);
        auto newArg = translateValue(instruction, arg);
        newCall->setArgOperand(i, newArg);
      }

      // Add the instruction to the current basic block and the locals.
      appendTracePuts(
          fmt::format("< {}", *llvm::cast<llvm::Instruction>(&instruction)));
      builder.Insert(newCall);
      appendTracePuts(
          fmt::format("> {}", *llvm::cast<llvm::Instruction>(&instruction)));
      stack.back().defineLocal(&instruction, newCall);

      // Create an initial locals map, just mapping arguments from the original
      // function to the new one.
      absl::flat_hash_map<llvm::Value *, llvm::Value *> locals;
      for (unsigned i = 0; i < calledFunction->arg_size(); i++) {
        locals.insert_or_assign(calledFunction->getArg(i),
                                newFunction->getArg(i));
      }

      // As the final piece of preparation for the call, we construct a new
      // stack frame. This doesn't actually need an instruction pointer set;
      // handleJumpTo will do that for us.
      stack.push_back(
          {*calledFunction,
           *newFunction,
           nullptr,
           std::move(locals),
           {},
           {{allocaBuilder.GetInsertBlock(), allocaBuilder.GetInsertPoint(),
             builder.GetInsertBlock(), builder.GetInsertPoint()}}});

      // Finally, we do the call by jumping to the entry block of the called
      // function and setting the insertion point there.
      //
      // Note that unlike most CPU architectures, we do *not* advance the
      // instruction pointer on function call; this is because returns will
      // perform some extra book-keeping.
      allocaBuilder.SetInsertPoint(&newFunction->getEntryBlock());
      builder.SetInsertPoint(&newFunction->getEntryBlock());
      handleJumpTo(&calledFunction->getEntryBlock());
    }
  }

  // To make the above code slightly simpler, we factor out the check for
  // whether a call should be translated as just that.
  bool isSafeExternalFunction(const std::string &name) {
    // We just check if each regex is a match.
    for (const auto &regex : safeExternalFunctionRegexes)
      if (std::regex_match(name, regex))
        return true;
    return false;
  }

  // We can more-or-less safely treat most of the intrinsics the same way as
  // any other instruction.
  void visitIntrinsicInst(llvm::IntrinsicInst &instruction) {
    visitInstruction(instruction);
  }

  // Next up, we look at returns.
  void visitReturnInst(llvm::ReturnInst &instruction) {
    // The Ret tape entry doesn't actually have any data in it, so although
    // we're reading it from the tape, that's only to check that it's the next
    // entry and discard it.
    this->nextTapeEntry<tape::Ret>("Ret");

    // If the return had a value, we need to translate it (in the current
    // frame) and return it to the parent frame.
    if (auto originalReturnValue = instruction.getReturnValue()) {
      auto returnValue = translateValue(instruction, originalReturnValue);
      builder.CreateRet(returnValue);
    } else {
      builder.CreateRetVoid();
    }

    std::string debugString;
    {
      llvm::raw_string_ostream stream(debugString);
      builder.GetInsertBlock()->getParent()->print(stream);
    }
    spdlog::trace("done with function: {}", debugString);

    // We then pop the current stack frame, discarding it and resetting the
    // builder's insertion point.
    auto insertionPointsOnReturn = stack.back().insertionPointsOnReturn.value();
    allocaBuilder.SetInsertPoint(std::get<0>(insertionPointsOnReturn),
                                 std::get<1>(insertionPointsOnReturn));
    builder.SetInsertPoint(std::get<2>(insertionPointsOnReturn),
                           std::get<3>(insertionPointsOnReturn));
    stack.pop_back();

    // We eat the end CallInfo that we returned to.
    auto callInfo = this->nextTapeEntry<tape::CallInfo>("CallInfo");
    if (callInfo.start) {
      spdlog::error("Expected an end CallInfo, got {}", callInfo);
      throw std::logic_error{"BUG"};
    }

    // Finally, we increment the instruction pointer to skip past the call
    // instruction.
    auto &parentFrame = stack.back();
    if (auto next = parentFrame.insnPtr->getNextNode()) {
      parentFrame.insnPtr = next;
    } else {
      spdlog::error(
          "Invalid IR on return: Found a basic block ({}) not terminated by a "
          "terminator, but instead by {}",
          *parentFrame.insnPtr->getParent(), *parentFrame.insnPtr);
      throw std::logic_error{"Invalid IR"};
    }
  }
};

static void
stepInstruction(const std::vector<std::regex> &safeExternalFunctionRegexes,
                const absl::flat_hash_map<uint64_t, llvm::BasicBlock *>
                    &annotatedBasicBlocks,
                SyscallHandlers &syscallHandlers,
                tape::Tape::const_iterator &tapeIterator,
                const tape::Tape::const_iterator &tapeIteratorEnd,
                llvm::IRBuilder<> &builder, llvm::IRBuilder<> &allocaBuilder,
                std::vector<StackFrame> &stack) {
  // A visitor is used to do all the actual work.
  StepInstructionVisitor visitor = StepInstructionVisitor(
      safeExternalFunctionRegexes, annotatedBasicBlocks, syscallHandlers,
      tapeIterator, tapeIteratorEnd, builder, allocaBuilder, stack);
  spdlog::trace("{}", *stack.back().insnPtr);
  visitor.visit(stack.back().insnPtr);
}
