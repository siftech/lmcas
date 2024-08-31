// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__FMT_LLVM_H
#define TABACCO__FMT_LLVM_H

#include <fmt/format.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

#define LMCAS_DEF_LLVM_FORMATTER(TYPE)                                         \
  template <> struct fmt::formatter<TYPE> : fmt::formatter<std::string_view> { \
    template <typename FormatContext>                                          \
    auto format(const TYPE &val, FormatContext &ctx) {                         \
      std::string name;                                                        \
      llvm::raw_string_ostream stream(name);                                   \
      val.print(stream);                                                       \
      return fmt::formatter<string_view>::format(name, ctx);                   \
    }                                                                          \
  }

#define LMCAS_DEF_LLVM_OPERAND_FORMATTER(TYPE)                                 \
  template <> struct fmt::formatter<TYPE> : fmt::formatter<std::string_view> { \
    template <typename FormatContext>                                          \
    auto format(const TYPE &val, FormatContext &ctx) {                         \
      std::string name;                                                        \
      llvm::raw_string_ostream stream(name);                                   \
                                                                               \
      if (!val.getName().empty())                                              \
        name = val.getName();                                                  \
      else                                                                     \
        val.printAsOperand(stream, true);                                      \
                                                                               \
      return fmt::formatter<string_view>::format(name, ctx);                   \
    }                                                                          \
  }

LMCAS_DEF_LLVM_OPERAND_FORMATTER(llvm::BasicBlock);
LMCAS_DEF_LLVM_OPERAND_FORMATTER(llvm::Function);
LMCAS_DEF_LLVM_OPERAND_FORMATTER(llvm::Value);

LMCAS_DEF_LLVM_FORMATTER(llvm::Argument);
LMCAS_DEF_LLVM_FORMATTER(llvm::Instruction);
LMCAS_DEF_LLVM_FORMATTER(llvm::Metadata);
LMCAS_DEF_LLVM_FORMATTER(llvm::Type);

#undef LMCAS_DEF_LLVM_FORMATTER
#undef LMCAS_DEF_LLVM_OPERAND_FORMATTER

#endif
