// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef LMCAS_INSTRUMENTATION_RUNTIME_H
#define LMCAS_INSTRUMENTATION_RUNTIME_H 1

// This header file defines the interface between the output of the
// instrumentation pass and the ptrace-watcher runtime. Note that since the
// instrumentation pass works at the level of LLVM IR, not C, this file is
// *not* used directly by it. However, it serves as good documentation, and is
// checked (at least at a function signature level) when building the runtime.

#include <inttypes.h>
#include <stdnoreturn.h>

// At the start of instrumented execution (i.e. the start of the main function,
// for now), this function is called to set up communication with the
// ptrace-watcher.
void lmcas_instrumentation_setup(void);

// At the end of instrumented execution (i.e. the neck), this function is
// called to finish communication with the ptrace-watcher.
noreturn void lmcas_instrumentation_done(void);

// At the start of each basic block, this function is called to record the
// change in sequence number. The arguments is the basic block ID number
// assigned during the annotation pass.
void lmcas_instrumentation_bb_start(uint64_t);

// Just before each call, this function is called. This makes it easier to
// infer the call structure of the tape without performing analysis.
void lmcas_instrumentation_call_start(void);

// At each ret terminator, this function is called to record it.
//
// TODO: This needs some thinking, particularly around inlining. It might be
// nice to split basic blocks such that call instructions are the only
// instruction in them? Maybe I'm overthinking it, though.
void lmcas_instrumentation_record_ret(void);

// At each conditional br terminator, this function is called to record the
// direction the branch is going. The i1 used in the br should be passed here
// as an i8.
void lmcas_instrumentation_record_cond_br(uint8_t);

// There's no record function for an unconditional br, since we don't need to
// record that.

// At each switch terminator, this function is called to record the destination.
// The value being switched on should be passed here as an i64.
void lmcas_instrumentation_record_switch(uint64_t);

// At each indirectbr terminator, this function is called to record the
// destination. The address being branched to should be passed here as the
// target's pointer size.
void lmcas_instrumentation_record_indirectbr(uintptr_t);

// At the unreachable terminator, this function is called to record it. This,
// of course, shouldn't be called either.
void lmcas_instrumentation_record_unreachable(void);

// TODO: callbr? "This instruction should only be used to implement the goto
//       feature of gcc style inline assembly."
// TODO: Exception-related terminators? invoke, resume, catchswitch, catchret,
//       cleanupret

#endif // LMCAS_INSTRUMENTATION_RUNTIME_H
