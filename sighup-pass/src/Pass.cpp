// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>

#include <iostream>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <unordered_set>

using namespace llvm;
using namespace std;

/**
 */
struct LmcasSighupPass : public ModulePass {
  static char ID;
  LmcasSighupPass() : ModulePass(ID) {}

  bool runOnModule(Module &mod) override {
    LLVMContext &context = mod.getContext();
    IRBuilder<> builder(context);

    std::unordered_set<std::string> names({"sigaction", "signal"});

    for (auto &function : mod) {
      for (auto &basicBlock : function) {
        for (BasicBlock::iterator insnIter = basicBlock.begin();
             insnIter != basicBlock.end(); ++insnIter) {
          // get instruction
          auto &insn = *insnIter;

          // check for call instruction
          auto callInst = dyn_cast<llvm::CallInst>(&insn);
          if (!callInst) {
            continue;
          }

          // and identifiable called function
          auto calledFunction = callInst->getCalledFunction();
          if (!calledFunction) {
            continue;
          }

          // then check if it is one of the functions of interest
          auto name = calledFunction->getName();
          if (names.count(name.str()) < 1) {
            continue;
          }

          // grab the first argument of the call
          auto *arg = callInst->getArgOperand(0);

          // Attempt to cast to a ConstantInt
          auto *cint = dyn_cast<llvm::ConstantInt>(arg);

          // Move on if it is not of the handled types
          if (!cint) {
            spdlog::warn("{} found where first arg is of unhandled type:",
                         name);
            std::string Str;
            raw_string_ostream OS(Str);
            arg->print(OS, true);
            OS << "\n";
            OS << basicBlock;
            spdlog::warn(OS.str());
            continue;
          }

          // check that it is a SIGHUP
          if (cint->getZExtValue() != 1) {
            continue;
          }

          spdlog::info("Detected sigaction using SIGHUP");

          // Attempt to replace the SIGHUP
          try {
            auto *new_cint = builder.getInt32(0);
            ReplaceInstWithValue(basicBlock.getInstList(), insnIter, new_cint);
            spdlog::info("Replaced SIGHUP");
          } catch (const exception e) {
            spdlog::error("Failed to handle SIGHUP: {}", e.what());
            throw e;
          }
        }
      }
    }
    return true;
  }
};

char LmcasSighupPass::ID = 0;
static RegisterPass<LmcasSighupPass> X("handle-sighup", "Lmcas Sighup pass");
