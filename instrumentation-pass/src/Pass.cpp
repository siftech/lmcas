// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "Annotation.h"
#include "FindNeck.h"
#include "FmtLlvm.h"
#include "FunctionPtrs.h"
#include "InstrumentControlFlow.h"
#include "InstrumentLibc.h"
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

using namespace lmcas_instrumentation_pass;

static llvm::cl::opt<bool> instrumentLibc(
    "instrument-libc",
    llvm::cl::desc("Performs libc-specific instrumentation tasks"));

struct LmcasInstrumentationPass : public llvm::ModulePass {
  static char ID;
  LmcasInstrumentationPass() : ModulePass(ID) {}

  bool runOnModule(llvm::Module &mod) override {
    spdlog::cfg::load_env_levels();

    // Insert a call to lmcas_instrumentation_done at the neck, if it existed;
    // otherwise, yell about it. This needs to be done before anything else,
    // because we don't want to mess with the index of an instruction in the
    // basic block when we insert more instructions into its basic block.
    auto neckMarkers = findNeckMarkers(mod, "_lmcas_neck");
    if (neckMarkers.empty())
      spdlog::error("Could not find a unique neck, so not instrumenting it");
    for (auto neckMarker : neckMarkers)
      insertDoneCall(neckMarker);

    // Iterate over each basic block in the module; for each one, add the
    // instrumentation at the start of the basic block, at its terminator, and
    // at any call instructions in between.
    std::vector<llvm::CallInst *> callsToInstrument;
    for (auto &function : mod) {
      for (auto &basicBlock : function) {
        if (auto basicBlockID = getBasicBlockID(basicBlock)) {
          // The ordering here is important to not accidentally insert
          // lmcas_instrumentation_call_start calls for the other
          // lmcas_instrumentation_* functions.
          for (auto &instruction : basicBlock)
            if (auto callInstruction =
                    llvm::dyn_cast<llvm::CallInst>(&instruction))
              callsToInstrument.push_back(callInstruction);

          insertBBStartCall(&*basicBlock.getFirstInsertionPt(), *basicBlockID);
          insertTerminatorCall(basicBlock.getTerminator());
        } else {
          spdlog::error("Found a basic block without an ID in {} (was the "
                        "annotation pass run correctly?): {}",
                        basicBlock.getParent()->getName(), basicBlock);
        }
      }
    }
    for (auto callInstruction : callsToInstrument)
      insertCallInfoCalls(callInstruction);

    // If we're running on libc, first perform some fixups specific to it.
    if (instrumentLibc) {
      if (!libcReplaceDunderSyscall(mod))
        return false;
    } else {
      // Otherwise, we should be running on a binary, so add the call to
      // lmcas_instrumentation_setup to the start of main, if we have a main
      // function. If not, yell about it.
      //
      // (We don't need to care about libraries other than libc, since we
      // already should have combined their bitcode into a single bitcode file
      // that represents the entire application.)
      if (auto main = mod.getFunction("main")) {
        insertSetupCall(main);
      } else {
        spdlog::error(
            "Could not find a main function, so not instrumenting it");
      }
    }

    // Make the table of function pointers for the parent to use.
    makeFunctionPointersTable(mod);

    return true;
  }
};

char LmcasInstrumentationPass::ID = 0;
static llvm::RegisterPass<LmcasInstrumentationPass> X("instrument",
                                                      "Lmcas compiler pass");
