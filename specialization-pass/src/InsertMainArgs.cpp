// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "InsertMainArgs.h"

#include <spdlog/spdlog.h>

std::tuple<llvm::Value *, llvm::Value *>
insertMainArgs(llvm::Module &mod, llvm::Argument *argc, llvm::Argument *argv,
               const std::vector<std::string> &specArgs,
               const absl::flat_hash_map<std::string, std::string> &specEnv,
               llvm::IRBuilder<> &builder) {

  // An llvm char* type
  llvm::Type *llvmCharPtrType =
      llvm::PointerType::get(llvm::IntegerType::get(argc->getContext(), 8), 0);

  auto argv0 = builder.CreateGlobalStringPtr(llvm::StringRef(specArgs.at(0)));
  auto argv0Ptr = builder.CreateGEP(llvmCharPtrType, argv, builder.getInt32(0));
  builder.CreateStore(argv0, argv0Ptr);

  // Store each of the environment variables. This is kinda broken, since
  // putenv() almost certainly allocates...
  if (llvm::Function *putenv = mod.getFunction("putenv")) {
    for (const auto &[key, value] : specEnv) {

      builder.CreateCall(putenv, {
                                     builder.CreateGlobalStringPtr(
                                         fmt::format("{}={}", key, value)),
                                 });
    }
  } else {
    throw std::runtime_error{"Could not find putenv"};
  }

  return {argc, argv};
}
