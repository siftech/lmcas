#include <boost/program_options/variables_map.hpp>
#include <filesystem>
#include <llvm/ADT/StringRef.h>
#include <memory>
#include <sstream>
#include <string>

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include "boost/program_options.hpp"

#include "NeckID/NeckID/Annotation.h"
#include "NeckID/NeckID/NeckVerification.h"

#include "FmtLlvm.h"
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// guiness
#include "GuiNeSS/Annotation.h"
#include "GuiNeSS/Config.h"
#include "GuiNeSS/GuiNeSS.h"
#include "GuiNeSS/Tape.h"
#include <nlohmann/json.hpp>

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

void validateCFG(const std::string &FilePath) {
  if (FilePath.empty()) {
    return;
  }
  std::filesystem::path OutFilePath(FilePath);
  if (std::filesystem::is_directory(OutFilePath)) {
    throw boost::program_options::error_with_option_name(
        "CFG output path '" + FilePath + "' is a directory!");
  }
}

void validateInsertNeckPath(const std::string &FilePath) {
  if (FilePath.empty()) {
    return;
  }
  std::filesystem::path OutFilePath(FilePath);
  if (std::filesystem::is_directory(OutFilePath)) {
    throw boost::program_options::error_with_option_name(
        "Insert Neck output path '" + FilePath + "' is a directory!");
  }
}
void validateNeckPlacementPath(const std::string &FilePath) {
  if (FilePath.empty()) {
    return;
  }
  std::filesystem::path OutFilePath(FilePath);
  if (std::filesystem::is_directory(OutFilePath)) {
    throw boost::program_options::error_with_option_name(
        "Neck placement path '" + FilePath + "' is a directory!");
  }
}
} // anonymous namespace

int verifyNeck(const boost::program_options::variables_map &Vars,
               neckid::NeckVerification &Verification,
               const std::string &ModulePath, const std::string &EntryPoint) {
  const auto &[Neck, IsnsIndex] = Verification.getNeck();
  llvm::BasicBlock *GroundTruth = Verification.getGroundTruth();

  if (Vars.count("write-cfg")) {
    spdlog::info("Starting writing EntryPoint CFG...");
    auto OutPath = Vars["write-cfg"].as<std::string>() + ".dot";
    Verification.writeCFG(OutPath, ModulePath, EntryPoint);
    spdlog::info("Wrote EntryPoint CFG to: {}", OutPath);

    if (Neck) {
      auto &NeckPath = Verification.getNeckPath();
      int idx = 0;
      for (auto &pathElemFunc : NeckPath) {
        llvm::Function *func = std::get<0>(pathElemFunc);
        if (func) {
          spdlog::info("Starting writing NeckFunction CFG...");
          auto OutPath = Vars["write-cfg"].as<std::string>() + "_" +
                         std::to_string(idx) + "_" +
                         std::string(func->getName()) + ".dot";
          Verification.writeCFG(OutPath, ModulePath, func);
          spdlog::info("Wrote NeckFunction CFG to: {}", OutPath);
        } else {
          spdlog::error("ERROR Neck without parent NeckFunction");
        }
        idx += 1;
      }
    }
  }

  if (!GroundTruth) {
    spdlog::error("Failed to find ground truth!");
    return EXIT_FAILURE;
  }

  const llvm::Function *GroundTruthFunc = GroundTruth->getParent();
  llvm::StringRef GroundTruthFuncName("???");
  if (GroundTruthFunc) {
    GroundTruthFuncName = GroundTruthFunc->getName();
  }

  if (Neck) {
    const auto &IdentifiedFuncName = Neck->getParent()->getName();
    spdlog::info("Results:"
                 "\n    IdentifiedNeck Func: {}"
                 "\n    IdentifiedNeck: {}"
                 "\n    GroundTruth Func: {}"
                 "\n    GroundTruth: {}",
                 IdentifiedFuncName, *Neck, GroundTruthFuncName, *GroundTruth);
  } else {
    spdlog::info("Did not identify neck:"
                 "\n    GroundTruth Func: {}"
                 "\n    GroundTruth: {}",
                 GroundTruthFuncName, *GroundTruth);
    return EXIT_FAILURE;
  }

  if (!Verification.isCorrect()) {
    spdlog::info("Identified the wrong neck!");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int insertNeck(const boost::program_options::variables_map &Vars,
               neckid::NeckVerification &Verification) {
  auto &NA = Verification.getNeckAnalysis();
  NA.markNeck();
  std::filesystem::path Path(Vars["insert-neck"].as<std::string>());
  std::error_code EC;
  llvm::raw_fd_ostream OF(Path.string(), EC);
  if (Path.extension() == ".ll") {
    spdlog::info("Writing marked llvm ir src to {}", Path.string());
    NA.dumpModule(OF);
  } else {
    spdlog::info("Writing marked llvm bitcode to {}", Path.string());
    llvm::WriteBitcodeToFile(NA.getModule(), OF);
  }
  return EC ? 1 : 0;
}

std::string getNeckPlacementJSON(const neckid::NeckMinerResults results) {
  const auto &[neck, neck_insn_index] = results;
  uint64_t bbid;
  std::stringstream OF;

  if (neck == nullptr) {
    // No Neck Found.
    OF << "[]";
    return OF.str();
  }

  // Found Neck
  std::optional<uint64_t> neckBBIDopt = neckid::getBasicBlockID(*neck);
  if (neckBBIDopt.has_value()) {
    bbid = neckBBIDopt.value();
  } else {
    // We get this answer running with IR files for which the
    // annotation pass had not been applied.
    bbid = UINT64_MAX;
  }

  // Finally output the json.
  OF << "[{";

  OF << "\"function\": "
     << "\"" << neck->getParent()->getName().str() << "\"";
  OF << ", ";

  OF << "\"basic_block_name\": "
     << "\"" << neckid::getBBName(const_cast<llvm::BasicBlock *>(neck))
     << "\", ";

  OF << "\"basic_block_annotation_id\": "
     << "\"" << bbid << "\"";
  OF << ", ";

  OF << "\"insn_index\": " << neck_insn_index;

  OF << "}]";

  return OF.str();
}

// Run GuiNeSS as it was originally written
std::optional<std::tuple<std::string, std::string, uint64_t, int>>
runGuiNeSS(boost::program_options::variables_map Vars) {
  if (!Vars.count("combined-module")) {
    spdlog::error(
        "When using GuiNeSS the 'combined-module' option must be provided.");
    std::exit(EXIT_FAILURE);
  }
  auto CombinedModulePath = Vars["combined-module"].as<std::string>();

  if (!Vars.count("guiness-tape")) {
    spdlog::error(
        "When using GuiNeSS the 'guiness-tape' option must be provided.");
    std::exit(EXIT_FAILURE);
  }
  auto TapePath = Vars["guiness-tape"].as<std::string>();

  llvm::LLVMContext ctx;
  // Load the combined module.
  llvm::SMDiagnostic parseErr;
  auto mod = llvm::parseIRFile(CombinedModulePath, parseErr, ctx);
  if (!mod) {
    spdlog::error("Failed to parse IRFile for combined-module");
    std::exit(EXIT_FAILURE);
  }
  auto tape = tape::load_tape_from_file(TapePath);

  guiness::Config config; // TODO: accept as args ?
  if (auto neck = guiness::findBestNeck(config, *mod, tape)) {
    spdlog::info("Found neck:");
    for (auto const &callInst : neck->stack)
      spdlog::info("  [{}, {}]{}", *callInst->getParent()->getParent(),
                   *callInst->getParent(),
                   *static_cast<llvm::Instruction *>(callInst));
    spdlog::info("  [{}, {}]{}", *neck->inst->getParent()->getParent(),
                 *neck->inst->getParent(), *neck->inst);

    auto inst = neck->inst;
    const llvm::BasicBlock *basicBlock = inst->getParent();
    auto basicBlockId = guiness::getBasicBlockAnnotation(basicBlock);
    if (!basicBlockId) {
      throw std::logic_error{
          fmt::format("basic block {} in {} didn't have an ID, despite "
                      "containing the neck instruction?",
                      *basicBlock, *basicBlock->getParent())};
    }
    auto function = basicBlock->getParent()->getName().str();
    auto bbName = neckid::getBBName(const_cast<llvm::BasicBlock *>(basicBlock));

    size_t instIndex = 0;
    auto instIter = basicBlock->begin();
    while (inst != &*instIter) {
      instIndex++;
      instIter++;
    }
    return {{function, bbName, *basicBlockId, instIndex}};
  }
  return {};
}

int main(int Argc, char **Argv) {
  spdlog::cfg::load_env_levels();
  // https://github.com/gabime/spdlog/wiki/0.-FAQ#switch-the-default-logger-to-stderr
  spdlog::set_default_logger(spdlog::stderr_color_mt("stderr"));
  spdlog::set_default_logger(spdlog::stderr_color_mt(""));

  // Declare a group of options that will be allowed only on command line
  boost::program_options::options_description Generic("Command-line options");
  Generic.add_options()("help,h", "Print help message");
  // Declare a group of options that will be allowed both on command line
  // and in config file
  boost::program_options::options_description Config(
      "Configuration file options");
  // clang-format off
    Config.add_options()
        ("debug", "Enable debugging output (includes verbose).")
        ("verbose", "Enable verbose output.")
        ("module,m", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateParamModule), "Path to the module under analysis")
        ("taint-config,c", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateParamConfig), "Path to the taint configuration")
        ("function-local-points-to-info-wo-globals", "Uses only function-local points to information (ignores points-to relations through global variables, too)")
        ("use-simplified-dfa", "Uses a simpler, less expensive data-flow analysis that may be less precise")
        ("write-cfg", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateCFG), "Path to output the cfg file")
        ("write-participating-functions-cfg",
	 boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateCFG),
	 "Path to output the participating functionscfg file")
        ("entry-point", boost::program_options::value<std::string>(), "Entry point function to run on")

	// TODO: These two are mutually exclusive not for any other
	// reason than the logic in main() is currently strange and
	// needs a rewrite. The rewrite would affect the default
	// behavior of the program and has to be deferred for the
	// initial release.
        ("verify-neck", "Whether or not neck-miner should verify against the ground truth provided in the target module. Mutually exclusive with --insert-neck since it is assumes the neck is already marked in the target module.")
        ("insert-neck", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateInsertNeckPath), "Path to output annotated file.")
        ("neck-placement", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateNeckPlacementPath), "Path to output json file which denotes the neck location.")

	// Guiness related options 
        ("combined-module", boost::program_options::value<std::string>()->multitoken()->zero_tokens()->composing()->notifier(&validateParamModule), "Path to the module under analysis after it has been combined with libc")
        ("guiness-tape", boost::program_options::value<std::string>(), "Path to the tape that GuiNeSS will use.")
        ("only-guiness", "Flag to bypass normal neck-miner behavior and only run guiness")
	;

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
    spdlog::error("Could not parse command-line arguments!\nError: {}",
                  Err.what());
    return EXIT_FAILURE;
  }

  // Perform some simple sanity checks on the parsed command-line arguments.
  if (Vars.empty() || Vars.count("help")) {
    std::stringstream StrStr;
    StrStr << Visible;
    spdlog::info(StrStr.str());
    return EXIT_SUCCESS;
  }

  // exit early if only-guiness
  if (Vars.count("only-guiness") > 0) {
    auto guinessResults = runGuiNeSS(Vars);
    nlohmann::json out = nlohmann::json::array();
    if (guinessResults) {
      const auto [fName, bbName, basicBlockId, instIndex] = *guinessResults;
      out.push_back({
          {"function", fName},
          {"basic_block_name", bbName},
          {"basic_block_annotation_id", std::to_string(basicBlockId)},
          {"insn_index", instIndex},
      });
    }
    if (Vars.count("neck-placement")) {
      std::error_code EC;
      std::filesystem::path Path(Vars["neck-placement"].as<std::string>());
      llvm::raw_fd_ostream OF(Path.string(), EC);
      OF << nlohmann::to_string(out) << "\n";
      OF.close();
    }
    std::cout << nlohmann::to_string(out) << "\n";
    return EXIT_SUCCESS;
  }

  std::optional<std::string> CombinedModulePath = std::nullopt;
  std::optional<std::string> TapePath = std::nullopt;
  if (Vars.count("guiness-tape") > 0) {
    if (!Vars.count("combined-module")) {
      spdlog::error(
          "When using GuiNeSS the 'combined-module' option must be provided.");
      std::exit(EXIT_FAILURE);
    }
    CombinedModulePath = Vars["combined-module"].as<std::string>();

    if (!Vars.count("guiness-tape")) {
      spdlog::error(
          "When using GuiNeSS the 'guiness-tape' option must be provided.");
      std::exit(EXIT_FAILURE);
    }
    TapePath = Vars["guiness-tape"].as<std::string>();
  }

  if (!Vars.count("module")) {
    spdlog::error("Need to specify an LLVM (.ll/.bc) module for analysis.");
    return EXIT_SUCCESS;
  }

  if (!Vars.count("taint-config")) {
    spdlog::error("Need to specify a taint configuration to determine "
                  "potential neck candidates.");
    return EXIT_SUCCESS;
  }

  if (Vars.count("verbose") > 0) {
    spdlog::set_level(spdlog::level::info);
  }
  if (Vars.count("debug") > 0) {
    spdlog::set_level(spdlog::level::debug);
  }

  std::string EntryPoint("main");
  if (Vars.count("entry-point")) {
    EntryPoint = Vars["entry-point"].as<std::string>();
  }

  auto ModulePath = Vars["module"].as<std::string>();

  neckid::NeckVerification Verification(
      ModulePath, Vars["taint-config"].as<std::string>(),
      Vars.count("function-local-points-to-info-wo-globals"),
      Vars.count("use-simplified-dfa"), EntryPoint, CombinedModulePath,
      TapePath, Vars.count("debug"));

  if (Vars.count("write-participating-functions-cfg")) {
    auto path = Vars["write-participating-functions-cfg"].as<std::string>();
    auto &NA = Verification.getNeckAnalysis();
    auto &P = NA.getParticipatingFunctions();
    for (llvm::Function *func : P) {
      auto OutPath = path + "_" + std::string(func->getName()) + "_pfunc.dot";
      Verification.writeCFG(OutPath, ModulePath, func);
    }
  }

  auto results = Verification.getOutput();

  nlohmann::json out = nlohmann::json::array();
  if (results) {
    const auto [fName, bbName, basicBlockId, instIndex] = *results;
    out.push_back({
        {"function", fName},
        {"basic_block_name", bbName},
        {"basic_block_annotation_id", std::to_string(basicBlockId)},
        {"insn_index", instIndex},
    });
  }

  if (Vars.count("neck-placement")) {
    std::error_code EC;
    std::filesystem::path Path(Vars["neck-placement"].as<std::string>());
    llvm::raw_fd_ostream OF(Path.string(), EC);
    OF << nlohmann::to_string(out) << "\n";
    OF.close();
  }
  std::cout << nlohmann::to_string(out) << "\n";

  if (!results) {
    spdlog::error("Failed to select neck");
    return EXIT_FAILURE;
  }

  if (Vars.count("verify-neck")) {
    return verifyNeck(Vars, Verification, ModulePath, EntryPoint);
  }

  if (Vars.count("insert-neck")) {
    return insertNeck(Vars, Verification);
  }

  return EXIT_SUCCESS;
}
