// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__UPDATE_INSTRUCTION_UTILS_H
#define TABACCO__UPDATE_INSTRUCTION_UTILS_H

#include "StackFrame.h"
#include <llvm/IR/Instruction.h>

void translateInsnOperands(StackFrame &stackFrame,
                           llvm::Instruction *newInstruction,
                           llvm::Instruction *oldInstruction);
void translateInsnSuccessors(StackFrame &stackFrame,
                             llvm::Instruction *instruction);

#endif
