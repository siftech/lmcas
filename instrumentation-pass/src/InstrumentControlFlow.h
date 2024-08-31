// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__INSTRUMENT_CONTROL_FLOW_H
#define TABACCO__INSTRUMENT_CONTROL_FLOW_H

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>

namespace lmcas_instrumentation_pass {

/**
 * Inserts a call to lmcas_instrumentation_setup at the start of the given
 * function.
 */
llvm::CallInst *insertSetupCall(llvm::Function *mainFunction);

/**
 * Inserts a call to lmcas_instrumentation_done immediately before the given
 * instruction.
 */
llvm::CallInst *insertDoneCall(llvm::Instruction *neckCall);

/**
 * Inserts a call to lmcas_instrumentation_call_start immediately before the
 * given instruction, and a call to lmcas_instrumentation_call_end immediately
 * after.
 */
void insertCallInfoCalls(llvm::CallInst *callInsn);

/**
 * Inserts a call to lmcas_instrumentation_bb_start immediately before the
 * given instruction.
 */
llvm::CallInst *insertBBStartCall(llvm::Instruction *beforeInsn,
                                  uint64_t basicBlockID);

/**
 * Inserts a call to the appropriate instrumentation function immediately
 * before the given instruction, which must be a terminator.
 */
llvm::CallInst *insertTerminatorCall(llvm::Instruction *terminator);

} // namespace lmcas_instrumentation_pass

#endif
