/******************************************************************************
 * Copyright (c) 2021 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <string>

#include "llvm/IR/Function.h"

#include "NeckID/NeckID/NeckAnalysis.h"
#include "NeckID/NeckID/NeckAnalysisCFG.h"

namespace neckid {

neckid::NeckAnalysisCFG::NeckAnalysisCFG(NeckAnalysis &NA, llvm::Function &F,
                                         const std::string &ProgramName)
    : DisplayFunction(F), GroundTruth(nullptr), NeckBBs(NA.getNeckCandidates()),
      TaintedBasicBlocks(NA.getTaintedBasicBlocks()),
      ArticulationPoints(NA.getArticulationPoints()),
      ChokePoints(NA.getChokePoints()),
      BB_AttrMaps(NA.getBasicBlockAttributeMap()),
      UserBranchAndCompInstructions(NA.getUserBranchAndCompInstructions()),
      PassAnnotations(NA.getPassAnnotations()), ProgramName(ProgramName) {
  const NeckMinerResults results = NA.getNeck();
  Neck = std::get<0>(results);
  NeckIsnsIndex = std::get<1>(results);

  llvm::BasicBlock &Front = DisplayFunction.front();
  for (auto &BB : DisplayFunction) {
    size_t Distance;
    if (!NA.isReachable(&Front, &BB, Distance, false, nullptr)) {
      Distance = -1;
    }
    DistanceMap[&BB] = Distance;
    // llvm::outs() << "Src is Dst: " << (&Front == &BB) << "\n"
    //              << "Src:\n" << Front << "\n"
    //              << "Dst:\n" << BB << "\n"
    //              << "Distance: " << Distance << "\n";

    LoopMap[&BB] = NA.isInLoopStructue(&BB);
    LeafMap[&BB] = llvm::succ_size(&BB) < 1;
  }
}

neckid::NeckAnalysisCFG::NeckAnalysisCFG(NeckVerification &NV,
                                         llvm::Function &F,
                                         const std::string &ProgramName)
    : NeckAnalysisCFG(NV.getNeckAnalysis(), F, ProgramName) {
  GroundTruth = NV.getGroundTruth();
}

void neckid::NeckAnalysisCFG::viewCFG() const {
  ViewGraph(this, "Neck-Analysis-CFG:" + DisplayFunction.getName());
}

std::string
neckid::NeckAnalysisCFG::writeCFG(const std::string &OutPath) const {
  return WriteGraph(this, "Neck-Analysis-CFG:" + DisplayFunction.getName(),
                    false, "", OutPath);
}

} // namespace neckid
