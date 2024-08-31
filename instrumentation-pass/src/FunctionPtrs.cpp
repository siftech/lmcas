// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "FunctionPtrs.h"
#include "Annotation.h"
#include "FmtLlvm.h"
#include <absl/container/flat_hash_map.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <spdlog/spdlog.h>

namespace lmcas_instrumentation_pass {

void makeFunctionPointersTable(llvm::Module &mod) {
  // Find all the functions to store in the table.
  std::vector<llvm::Function *> functions;
  for (auto &func : mod)
    functions.push_back(&func);
  spdlog::info("makeFunctionPointersTable: Found {} function pointees",
               functions.size());

  // Make the type of the elements in the table.
  auto u64 = llvm::IntegerType::get(mod.getContext(), 64);
  auto entry = llvm::StructType::get(u64, u64);
  auto arrayType = llvm::ArrayType::get(entry, functions.size());

  // Create the array values as a constant.
  std::vector<llvm::Constant *> entries;
  for (auto func : functions) {
    if (func->begin() == func->end()) {
      spdlog::warn("Function {} had no entry block", *func);
      continue;
    }

    if (auto basicBlockId = getBasicBlockID(func->getEntryBlock())) {
      auto funcAddr = llvm::ConstantExpr::getPtrToInt(
          llvm::cast<llvm::Constant>(func), u64);
      auto id = llvm::ConstantInt::get(u64, *basicBlockId);
      entries.push_back(llvm::ConstantStruct::get(entry, funcAddr, id));
    } else {
      spdlog::warn("Function {}'s entry block had no ID", *func);
    }
  }
  auto arrayInitializer = llvm::ConstantArray::get(arrayType, entries);

  // Define the global variable with appending linkage, so the various arrays
  // will get combined.
  auto globalVariable = new llvm::GlobalVariable(
      mod, arrayType, true,
      llvm::GlobalVariable::LinkageTypes::LinkOnceODRLinkage, arrayInitializer);
  globalVariable->setSection("lmcas_function_pointer_table");
}

} // namespace lmcas_instrumentation_pass
