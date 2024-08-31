// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "Spec.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace spec {

void from_json(const nlohmann::json &j, OptionConfig &obj) {
  obj.args = j.at("args").get<std::vector<std::string>>();
  obj.env = j.at("env").get<absl::flat_hash_map<std::string, std::string>>();
  obj.cwd = j.at("cwd").get<std::string>();
  obj.tape = j.at("tape").get<tape::Tape>();
}

void from_json(const nlohmann::json &j, Spec &obj) {
  obj.binary = j.at("binary").get<std::string>();
  obj.configs = j.at("configs").get<std::vector<OptionConfig>>();
}

Spec load_spec_from_file(const std::string &path) {
  nlohmann::json spec_as_json;
  std::ifstream file(path);
  file >> spec_as_json;
  return spec_as_json.get<Spec>();
}

}; // namespace spec
