// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "FindNeck.h"

#include "Annotation.h"
#include <absl/container/flat_hash_map.h>
#include <cassert>
#include <fstream>
#include <llvm/Support/CommandLine.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace tabacco {

struct u64_as_string {
  uint64_t value;

  u64_as_string() = default;
  u64_as_string(const uint64_t v) : value(v) {}
  u64_as_string(const u64_as_string &v) = default;
  u64_as_string &operator=(const u64_as_string &rhs) = default;
  u64_as_string &operator=(const uint64_t &rhs) {
    value = rhs;
    return *this;
  }
  operator const uint64_t &() const { return value; }
  bool operator==(const u64_as_string &rhs) const { return value == rhs.value; }
  bool operator==(const uint64_t &rhs) const { return value == rhs; }
};

static void from_json(const nlohmann::json &json, u64_as_string &obj) {
  obj.value = std::stoull(json.get<std::string>());
}

struct NeckLocation {
  uint64_t basicBlockAnnotationId;
  uint32_t insnIndex;
};

static void from_json(const nlohmann::json &json, NeckLocation &obj) {
  obj = {json.at("basic_block_annotation_id").get<u64_as_string>(),
         json.at("insn_index").get<uint32_t>()};
}

static auto loadNeckLocations(std::string const &path)
    -> std::vector<NeckLocation> {
  nlohmann::json json;
  {
    std::ifstream file(path);
    file >> json;
  }

  return json.get<std::vector<NeckLocation>>();
}

static auto findAnnotatedBasicBlocks(llvm::Module &mod)
    -> absl::flat_hash_map<uint64_t, llvm::BasicBlock *> {
  // This just calls getBasicBlockID on every basic block in the program and
  // accumulates the results into a map.
  absl::flat_hash_map<uint64_t, llvm::BasicBlock *> out;
  for (auto &function : mod)
    for (auto &basicBlock : function)
      if (auto id = getBasicBlockID(basicBlock))
        out.insert_or_assign(id.value(), &basicBlock);
  return out;
}

static llvm::cl::opt<std::string>
    neckLocationsOpt("neck-locations",
                     llvm::cl::desc("The path to a JSON file containing the "
                                    "locations of additional necks."));

auto findNeckMarkers(llvm::Module &mod, llvm::StringRef neckMarkerName)
    -> absl::flat_hash_set<llvm::Instruction *> {
  absl::flat_hash_set<llvm::Instruction *> out;

  // Look for calls to the named neck marker function.
  for (auto &function : mod)
    for (auto &basicBlock : function)
      for (auto &insn : basicBlock)
        if (auto callInst = llvm::dyn_cast<llvm::CallInst>(&insn))
          if (auto calledFunction = callInst->getCalledFunction())
            if (calledFunction->getName() == neckMarkerName)
              out.insert(callInst);

  // Load the locations from the neck locations file, if one was specified.
  auto const &neckLocationsPath = neckLocationsOpt.getValue();
  auto annotatedBasicBlocks = findAnnotatedBasicBlocks(mod);
  if (neckLocationsPath != "") {
    for (auto neckLocation : loadNeckLocations(neckLocationsPath)) {
      // Find the basic block.
      assert(
          annotatedBasicBlocks.contains(neckLocation.basicBlockAnnotationId));
      auto basicBlock =
          annotatedBasicBlocks.at(neckLocation.basicBlockAnnotationId);

      // Find the instruction.
      auto inst = basicBlock->begin();
      for (uint32_t i = 0; i < neckLocation.insnIndex; i++) {
        assert(!llvm::isa<llvm::Terminator>(*inst));
        inst++;
      }

      // Add it to the values to be returned.
      out.insert(&*inst);
    }
  }

  return out;
}

} // namespace tabacco
