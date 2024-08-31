#ifndef NECKID_NECKID_NECKVERIFICATION_H
#define NECKID_NECKID_NECKVERIFICATION_H

#include <string>

#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include "NeckID/NeckID/NeckAnalysis.h"

namespace neckid {

class NeckVerification {
private:
  llvm::BasicBlock *GroundTruth;

  llvm::SMDiagnostic Diag;
  llvm::LLVMContext CTX;
  std::unique_ptr<llvm::Module> M;
  std::unique_ptr<NeckAnalysis> NA;

  llvm::CallBase *retrieveIDFunctionCall(const std::string &FunName);

public:
  NeckVerification(
      const std::string &PathToModuleFile,
      const std::string &PathToTaintConfigFile, bool FunctionLocalPTAwoGlobals,
      bool UseSimplifiedDFA, const std::string &FunctionName = "main",
      const std::optional<std::string> &CombinedPath = std::nullopt,
      const std::optional<std::string> &TapePath = std::nullopt,
      bool debug = false);

  bool isCorrect();
  NeckMinerResults getNeck();
  std::optional<NeckMinerOutput> getOutput();
  BB_BFS_Q_Path &getNeckPath();
  llvm::BasicBlock *getGroundTruth();

  NeckAnalysis &getNeckAnalysis();

  std::string writeCFG(const std::string &OutPath,
                       const std::string &ProgramName,
                       const std::string &FunctionName = "main");
  std::string writeCFG(const std::string &OutPath,
                       const std::string &ProgramName,
                       llvm::Function *FunctionName);
};
} // namespace neckid

#endif
