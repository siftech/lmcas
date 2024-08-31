// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef GUINESS__CONFIG_H
#define GUINESS__CONFIG_H

#include <absl/container/flat_hash_set.h>
#include <regex>
#include <string>
#include <vector>

namespace guiness {

struct Config {
  std::vector<std::regex> safeExternalFunctionRegexes = {
      std::regex{"(alloc|aligned_alloc|__libc_malloc_impl|__libc_free|__libc_"
                 "realloc|__malloc_donate)(\\.[0-9]+)?"},
      std::regex{"mem(cmp|cpy|move|set)"},
      std::regex{"__get_tp(\\.[0-9]+)?"},
      std::regex{"getenv"},
  };
  absl::flat_hash_set<std::string> ignorableFunctionAttributes = {
      // The various optimization levels don't result in semantic changes.
      "optsize",

      // We're insensitive to function termination (since we have the tape, we
      // know which functions terminated!), so we can ignore these attributes.
      "nounwind",
      "willreturn",

      // We don't compute values directly, so these attributes can be ignored
      // too.
      "readnone",
      "signext",
      "zeroext",

      // We don't perform the optimizations these attributes inhibit, so we can
      // ignore them.
      "nobuiltin",
      "\"no-builtins\"",
      "strictfp",
      "\"strictfp\"",
  };

  /**
   * Returns whether a function attribute can be safely ignored.
   */
  auto isIgnorableFunctionAttribute(const std::string &name) const -> bool {
    return this->ignorableFunctionAttributes.find(name) !=
           this->ignorableFunctionAttributes.end();
  }

  /**
   * Returns whether a function is marked as a safe external function.
   */
  auto isSafeExternalFunction(const std::string &name) const -> bool {
    // We just check if each regex is a match.
    for (const auto &regex : this->safeExternalFunctionRegexes)
      if (std::regex_match(name, regex))
        return true;
    return false;
  }
};

} // namespace guiness

#endif
