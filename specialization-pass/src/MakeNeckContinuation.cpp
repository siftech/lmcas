// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "MakeNeckContinuation.h"
#include "UpdateInstructionUtils.h"
#include <deque>
#include <spdlog/spdlog.h>

/*
 * Create a new BB for all child nodes in the dominator tree after the basic
 * block containing the call instruction which reached the neck. New BB is
 * associated with the child node BB in the stack frame BB locals. Function
 * accepts as input the dominator tree node holding the BB containing the
 * relevant call instruction, the current stack frame, and a deque to keep track
 * of the order in which the old BBs are copied.
 */
void createChildBBsAfterStackInstruction(
    llvm::DomTreeNodeBase<llvm::BasicBlock> *node, StackFrame &stackFrame,
    std::deque<llvm::BasicBlock *> &originalBBs) {

  for (auto child : node->children()) {
    auto BB = child->getBlock();
    // New BB parent should be the new function associated with the stack frame
    auto newBB = llvm::BasicBlock::Create(stackFrame.newFunction.getContext(),
                                          "", &stackFrame.newFunction);

    stackFrame.defineBBLocal(BB, newBB);
    originalBBs.push_back(BB);

    createChildBBsAfterStackInstruction(child, stackFrame, originalBBs);
  }
}

/*
 * Create a new basic block for every reachable basic block after the stack call
 * instruction that triggered the neck. Stores the order in which BBs were
 * copied in the deque originalBBs. Associates the new BB with the old reachable
 * BB in the stack frame locals for later translation. Accepts as input the node
 * in the dominator tree from which we should start copying BBs, the current
 * stack frame, and a deque of originalBBs to keep track of the order in which
 * old BBs are copied.
 */
void createBBsAfterStackInstruction(
    llvm::DomTreeNodeBase<llvm::BasicBlock> *node, StackFrame &stackFrame,
    std::deque<llvm::BasicBlock *> &originalBBs) {

  // Child BBs of the node instruction in the dominator tree should be copied
  // first
  createChildBBsAfterStackInstruction(node, stackFrame, originalBBs);

  // Copy all other reachable basic blocks after the neck call instruction
  // Iteration begins at the next instruction
  auto curInsnBB = stackFrame.insnPtr->getParent();
  auto endBB = stackFrame.function.end();

  for (auto BB = curInsnBB->getIterator(); BB != endBB; ++BB) {
    // If the basic block is already in originalBBs, this means it was copied
    // directly from the dominator tree and we do not need to recreate it
    auto foundBB = std::find(originalBBs.begin(), originalBBs.end(), &(*BB));
    if (foundBB == originalBBs.end()) {
      // New BB parent should be the new function associated with the stack
      // frame
      auto newBB = llvm::BasicBlock::Create(stackFrame.newFunction.getContext(),
                                            "", &stackFrame.newFunction);
      stackFrame.defineBBLocal(&(*BB), newBB);
      originalBBs.push_back(&(*BB));
    }
  }
}

/*
 * Copy all instructions starting after the current stack frame
 * instruction pointer into the builder, specifically only those
 * instructions in the same BB as the neck call instruction.
 * The current instruction should be the one which triggered the
 * call to the neck.
 */
void copyRemainingCallBBInstructions(StackFrame &stackFrame,
                                     llvm::IRBuilder<> &builder) {

  while (auto nextInsn = stackFrame.insnPtr->getNextNode()) {
    auto newInstruction = nextInsn->clone();

    // Translate instruction operands and successors
    translateInsnOperands(stackFrame, newInstruction, nextInsn);
    if (newInstruction->isTerminator()) {
      translateInsnSuccessors(stackFrame, newInstruction);
    }

    builder.Insert(newInstruction);
    stackFrame.defineLocal(nextInsn, newInstruction);

    stackFrame.insnPtr = nextInsn;
  }
}

/*
 * Copies the data from the original phinode to the dummy one that was created
 * during the copying of post-neck instructions.
 */
void updateDummyPhinodesWithData(
    absl::flat_hash_map<llvm::PHINode *, llvm::PHINode *> &phinodes,
    StackFrame &stackFrame) {

  for (auto entry : phinodes) {
    auto oldPhiNode = std::get<0>(entry);
    auto newPhiNode = std::get<1>(entry);

    for (unsigned i = 0; i < oldPhiNode->getNumIncomingValues(); i++) {
      // For every incoming value in the old phinode, translate both
      // the value and the BB and add them as incoming to the new phinode
      auto originalBB = oldPhiNode->getIncomingBlock(i);
      auto translatedBB = stackFrame.translateBBValue(originalBB);
      auto selectedValue = oldPhiNode->getIncomingValue(i);
      auto translatedValue =
          stackFrame.translateValue(*oldPhiNode, selectedValue);

      newPhiNode->addIncoming(translatedValue, translatedBB);
    }
  }
}

/*
 * Copy all instructions after the call instruction which triggered
 * the neck call in the current stack frame. Will be all instructions
 * directly after the neck if this is the first stack frame in the stack.
 * Accepts as input the current stack frame, the dominator tree generated
 * from old function associated with the stack frame, and the IR builder.
 */
void copyFuncInstructionsAfterNeck(StackFrame &stackFrame,
                                   llvm::DominatorTree &DT,
                                   llvm::IRBuilder<> &builder) {

  // Find the node containing the parent BB of the neck-triggering call
  // instruction
  auto callInsnBB = stackFrame.insnPtr->getParent();
  auto callInsnBBnode = DT.getNode(callInsnBB);

  // Create a deque to keep track of the order in which old BBs are traversed
  std::deque<llvm::BasicBlock *> originalBBs = {};

  // Create new BBs for every reachable BB after the neck, associating
  // them with the original BB in the stack frame BB locals. Copy the
  // immediate instructions after the neck triggering call in the current BB.
  createBBsAfterStackInstruction(callInsnBBnode, stackFrame, originalBBs);
  copyRemainingCallBBInstructions(stackFrame, builder);

  // Keep track of encountered phinodes since these may have incoming blocks
  // that have not yet been copied. We map original phinodes to "dummy"
  // phinodes, that we will handle after all other BB instructions have been
  // copied.
  absl::flat_hash_map<llvm::PHINode *, llvm::PHINode *> phinodes;

  while (!originalBBs.empty()) {
    auto originalBB = originalBBs.front();
    auto translatedBB = stackFrame.translateBBValue(originalBB);

    // Set instruction insertion point to the new BB associated with the
    // original
    builder.SetInsertPoint(translatedBB);
    for (auto &insn : *originalBB) {
      if (auto phiNode = llvm::dyn_cast<llvm::PHINode>(&insn)) {
        // If the instruction is a phinode, create a new "dummy" phinode with
        // no operands and define it in the locals. This will be updated after
        // all other BBs have been copied.
        auto dummyPhiNode = builder.CreatePHI(insn.getType(), 0);
        phinodes.insert_or_assign(phiNode, dummyPhiNode);
        stackFrame.defineLocal(&insn, dummyPhiNode);
      } else {
        auto newInstruction = insn.clone();

        // Translate instruction operands and successors
        translateInsnOperands(stackFrame, newInstruction, &insn);
        if (newInstruction->isTerminator()) {
          translateInsnSuccessors(stackFrame, newInstruction);
        }

        builder.Insert(newInstruction);
        stackFrame.defineLocal(&insn, newInstruction);
      }
    }
    originalBBs.pop_front();
  }

  // Update all dummy phinodes with the data from the original phinode
  updateDummyPhinodesWithData(phinodes, stackFrame);
}
