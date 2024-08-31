// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "UpdateInstructionUtils.h"

// Updates a new instruction's operands with the translated values from the old
// instruction
void translateInsnOperands(StackFrame &stackFrame,
                           llvm::Instruction *newInstruction,
                           llvm::Instruction *oldInstruction) {
  for (unsigned i = 0; i < newInstruction->getNumOperands(); i++) {
    auto operand = newInstruction->getOperand(i);
    auto newOperand = stackFrame.translateValue(*oldInstruction, operand);
    newInstruction->setOperand(i, newOperand);
  }
}

// Updates the successor basic blocks of the given instruction to its
// newly created corresponding basic block from the builder
void translateInsnSuccessors(StackFrame &stackFrame,
                             llvm::Instruction *instruction) {
  for (unsigned i = 0; i < instruction->getNumSuccessors(); i++) {
    auto successorBB = instruction->getSuccessor(i);
    auto newSuccessorBB = stackFrame.translateBBValue(successorBB);
    instruction->setSuccessor(i, newSuccessorBB);
  }
}
