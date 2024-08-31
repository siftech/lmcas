// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>

#include <iostream>
#include <spdlog/spdlog.h>

using namespace llvm;
using namespace std;

cl::opt<int> IdOffset(cl::Optional, "id-offset",
                      cl::desc("Offset for all IDs annotated by the pass"));

/**
 */
struct LmcasAnnotationPass : public ModulePass {
  static char ID;
  LmcasAnnotationPass() : ModulePass(ID) {}

  bool runOnModule(Module &mod) override {
    LLVMContext &context = mod.getContext();

    // 0 by default if it's not passed
    uint64_t id = IdOffset.getValue();

    for (auto &func : mod) {
      // It's kind of a hack to put this here, but... it needs to run on all our
      // bitcode.
      func.addFnAttr(llvm::Attribute::NoBuiltin);

      for (auto &bb : func) {
        auto terminatorInst = bb.getTerminator();

        auto idStr = fmt::format("{}", id);

        terminatorInst->setMetadata(
            "LmcasBasicBlockID",
            MDNode::get(context, MDString::get(context, idStr)));

        id += 1;
      }
    }

    return true;
  }
};

char LmcasAnnotationPass::ID = 0;
static RegisterPass<LmcasAnnotationPass> X("annotate", "Lmcas annotation pass");
