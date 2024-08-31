/******************************************************************************
 * Copyright (c) 2021 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/IFDSIDESolverConfig.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IDEExtendedTaintAnalysis.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IFDSTaintAnalysis.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/SolverResults.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToSet.h"
#include "phasar/PhasarLLVM/TaintConfig/TaintConfig.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"
#include "phasar/Utils/Logger.h"

#include "NeckID/NeckID/TaintAnalysis.h"

#include "FmtLlvm.h"
#include <spdlog/spdlog.h>

namespace neckid {

TaintAnalysis::TaintAnalysis(llvm::Module &M,
                             const std::string &TaintConfigPath,
                             bool FunctionLocalPTAwoGlobals,
                             bool UseSimplifiedDFA, bool Debug)
    : IR([&]() {
        psr::initializeLogger(false);
        spdlog::debug("Built project IR database ...");
        return psr::ProjectIRDB(std::vector<llvm::Module *>({&M}),
                                psr::IRDBOptions::WPA);
      }()),
      Config([&]() {
        try {
          return psr::TaintConfig(IR, psr::parseTaintConfig(TaintConfigPath));
        } catch (std::ios_base::failure &IOFailure) {
          spdlog::error("Could not parse taint configuration '{}"
                        "'!\nContinuing by trying to parse the config from "
                        "the (hopefully) annotated LLVM IR.",
                        TaintConfigPath);
          return psr::TaintConfig(IR);
        }
      }()),
      T([&]() {
        spdlog::debug("Built type hierarchy ...");
        return psr::LLVMTypeHierarchy(IR);
      }()),
      P([&]() {
        spdlog::debug("Built points-to sets ...");
        return psr::LLVMPointsToSet(IR, true /* lazy eval on */,
                                    psr::PointerAnalysisType::CFLAnders,
                                    FunctionLocalPTAwoGlobals);
      }()),
      I([&]() {
        spdlog::debug("Built inter-procedural control-flow graph ...");
        return psr::LLVMBasedICFG(IR, psr::CallGraphAnalysisType::CHA, {"main"},
                                  &T, &P);
      }()) {
  // Print the taint configuration
  std::stringstream Ss;
  Ss << Config;
  spdlog::debug(Ss.str());
  // Set up analysis and solver
  spdlog::debug("Setting up data-flow analysis ...");
  psr::IFDSIDESolverConfig SolverConfig(
      psr::SolverConfigOptions::ComputeValues |
      psr::SolverConfigOptions::FollowReturnsPastSeeds);
  if (!UseSimplifiedDFA) {
    psr::IDEExtendedTaintAnalysis<1, false> TaintAnalysis(&IR, &T, &I, &P,
                                                          Config);
    TaintAnalysis.setIFDSIDESolverConfig(SolverConfig);
    std::stringstream SolverConfigStr;
    SolverConfigStr << "Using solver config: "
                    << TaintAnalysis.getIFDSIDESolverConfig() << '\n';
    spdlog::debug(SolverConfigStr.str());
    psr::IDESolver Solver(TaintAnalysis);
    spdlog::debug("Solving data-flow analysis ...");
    Solver.solve();
    spdlog::debug("Data-flow analysis has been solved.");
    if (Debug) {
      spdlog::debug("Raw data-flow results:");
      std::stringstream RawSolverOut;
      Solver.dumpResults(RawSolverOut);
      spdlog::debug(RawSolverOut.str());
    }
    // Retrieve all usages of data that is depending on the initial seeds. In
    // case of command-line tools, these are data-flow facts that are
    // transitively reachable from the argc and argv parameters of the main
    // function.
    auto SolverRes = Solver.getSolverResults();
    auto AllResEntries = SolverRes.getAllResultEntries();
    // Container to store potential neck candidates that have been identified by
    // the taint analysis.
    // Iterate all instructions and check if any of those
    // instructions uses a tainted value. These tainted instruction operands are
    // neck candidates.
    for (auto &Res : AllResEntries) {
      const llvm::Instruction *Inst = Res.getRowKey();
      auto ResAtInst = SolverRes.resultsAt(Inst);
      // Iterate all operands of an instruction an check if one of them is
      // tainted. If so, this instruction is a potential neck candidate.
      for ([[maybe_unused]] const auto &Op : Inst->operands()) {
        for (auto &[Fact, Value] : ResAtInst) {
          llvm::Value *PotentialGepPointerOp = nullptr;
          if (auto *Gep = llvm::dyn_cast<llvm::GetElementPtrInst>(Op)) {
            PotentialGepPointerOp = Gep->getPointerOperand();
          }
          if (Op == Fact->base() || (PotentialGepPointerOp &&
                                     PotentialGepPointerOp == Fact->base())) {
            NeckCandidates.push_back(
                const_cast<llvm::Instruction *>(Inst)); // NOLINT ;-)
          }
        }
      }
    }
  } else {
    // TODO avoid redundancy
    psr::IFDSTaintAnalysis TaintAnalysis(&IR, &T, &I, &P, Config, {});
    TaintAnalysis.setIFDSIDESolverConfig(SolverConfig);
    std::stringstream SolverConfigStr;
    SolverConfigStr << "Using solver config: "
                    << TaintAnalysis.getIFDSIDESolverConfig() << '\n';
    spdlog::debug(SolverConfigStr.str());
    psr::IFDSSolver Solver(TaintAnalysis);
    spdlog::debug("Solving simplified data-flow analysis ...");
    Solver.solve();
    spdlog::debug("Data-flow analysis has been solved.");
    if (Debug) {
      spdlog::debug("Raw data-flow results:");
      std::stringstream RawSolverOut;
      Solver.dumpResults(RawSolverOut);
      spdlog::debug(RawSolverOut.str());
    }
    // Retrieve all usages of data that is depending on the initial seeds. In
    // case of command-line tools, these are data-flow facts that are
    // transitively reachable from the argc and argv parameters of the main
    // function.
    auto SolverRes = Solver.getSolverResults();
    auto AllResEntries = SolverRes.getAllResultEntries();
    // Container to store potential neck candidates that have been identified by
    // the taint analysis.
    // Iterate all instructions and check if any of those
    // instructions uses a tainted value. These tainted instruction operands are
    // neck candidates.
    for (auto &Res : AllResEntries) {
      const llvm::Instruction *Inst = Res.getRowKey();
      auto ResAtInst = SolverRes.resultsAt(Inst);
      // Iterate all operands of an instruction an check if one of them is
      // tainted. If so, this instruction is a potential neck candidate.
      for ([[maybe_unused]] const auto &Op : Inst->operands()) {
        for (auto &[Fact, Value] : ResAtInst) {
          if (Op == Fact) {
            NeckCandidates.push_back(
                const_cast<llvm::Instruction *>(Inst)); // NOLINT ;-)
            if (llvm::isa<llvm::CmpInst>(Inst) ||
                llvm::isa<llvm::BranchInst>(Inst) ||
                llvm::isa<llvm::PHINode>(Inst)) {
              UserBranchAndCompInstructions.insert(
                  const_cast<llvm::BasicBlock *>(Inst->getParent())); // NOLINT
            }
          }
        }
      }
    }
  }
}

std::vector<llvm::Instruction *> TaintAnalysis::getNeckCandidates() {
  return NeckCandidates;
}

std::unordered_set<llvm::BasicBlock *>
TaintAnalysis::getUserBranchAndCompInstructions() {
  return UserBranchAndCompInstructions;
}

psr::LLVMBasedICFG &TaintAnalysis::getLLVMBasedICFG() { return I; }

} // namespace neckid
