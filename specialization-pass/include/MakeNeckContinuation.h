// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__MAKE_NECK_CONTINUATION_H
#define TABACCO__MAKE_NECK_CONTINUATION_H

#include "StackFrame.h"
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>

/*
 * Copies all instructions after the neck call for the given function
 * on the stack frame. In the specialization pass, we iterate over all
 * stack frames encountered before the neck call instruction; for every
 * given stack frame and its original associated function, we copy all
 * remaining instructions in the function into the builder.
 *
 * This is done by first creating new basic blocks from the old ones in the
 * function. A dominator tree is used to create these in the correct order --
 * this is, parents are created and ultimately copied first, before their
 * children, since they may reference these parent basic blocks. We have found
 * that there may be additional reachable basic blocks that are not in the
 * dominator tree (e.g. basic blocks that just handle the return from the
 * function), so these are also copied after those in the dominator tree.
 *
 * Once the new basic blocks are created, we copy the instructions from the
 * old basic blocks into the new. The reason the basic blocks are created
 * before we copy the instructions is because instructions may reference other
 * basic blocks, and they must reference those in the builder and not those
 * associated with the old function.
 *
 * TO NOTE:
 * A stack frame's current instruction pointer when passed here is at the
 * relevant call instruction that ultimately triggered the neck call.
 * The dominator tree is the DT created from a stack frame's old function.
 */
void copyFuncInstructionsAfterNeck(StackFrame &stackFrame,
                                   llvm::DominatorTree &DT,
                                   llvm::IRBuilder<> &builder);

#endif
