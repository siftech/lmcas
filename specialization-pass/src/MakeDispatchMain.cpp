// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "MakeDispatchMain.h"
#include <llvm/IR/IRBuilder.h>

llvm::Function *
makeDispatchMain(llvm::Module &mod, llvm::Twine const &name,
                 std::vector<spec::OptionConfig> const &optionConfigs,
                 std::vector<llvm::Function *> const &tapeUnrollings) {
  assert(optionConfigs.size() == tapeUnrollings.size());

  // Find the strcmp function.
  llvm::FunctionCallee strcmp{mod.getFunction("strcmp")};

  // Make the main function and a builder that's positioned at its entry basic
  // block.
  auto mainFunctionType = llvm::FunctionType::get(
      llvm::IntegerType::get(mod.getContext(), 32),
      {llvm::IntegerType::get(mod.getContext(), 32),
       llvm::PointerType::get(
           llvm::PointerType::get(llvm::IntegerType::get(mod.getContext(), 8),
                                  0),
           0)},
      false);
  auto mainFunction = llvm::Function::Create(
      mainFunctionType, llvm::GlobalValue::LinkageTypes::ExternalLinkage, name,
      mod);

  // Get the argc and argv values.
  auto argc = mainFunction->getArg(0), argv = mainFunction->getArg(1);

  // The structure we'll be making is essentially:
  //
  //     int main(int argc, char **argv) {
  //         if(argc >= 4) {
  //             if( (strcmp(argv[1], "--example") == 0)
  //               & (strcmp(argv[2], "--arg") == 0)
  //               & (strcmp(argv[3], "1") == 0)
  //               ) {
  //                 return _tabacco_main_1(argc, argv);
  //             }
  //         }
  //         if(argc >= 4) {
  //             if( (strcmp(argv[1], "--example") == 0)
  //               & (strcmp(argv[2], "--arg") == 0)
  //               & (strcmp(argv[3], "2") == 0)
  //               ) {
  //                 return _tabacco_main_2(argc, argv);
  //             }
  //         }
  //         if(argc >= 2) {
  //             if((strcmp(argv[1], "--other") == 0)) {
  //                 return _tabacco_main_3(argc, argv);
  //             }
  //         }
  //         fprintf(stderr, ...);
  //         return 100;
  //     }
  //
  // Note the use of & instead of && -- this greatly simplifies the
  // implementation below, and probably doesn't have too bad of an effect on
  // codegen.

  // To do this, we make three basic blocks for each config, plus one final one.
  // The per-config blocks are:
  //
  // - The basic block that checks if argc is large enough
  // - The basic block that checks if the argument strings match
  // - The basic block that calls the unrolled tape
  //
  // If either check fails, we continue to the next argcTestBlock, or, on the
  // final iteration, the usageError block.
  //
  // The first-created basic block for the function becomes the entry one; this
  // is either the argcTest_0 block, or the usageError block if there are no
  // configs.
  std::vector<llvm::BasicBlock *> argcTestBlocks, argvTestBlocks, callBlocks;
  for (size_t i = 0; i < optionConfigs.size(); i++) {
    argcTestBlocks.push_back(llvm::BasicBlock::Create(
        mod.getContext(), fmt::format("argcTest_{}", i), mainFunction));
    argvTestBlocks.push_back(llvm::BasicBlock::Create(
        mod.getContext(), fmt::format("argvTest_{}", i), mainFunction));
    callBlocks.push_back(llvm::BasicBlock::Create(
        mod.getContext(), fmt::format("call_{}", i), mainFunction));
  }

  // Create the usageErrorBlock.
  auto usageErrorBlock =
      llvm::BasicBlock::Create(mod.getContext(), "usageError", mainFunction);

  // We create a builder; for now, it's positioned at the usageError block, but
  // it'll get repositioned for each of the configs as we build them up.
  llvm::IRBuilder<> builder{mod.getContext()};

  for (size_t i = 0; i < optionConfigs.size(); i++) {
    // Compute the block to continue to if the argc or argv tests fail. This is
    // the next argc block except on the final iteration; for the final
    // iteration, it's the usageErrorBlock.
    auto nextBlock = (i + 1 == optionConfigs.size()) ? usageErrorBlock
                                                     : argcTestBlocks[i + 1];

    // Get the arguments to check against.
    auto const &args = optionConfigs[i].args;

    // Create the argc test. Since we've got the unwanted args[0] in
    // args.size(), the test is for whether argc >= args.size(). Consider:
    //
    //     argc = 2
    //     argv = {"./ls", "-l", NULL}
    //     args = {"ls", "-l"}
    builder.SetInsertPoint(argcTestBlocks[i]);
    builder.CreateCondBr(
        builder.CreateICmpUGE(argc, builder.getInt32(args.size())),
        argvTestBlocks[i], nextBlock);

    // Create the argv test.
    builder.SetInsertPoint(argvTestBlocks[i]);
    llvm::Value *allArgsMatched = builder.getInt1(true);
    for (size_t j = 1; j < args.size(); j++) {
      auto arg = builder.CreateGlobalStringPtr(args[j]);
      auto argv_j = builder.CreateLoad(
          builder.CreateGEP(builder.getInt8PtrTy(), argv, builder.getInt32(j)));
      auto strcmpRet = builder.CreateCall(strcmp, {arg, argv_j});
      allArgsMatched = builder.CreateAnd(
          allArgsMatched, builder.CreateICmpEQ(strcmpRet, builder.getInt32(0)));
    }
    builder.CreateCondBr(allArgsMatched, callBlocks[i], nextBlock);

    // Create the call to the unrolled tape.
    builder.SetInsertPoint(callBlocks[i]);
    auto mainRet =
        builder.CreateCall(mainFunctionType, tapeUnrollings[i], {argc, argv});
    builder.CreateRet(mainRet);

    // Make the call to main a tail call if possible.
    mainRet->setTailCall(true);
  }

  // Fill out the usageError block.
  builder.SetInsertPoint(usageErrorBlock);
  std::string usageMessage;
  bool firstConfig = true;
  for (auto const &config : optionConfigs) {
    if (firstConfig) {
      firstConfig = false;
      usageMessage += "USAGE:";
    } else {
      usageMessage += "      ";
    }
    for (auto const &arg : config.args) {
      usageMessage += " ";
      usageMessage += arg;
    }
    usageMessage += " [ARGS...]\n";
  }
  auto stderr = mod.getGlobalVariable("stderr");
  builder.CreateCall(llvm::FunctionCallee{mod.getFunction("fprintf")},
                     {
                         builder.CreateLoad(stderr->getValueType(), stderr),
                         builder.CreateGlobalStringPtr(usageMessage),
                     });
  builder.CreateRet(builder.getInt32(100));

  return mainFunction;
}
