// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <vector>

#include "FindNeck.h"
#include "FmtLlvm.h"
#include "InsertMainArgs.h"
#include "MakeDispatchMain.h"
#include "MakeNeckContinuation.h"
#include "Spec.h"
#include "Tape.h"
#include "UnrollTape.h"

llvm::cl::opt<std::string>
    specOpt("specialization-spec",
            llvm::cl::desc("The path to the file containing the TaBaCCo "
                           "debloating specification"));

llvm::cl::list<std::string> safeExternalFunctionRegexesOpt(
    "tabacco-safe-external-function-regex",
    llvm::cl::desc(
        "If a function matches this regex, it will be assumed to contain no "
        "\"significant\" control flow and directly called"));

struct LmcasSpecializationPass : public llvm::ModulePass {
  static char ID;
  LmcasSpecializationPass() : ModulePass(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    // Required to retrieve dominator tree from anaylsis
    AU.addRequired<llvm::DominatorTreeWrapperPass>();
  }

  bool runOnModule(llvm::Module &mod) override {
    spdlog::cfg::load_env_levels();

    // Collect the list of safe external functions.
    std::vector<std::regex> safeExternalFunctionRegexes;
    std::transform(safeExternalFunctionRegexesOpt.begin(),
                   safeExternalFunctionRegexesOpt.end(),
                   std::back_inserter(safeExternalFunctionRegexes),
                   [](const auto &s) { return std::regex(s); });

    // Load the specialization specification.
    auto spec = spec::load_spec_from_file(specOpt.getValue());

    // Find the main function in the module.
    auto mainFunction = mod.getFunction("main");
    if (!mainFunction) {
      spdlog::error("Module had no main function!");
      throw std::runtime_error{"Module had no main function!"};
    }

    // Find the neck; if there wasn't one, bail out.
    auto neckCallMarkers = tabacco::findNeckMarkers(mod, "_lmcas_neck");
    if (neckCallMarkers.empty()) {
      throw std::runtime_error{"failed to find a neck marker"};
    }

    // Create a tape unrolling for each of the tapes.
    std::vector<llvm::Function *> tapeUnrollings;
    for (size_t i = 0; i < spec.configs.size(); i++) {
      auto const &tape = spec.configs[i].tape;

      // Make a new main function, and construct an IRBuilder at its beginning.
      auto newMainType = llvm::FunctionType::get(
          llvm::IntegerType::get(mod.getContext(), 32),
          {llvm::IntegerType::get(mod.getContext(), 32),
           llvm::PointerType::get(
               llvm::PointerType::get(
                   llvm::IntegerType::get(mod.getContext(), 8), 0),
               0)},
          false);
      auto newMain = llvm::Function::Create(
          newMainType, llvm::GlobalValue::LinkageTypes::ExternalLinkage, "",
          mod);
      newMain->setName(fmt::format("_tabacco_{}_main", i));
      tapeUnrollings.push_back(newMain);
      llvm::IRBuilder<> builder(
          llvm::BasicBlock::Create(mod.getContext(), "", newMain));

      // Insert the arguments that were present when the instrumented binary was
      // run.
      llvm::Argument *mainArgc = newMain->getArg(0);
      llvm::Argument *mainArgv = newMain->getArg(1);
      auto [argc, argv] =
          insertMainArgs(mod, mainArgc, mainArgv, spec.configs[i].args,
                         spec.configs[i].env, builder);

      // We use a separate IRBuilder for allocas, so that we can insert them at
      // the start of the function. This is necessary to get reasonable machine
      // code out of LLVM.
      llvm::IRBuilder<> allocaBuilder(&newMain->getEntryBlock().front());

      // Unroll the tape; i.e., lay out each recorded basic block in the tape.
      auto stack = unrollTape(tape, safeExternalFunctionRegexes, argc, argv,
                              *mainFunction, *newMain, neckCallMarkers,
                              allocaBuilder, builder);

      // Insert a call to the _tabacco_at_neck function, which unmaps the
      // pre-neck code.
      builder.CreateCall(mod.getFunction("_tabacco_at_neck"));

      // Iterate over all encountered stack frames before the neck is reached
      while (!stack.empty()) {
        auto stackFrame = stack.back();

        // Move the function back to the text section; if it's still on the
        // stack at the neck, we don't want to unmap it.
        stackFrame.newFunction.setSection(".text");

        // Get dominator tree for the current stack frame function
        auto &dominatorTree =
            getAnalysis<llvm::DominatorTreeWrapperPass>(stackFrame.function)
                .getDomTree();

        // Copy all instruction after the neck triggering call in the current
        // stack frame. First frame popped off the stack will be the function
        // that called the neck itself.
        copyFuncInstructionsAfterNeck(stackFrame, dominatorTree, builder);

        // Set IR builder insertion point to the point at which we continue
        // at when the function returns
        if (stackFrame.insertionPointsOnReturn.has_value()) {
          auto insertionPointOnReturn =
              stackFrame.insertionPointsOnReturn.value();
          builder.SetInsertPoint(std::get<2>(insertionPointOnReturn),
                                 std::get<3>(insertionPointOnReturn));
        }
        stack.pop_back();
      }
    }

    // Rename the original main function, so that we don't get a conflict
    // between it and the new one we'll be making.
    //
    // TODO: Should we check instead of it gets dead-code-eliminated if we
    // internalize it?
    mainFunction->setName("_tabacco_old_main");

    makeDispatchMain(mod, "main", spec.configs, tapeUnrollings);

    return true;
  }
};

char LmcasSpecializationPass::ID = 0;
static llvm::RegisterPass<LmcasSpecializationPass>
    X("tabacco-specialize", "TaBaCCo Specialization Pass");
