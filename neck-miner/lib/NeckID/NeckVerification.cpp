#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include "NeckID/NeckID/NeckAnalysisCFG.h"
#include "NeckID/NeckID/NeckVerification.h"

#include "FmtLlvm.h"
#include <spdlog/spdlog.h>

namespace {
const std::string NeckIDFunctionName = "_neck_identification_mark_as_neck_";
const std::string KleeIDFunctionName = "_lmcas_neck";
} // anonymous namespace

namespace neckid {

llvm::CallBase *
NeckVerification::retrieveIDFunctionCall(const std::string &FunName) {
  auto *NeckID = M->getFunction(FunName);
  if (!NeckID) {
    return nullptr;
  }
  assert(NeckID->hasOneUse() &&
         "Expected one and only one use of the id marker function!");
  llvm::CallBase *NeckIDCallSite = nullptr;
  for (auto *User : NeckID->users()) {
    // get the call to the special neck id function
    if ((NeckIDCallSite = llvm::dyn_cast<llvm::CallBase>(User))) {
      return NeckIDCallSite;
    }
  }
  return nullptr;
}

bool NeckVerification::isCorrect() {
  return std::get<0>(getNeck()) == GroundTruth;
}

NeckMinerResults NeckVerification::getNeck() { return NA->getNeck(); }
std::optional<NeckMinerOutput> NeckVerification::getOutput() {
  return NA->getOutput();
}
BB_BFS_Q_Path &NeckVerification::getNeckPath() { return NA->getNeckPath(); }

llvm::BasicBlock *NeckVerification::getGroundTruth() { return GroundTruth; }

NeckAnalysis &NeckVerification::getNeckAnalysis() { return *NA; }

std::string NeckVerification::writeCFG(const std::string &OutPath,
                                       const std::string &ProgramName,
                                       const std::string &FunctionName) {
  auto *F = M->getFunction(FunctionName);
  assert(F && "Expected to find a '" + FunctionName + "' function!");
  return writeCFG(OutPath, ProgramName, F);
}

std::string NeckVerification::writeCFG(const std::string &OutPath,
                                       const std::string &ProgramName,
                                       llvm::Function *Function) {
  auto CFG = NeckAnalysisCFG(*this, *Function, ProgramName);
  return CFG.writeCFG(OutPath);
}

NeckVerification::NeckVerification(
    const std::string &PathToModuleFile,
    const std::string &PathToTaintConfigFile, bool FunctionLocalPTAwoGlobals,
    bool UseSimplifiedDFA, const std::string &FunctionName,
    const std::optional<std::string> &CombinedPath,
    const std::optional<std::string> &TapePath, bool debug)
    : GroundTruth(nullptr) {
  // Parsing
  bool BrokenDbgInfo = false;
  M = llvm::parseIRFile(PathToModuleFile, Diag, CTX);
  assert(M && "Could not parse module!");
  if (llvm::verifyModule(*M, &llvm::errs(), &BrokenDbgInfo)) {
    spdlog::error("error: invalid module");
  }
  if (BrokenDbgInfo) {
    spdlog::error("caution: debug info is broken");
  }
  // Neck identification
  NA = std::make_unique<NeckAnalysis>(
      *M, PathToTaintConfigFile, FunctionLocalPTAwoGlobals, UseSimplifiedDFA,
      FunctionName, CombinedPath, TapePath, debug);

  // Get ground truth and compare with the computed result
  llvm::CallBase *NeckIDCallSite = nullptr;
  NeckIDCallSite = retrieveIDFunctionCall(NeckIDFunctionName);
  if (!NeckIDCallSite) {
    NeckIDCallSite = retrieveIDFunctionCall(KleeIDFunctionName);
  }

  if (!NeckIDCallSite) {
    spdlog::info("Failed to find call site for ground truth neck!");
    return;
  }
  // Get the callsites respective basic block
  GroundTruth = NeckIDCallSite->getParent();
}
} // namespace neckid
