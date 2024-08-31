#ifndef TABACCO__FMT_LLVM_H
#define TABACCO__FMT_LLVM_H

#include <fmt/format.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>
#include <spdlog/spdlog.h>
#include <string>

template <typename T> struct llvm_formatter {
  bool alternate;

  constexpr auto parse(fmt::format_parse_context &ctx)
      -> decltype(ctx.begin()) {
    auto iter = ctx.begin(), end = ctx.end();
    if (iter == end) {
      alternate = false;
    } else {
      char ch = *iter++;
      if (ch != '?')
        throw fmt::format_error(fmt::format(
            "invalid format for {}: unknown format specification character",
            typeid(T).name()));
      if (iter != end && *iter != '}')
        throw fmt::format_error(
            fmt::format("invalid format for {}: too long", typeid(T).name()));
      alternate = true;
    }
    return iter;
  }

  template <typename FormatContext>
  auto format(const T &val, FormatContext &ctx) {
    std::string str;
    llvm::raw_string_ostream stream(str);
    if (!this->alternate)
      val.printAsOperand(stream);
    else
      val.print(stream);
    return fmt::format_to(ctx.out(), "{}", str);
  }
};

// Instantiations.

template <>
struct fmt::formatter<llvm::Argument> : llvm_formatter<llvm::Argument> {};

template <>
struct fmt::formatter<llvm::BasicBlock> : llvm_formatter<llvm::BasicBlock> {};

template <>
struct fmt::formatter<llvm::Function> : llvm_formatter<llvm::Function> {};

template <>
struct fmt::formatter<llvm::Instruction> : llvm_formatter<llvm::Instruction> {
  template <typename FormatContext>
  auto format(const llvm::Instruction &inst, FormatContext &ctx) {
    // We handle printing instructions differently. We almost never want to
    // print a generic instruction as an operand (ones that return void aren't
    // even printable as operands!), so the default is to print it normally.
    //
    // In alternate mode, we instead print the whole function the instruction is
    // in, with the instruction bolded.

    std::string instStr;
    llvm::raw_string_ostream stream(instStr);
    inst.print(stream);

    if (!alternate) {
      return fmt::format_to(ctx.out(), "{}", instStr);
    } else {
      // Format the entire function.
      std::string funcStr;
      llvm::raw_string_ostream stream(funcStr);
      inst.getParent()->getParent()->print(stream);
      std::string_view funcStrView{funcStr};

      // Iterate over lines of the parent.
      auto out = ctx.out();
      size_t pos = 0;
      while (pos < funcStrView.size()) {
        auto end = funcStrView.find('\n', pos);
        auto line = funcStr.substr(pos, end - pos);
        pos = end + 1; // Advance past the newline.

        bool isInst = line == instStr;
        out = fmt::format_to(out, "{}{}{}\n", isInst ? "\x1b[1;36m" : "", line,
                             isInst ? "\x1b[0m" : "");
      }
      return out;
    }
  }
};

template <>
struct fmt::formatter<llvm::Metadata> : llvm_formatter<llvm::Metadata> {};

template <> struct fmt::formatter<llvm::Type> : llvm_formatter<llvm::Type> {};

template <> struct fmt::formatter<llvm::Value> : llvm_formatter<llvm::Value> {};

#endif
