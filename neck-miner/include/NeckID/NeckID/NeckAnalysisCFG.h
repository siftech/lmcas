#ifndef NECKID_NECKID_NECKANALYSISCFG_H
#define NECKID_NECKID_NECKANALYSISCFG_H

/******************************************************************************
 * Copyright (c) 2022 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "llvm/ADT/GraphTraits.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"

#include "NeckID/NeckID/NeckAnalysis.h"
#include "NeckID/NeckID/NeckSearchTypes.h"
#include "NeckID/NeckID/NeckVerification.h"

namespace neckid {

struct NeckAnalysisCFG {
  // Displays the basic blocks for the specified function. Neck candidates and
  // the identified neck a highlighted (if they are located in the chosen
  // function).
  NeckAnalysisCFG(NeckAnalysis &NA, llvm::Function &F,
                  const std::string &ProgramName = "");

  NeckAnalysisCFG(NeckVerification &NV, llvm::Function &F,
                  const std::string &ProgramName = "");

  void viewCFG() const;
  std::string writeCFG(const std::string &OutPath) const;

  llvm::Function &DisplayFunction;
  llvm::BasicBlock *Neck;
  uint32_t NeckIsnsIndex;
  llvm::BasicBlock *GroundTruth;
  std::unordered_set<llvm::BasicBlock *> NeckBBs;
  std::unordered_set<llvm::BasicBlock *> TaintedBasicBlocks;
  std::unordered_set<llvm::BasicBlock *> ArticulationPoints;
  std::unordered_set<llvm::BasicBlock *> ChokePoints;
  std::unordered_map<llvm::BasicBlock *, AttributeMap> BB_AttrMaps;
  std::unordered_set<llvm::BasicBlock *> UserBranchAndCompInstructions;
  std::unordered_map<const llvm::BasicBlock *, size_t> DistanceMap;
  std::unordered_map<const llvm::BasicBlock *, bool> LeafMap;
  std::unordered_map<const llvm::BasicBlock *, bool> LoopMap;
  std::unordered_map<llvm::BasicBlock *, std::deque<std::string>>
      PassAnnotations;
  std::string ProgramName;
};

} // namespace neckid

namespace llvm {

template <>
struct GraphTraits<const neckid::NeckAnalysisCFG *>
    : public GraphTraits<const BasicBlock *> {
  using NodeRef = GraphTraits<const BasicBlock *>::NodeRef;
  using ChildIteratorType = GraphTraits<const BasicBlock *>::ChildIteratorType;

  static NodeRef getEntryNode(const neckid::NeckAnalysisCFG *NACFG) {
    return &NACFG->DisplayFunction.getEntryBlock();
  }

  using nodes_iterator = pointer_iterator<Function::iterator>;

  static nodes_iterator
  nodes_begin(const neckid::NeckAnalysisCFG *NACFG) { // NOLINT
    return nodes_iterator(NACFG->DisplayFunction.begin());
  }

  static nodes_iterator
  nodes_end(const neckid::NeckAnalysisCFG *NACFG) { // NOLINT
    return nodes_iterator(NACFG->DisplayFunction.end());
  }

  static size_t size(const neckid::NeckAnalysisCFG *NACFG) {
    return NACFG->DisplayFunction.size();
  }
};

template <>
struct DOTGraphTraits<const neckid::NeckAnalysisCFG *> : DefaultDOTGraphTraits {
  DOTGraphTraits(bool IsSimple = false) : DefaultDOTGraphTraits(IsSimple) {}

  static std::string getGraphName(const neckid::NeckAnalysisCFG *NACFG) {
    return !(NACFG->ProgramName.empty())
               ? "Neck Analysis for '" +
                     NACFG->DisplayFunction.getName().str() +
                     "' Function in '" + NACFG->ProgramName + "'"
               : "Neck Analysis for '" +
                     NACFG->DisplayFunction.getName().str() + "'";
  }

  std::string getNodeLabel(const BasicBlock *Node,
                           const neckid::NeckAnalysisCFG *NACFG) {
    size_t Distance = NACFG->DistanceMap.find(Node)->second;
    FuncInfos.push_back(std::make_unique<DOTFuncInfo>(&NACFG->DisplayFunction));

    bool IsGroundTruth = NACFG->GroundTruth && Node == NACFG->GroundTruth;

    bool IsLeaf = NACFG->LeafMap.find(Node)->second;

    bool IsTainted = false;
    if (NACFG->TaintedBasicBlocks.find(const_cast<llvm::BasicBlock *>(Node)) !=
        NACFG->TaintedBasicBlocks.end()) {
      IsTainted = true;
    }

    std::stringstream Label;
    if (IsGroundTruth) {
      Label << "Ground Truth"
            << R"(\|\{\|\{)";
    }
    if (IsTainted) {
      std::string Guard(50, '|');
      Label << Guard << " Tainted " << Guard << "\\|";
    }

    Label << "Distance: " << Distance << "\\|";

    auto it = NACFG->BB_AttrMaps.find(const_cast<llvm::BasicBlock *>(Node));
    if (it != NACFG->BB_AttrMaps.end()) {
      const neckid::AttributeMap &attrMap = it->second;
      Label << "Succeeded Loops: "
            << std::any_cast<int>(
                   attrMap.at(neckid::attrname_numSucceededLoopHeads))
            << ", ";
      Label << "Succeeded Tainted Loops: "
            << std::any_cast<int>(
                   attrMap.at(neckid::attrname_numSucceededTaintedLoopHeads))
            << "\\|";
    }
    Label << DOTGraphTraits<DOTFuncInfo *>::getCompleteNodeLabel(
        Node, FuncInfos.back().get());

    // Get annotations
    auto Itr =
        NACFG->PassAnnotations.find(const_cast<llvm::BasicBlock *>(Node));
    if (Itr != NACFG->PassAnnotations.end()) {
      Label << "\\|"
            << "Annotations:";
      for (const auto &Ann : Itr->second) {
        Label << " " << Ann;
      }
    }

    if (IsLeaf) {
      std::string Guard(50, '|');
      Label << "\\|" << Guard << " Leaf " << Guard;
    }

    if (IsGroundTruth) {
      Label << R"(\}\|\}\|)";
    }
    return Label.str();
  }

  static std::string getNodeAttributes(const BasicBlock *Node,
                                       const neckid::NeckAnalysisCFG *NACFG) {
    bool IsNeck = false;
    if (Node == NACFG->Neck) {
      IsNeck = true;
    }
    bool IsNeckCandidate = false;
    if (std::find(NACFG->NeckBBs.begin(), NACFG->NeckBBs.end(), Node) !=
        NACFG->NeckBBs.end()) {
      IsNeckCandidate = true;
    }
    bool IsUsedInComparisonOrBranchInstruction = false;
    if (NACFG->UserBranchAndCompInstructions.find(
            const_cast<llvm::BasicBlock *>(Node)) !=
        NACFG->UserBranchAndCompInstructions.end()) {
      IsUsedInComparisonOrBranchInstruction = true;
    }

    bool IsTainted = false;
    if (NACFG->TaintedBasicBlocks.find(const_cast<llvm::BasicBlock *>(Node)) !=
        NACFG->TaintedBasicBlocks.end()) {
      IsTainted = true;
    }

    bool IsArticulationPoint = false;
    if (NACFG->ArticulationPoints.find(const_cast<llvm::BasicBlock *>(Node)) !=
        NACFG->ArticulationPoints.end()) {
      IsArticulationPoint = true;
    }

    bool IsChokePoint = false;
    if (NACFG->ChokePoints.find(const_cast<llvm::BasicBlock *>(Node)) !=
        NACFG->ChokePoints.end()) {
      IsChokePoint = true;
    }

    bool IsGroundTruth = NACFG->GroundTruth && Node == NACFG->GroundTruth;
    bool IsLoop = NACFG->LoopMap.find(Node)->second;

    std::stringstream Attrs;
    Attrs << "style=\"filled";
    if (IsLoop) {
      Attrs << ",diagonals";
    }
    Attrs << "\",";

    if (IsNeck) {
      if (IsGroundTruth) {
        Attrs << "fillcolor=\"#55FF55\"";
      } else {
        Attrs << "fillcolor=\"#FF4444\"";
      }
    } else if (IsNeckCandidate) {
      if (IsGroundTruth) {
        Attrs << "fillcolor=\"#FFAA55\"";
      } else {
        Attrs << "fillcolor=\"#DDFFDD\"";
      }
    } else {
      if (IsGroundTruth) {
        Attrs << "fillcolor=\"#FF80a0\"";
      } else {
        Attrs << "fillcolor=\"#FFFFFF\"";
      }
    }
    Attrs << ",";

    Attrs << "";
    if (IsArticulationPoint) {
      if (IsChokePoint) {
        Attrs << "color=\"#FF00FF\",";
      } else {
        Attrs << "color=\"#FFC8FF\",";
      }
    }

    if (IsUsedInComparisonOrBranchInstruction) {
      Attrs << "fontcolor=\"#2222FF\",";
    }

    if (IsGroundTruth) {
      Attrs << "margin=\"0.75,0.1\",";
    }

    if (IsChokePoint) {
      Attrs << "penwidth=8";
    } else {
      Attrs << "penwidth=1";
    }
    return Attrs.str();
  }

  static std::string getEdgeAttributes(const BasicBlock *Node,
                                       const_succ_iterator I,
                                       const neckid::NeckAnalysisCFG *NACFG) {
    return DefaultDOTGraphTraits::getEdgeAttributes(Node, I,
                                                    &NACFG->DisplayFunction);
  }

  std::vector<std::unique_ptr<DOTFuncInfo>> FuncInfos;
};

} // namespace llvm

#endif
