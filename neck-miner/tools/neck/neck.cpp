#include <filesystem>
#include <memory>
#include <sstream>

#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include "boost/program_options.hpp"

#include "NeckID/NeckID/NeckAnalysis.h"
#include "NeckID/NeckID/NeckAnalysisCFG.h"

namespace {

void validateParamModule(const std::string &Module) {
  if (Module.empty()) {
    throw boost::program_options::error_with_option_name(
        "At least one LLVM target module is required!");
  }
  std::filesystem::path ModulePath(Module);
  if (!(std::filesystem::exists(ModulePath) &&
        !std::filesystem::is_directory(ModulePath) &&
        (ModulePath.extension() == ".ll" || ModulePath.extension() == ".bc"))) {
    throw boost::program_options::error_with_option_name(
        "LLVM module '" + Module + "' does not exist!");
  }
}

void validateParamConfig(const std::string &TaintConfig) {
  if (TaintConfig.empty()) {
    throw boost::program_options::error_with_option_name(
        "Taint configuration is required!");
  }
  std::filesystem::path TaintConfigPath(TaintConfig);
  if (!(std::filesystem::exists(TaintConfigPath) &&
        !std::filesystem::is_directory(TaintConfigPath) &&
        (TaintConfigPath.extension() == ".json"))) {
    throw boost::program_options::error_with_option_name(
        "Taint configuration '" + TaintConfig + "' does not exist!");
  }
}

} // anonymous namespace

int main(int Argc, char **Argv) {
  // Declare a group of options that will be allowed only on command line
  boost::program_options::options_description Generic("Command-line options");
  Generic.add_options()("help,h", "Print help message");
  // Declare a group of options that will be allowed both on command line
  // and in config file
  boost::program_options::options_description Config(
      "Configuration file options");
  // clang-format off
  Config.add_options()
    ("module,m", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateParamModule), "Path to the module under analysis")
    ("taint-config,c", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateParamConfig), "Path to the taint configuration")
    ("function-local-points-to-info-wo-globals", "Uses only function-local points to information (ignores points-to relations through global variables, too)")
    ("use-simplified-dfa", "Uses a simpler, less expensive data-flow analysis that may be less precise")
    ("verbose,v", "Print output to the command line (=default is false)")
    ("annotate", "Dump neck-annotated LLVM IR to the commandline (=default is false)")
    ("no-display", "Do not require a display when executing (=default is false)");
  // clang-format on
  boost::program_options::options_description CmdlineOptions;
  CmdlineOptions.add(Generic).add(Config);
  boost::program_options::options_description Visible("Allowed options");
  Visible.add(Generic).add(Config);
  boost::program_options::variables_map Vars;
  try {
    boost::program_options::store(
        boost::program_options::command_line_parser(Argc, Argv)
            .options(CmdlineOptions)
            .run(),
        Vars);
    boost::program_options::notify(Vars);
  } catch (boost::program_options::error &Err) {
    llvm::errs() << "Could not parse command-line arguments!\n"
                 << "Error: " << Err.what() << '\n';
    return 1;
  }
  // Perform some simple sanity checks on the parsed command-line arguments.
  if (Vars.empty() || Vars.count("help")) {
    std::stringstream StrStr;
    StrStr << Visible;
    llvm::outs() << StrStr.str() << '\n';
    return 0;
  }
  if (!Vars.count("module")) {
    llvm::outs() << "Need to specify an LLVM (.ll/.bc) module for analysis.\n";
    return 0;
  }
  if (!Vars.count("taint-config")) {
    llvm::outs() << "Need to specify a taint configuration to determine "
                    "potential neck candidates.\n";
    return 0;
  }
  // Parse an LLVM IR file.
  llvm::SMDiagnostic Diag;
  llvm::LLVMContext CTX;
  std::unique_ptr<llvm::Module> M =
      llvm::parseIRFile(Vars["module"].as<std::string>(), Diag, CTX); // NOLINT
  // Check if the module is valid.
  bool BrokenDbgInfo = false;
  if (llvm::verifyModule(*M, &llvm::errs(), &BrokenDbgInfo)) {
    llvm::errs() << "error: invalid module!\n";
    return 1;
  }
  if (BrokenDbgInfo) {
    llvm::errs() << "caution: debug info is broken!\n";
  }
  neckid::NeckAnalysis NA(
      *M, Vars["taint-config"].as<std::string>(),
      Vars.count("function-local-points-to-info-wo-globals"),
      Vars.count("use-simplified-dfa"), "main", std::nullopt, std::nullopt,
      Vars.count("verbose"));
  const auto &[Neck, IsnsIndex] = NA.getNeck();
  if (!Neck) {
    llvm::outs() << "No neck found!\n";
  }
  llvm::outs() << "Display identified neck and neck candidates within 'main'\n";
  auto *Main = M->getFunction("main");
  assert(Main && "Expected to find a 'main' function!");
  neckid::NeckAnalysisCFG G(NA, *Main, Vars["module"].as<std::string>());
  if (!Vars.count("no-display")) {
    G.viewCFG();
  }
  if (Neck) {
    NA.markNeck();
  }
  if (Vars.count("annotate")) {
    std::filesystem::path Path(Vars["module"].as<std::string>());
    std::string FileName(Path.stem());
    std::error_code EC;
    llvm::raw_fd_ostream OF(FileName + "_neck.ll", EC);
    NA.dumpModule(OF);
  }
  return 0;
}
