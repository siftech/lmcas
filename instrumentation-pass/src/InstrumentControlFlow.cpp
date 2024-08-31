// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "InstrumentControlFlow.h"

#include "FmtLlvm.h"
#include <llvm/IR/InstVisitor.h>
#include <spdlog/spdlog.h>

namespace lmcas_instrumentation_pass {

/**
 * Returns the function if it exists in the module, or creates it (with external
 * linkage) if not.
 *
 * This is used to get functions that are defined in the runtime.
 */
static llvm::Function *getOrCreateFunction(llvm::Module *mod,
                                           llvm::StringRef name,
                                           llvm::FunctionType *functionType) {
  if (auto existingFunction = mod->getFunction(name)) {
    if (existingFunction->getFunctionType() != functionType) {
      spdlog::error(
          "Function {} existed with type {}, expected type {}", name,
          *llvm::cast<llvm::Type>(existingFunction->getFunctionType()),
          *llvm::cast<llvm::Type>(functionType));
    }
    return existingFunction;
  } else {
    return llvm::Function::Create(
        functionType, llvm::GlobalValue::ExternalLinkage, name, mod);
  }
}

/**
 * Creates a call to the function with the given name, with the given arguments.
 * This requires that the function returns void, and is not a varargs function.
 */
static llvm::CallInst *createCall(llvm::Module *mod, llvm::StringRef name,
                                  llvm::ArrayRef<llvm::Value *> args) {
  // Collect the argument types.
  std::vector<llvm::Type *> argTypes;
  std::transform(args.begin(), args.end(), std::back_inserter(argTypes),
                 [](auto value) { return value->getType(); });

  // Create the expected function type.
  auto functionType = llvm::FunctionType::get(
      llvm::Type::getVoidTy(mod->getContext()), argTypes, false);

  // Get the function to call.
  auto function = getOrCreateFunction(mod, name, functionType);

  // Create and return the call instruction.
  return llvm::CallInst::Create(function, args);
}

llvm::CallInst *insertSetupCall(llvm::Function *mainFunction) {
  // Create a call to the lmcas_instrumentation_setup function.
  auto mod = mainFunction->getParent();
  auto call = createCall(mod, "lmcas_instrumentation_setup", {});

  // Insert the call at the start of mainFunction.
  call->insertBefore(&*mainFunction->getEntryBlock().getFirstInsertionPt());
  return call;
}

llvm::CallInst *insertDoneCall(llvm::Instruction *neckCall) {
  auto mod = neckCall->getModule();
  auto call = createCall(mod, "lmcas_instrumentation_done", {});
  call->insertBefore(neckCall);
  return call;
}

llvm::CallInst *insertBBStartCall(llvm::Instruction *beforeInsn,
                                  uint64_t basicBlockID) {
  // Create a constant for the basic block ID.
  auto &ctx = beforeInsn->getContext();
  auto basicBlockIDConstant =
      llvm::ConstantInt::get(llvm::IntegerType::get(ctx, 64), basicBlockID);

  // Create the call and insert it.
  auto mod = beforeInsn->getModule();
  auto call =
      createCall(mod, "lmcas_instrumentation_bb_start", {basicBlockIDConstant});
  call->insertBefore(beforeInsn);
  return call;
}

void insertCallInfoCalls(llvm::CallInst *callInsn) {
  // Don't instrument intrinsics or inline assembly.
  if (callInsn->isInlineAsm() || llvm::isa<llvm::IntrinsicInst>(callInsn))
    return;

  auto mod = callInsn->getModule();
  createCall(mod, "lmcas_instrumentation_call_start", {})
      ->insertBefore(callInsn);
  createCall(mod, "lmcas_instrumentation_call_end", {})->insertAfter(callInsn);
}

struct InsertTerminatorVisitor
    : public llvm::InstVisitor<InsertTerminatorVisitor, llvm::CallInst *> {
  llvm::LLVMContext &ctx;
  llvm::Module *mod;

  // In the default case of an instruction, warn that insertTerminatorCall was
  // called incorrectly.
  llvm::CallInst *visitInstruction(llvm::Instruction &instruction) {
    spdlog::error("BUG: lmcas_instrumentation_pass::insertTerminatorCall was "
                  "called on an instruction that was not a terminator");
    throw std::logic_error{"BUG"};
  }

  // If there's a terminator we don't yet handle, we error out.
  llvm::CallInst *visitTerminator(llvm::Instruction &terminator) {
    spdlog::error("TODO: Terminator not yet handled: {}", terminator);
    throw std::runtime_error{"TODO"};
  }

  // On a conditional branch, we call lmcas_instrumentation_record_cond_br. If
  // the branch is unconditional, we don't do anything.
  llvm::CallInst *visitBranchInst(llvm::BranchInst &instruction) {
    if (instruction.isConditional()) {
      auto conditionAsI8 = llvm::CastInst::CreateZExtOrBitCast(
          instruction.getCondition(), llvm::IntegerType::get(ctx, 8));
      conditionAsI8->insertBefore(&instruction);

      auto call = createCall(mod, "lmcas_instrumentation_record_cond_br",
                             {conditionAsI8});
      call->insertBefore(&instruction);
      return call;
    } else {
      return nullptr;
    }
  }

  // The rest of the terminators we have lmcas_instrumentation_record_*
  // functions for translate pretty straightforwardly.

  llvm::CallInst *visitReturnInst(llvm::ReturnInst &instruction) {
    auto call = createCall(mod, "lmcas_instrumentation_record_ret", {});
    call->insertBefore(&instruction);
    return call;
  }

  llvm::CallInst *visitSwitchInst(llvm::SwitchInst &instruction) {
    auto conditionAsI64 = llvm::CastInst::CreateZExtOrBitCast(
        instruction.getCondition(), llvm::IntegerType::get(ctx, 64));
    conditionAsI64->insertBefore(&instruction);

    auto call = createCall(mod, "lmcas_instrumentation_record_switch",
                           {conditionAsI64});
    call->insertBefore(&instruction);
    return call;
  }

  llvm::CallInst *visitIndirectBrInst(llvm::IndirectBrInst &instruction) {
    auto call = createCall(mod, "lmcas_instrumentation_record_indirectbr",
                           {instruction.getAddress()});
    call->insertBefore(&instruction);
    return call;
  }

  llvm::CallInst *visitUnreachableInst(llvm::UnreachableInst &instruction) {
    auto call = createCall(mod, "lmcas_instrumentation_record_unreachable", {});
    call->insertBefore(&instruction);
    return call;
  }
};

llvm::CallInst *insertTerminatorCall(llvm::Instruction *terminator) {
  // Use the visitor to choose which function to call.
  InsertTerminatorVisitor visitor{
      .ctx = terminator->getContext(),
      .mod = terminator->getModule(),
  };
  return visitor.visit(terminator);
}

} // namespace lmcas_instrumentation_pass
