// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__SPEC_H
#define TABACCO__SPEC_H

#include "Tape.h"
#include <absl/container/flat_hash_map.h>
#include <fmt/ranges.h>
#include <string>
#include <vector>

namespace spec {

struct OptionConfig {
  std::vector<std::string> args;
  absl::flat_hash_map<std::string, std::string> env;
  std::string cwd;
  tape::Tape tape;
};

struct Spec {
  std::string binary;
  std::vector<OptionConfig> configs;
};

Spec load_spec_from_file(const std::string &path);

}; // namespace spec

template <>
struct fmt::formatter<spec::OptionConfig> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const spec::OptionConfig &config, FormatContext &ctx) {
    auto out =
        fmt::format("OptionConfig {{ args: {}, env: {}, cwd: {}, tape: ... }}",
                    config.args, config.env, config.cwd);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<spec::Spec> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const spec::Spec &spec, FormatContext &ctx) {
    auto out = fmt::format("Spec {{ binary: {}, configs: {} }}", spec.binary,
                           spec.configs);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

#endif
