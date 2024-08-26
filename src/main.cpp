// Declares clang::SyntaxOnlyAction.
#include "clang/AST/Decl.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// Declares llvm::cl::extrahelp.
#include "ASTConsumer.h"
#include "OptionsParser.h"
#include "meta.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TimeProfiler.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>

namespace tooling = clang::tooling;
// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory ToolCategoryOption("meta options");
static llvm::cl::cat ToolCategory(ToolCategoryOption);

// command args
static llvm::cl::opt<std::string> Output(
    "output", llvm::cl::Required,
    llvm::cl::desc("Specify database output directory, depending on extension"),
    ToolCategory, llvm::cl::value_desc("directory"));
static llvm::cl::opt<std::string>
    Root("root", llvm::cl::Required,
         llvm::cl::desc("Specify parse root directory"), ToolCategory,
         llvm::cl::value_desc("directory"));

// new command args
// static llvm::cl::opt<std::string> Config(
//     "config", llvm::cl::Required,
//     llvm::cl::desc("Specify config file"),
//     ToolCategory, llvm::cl::value_desc("file path"));
// static llvm::cl::opt<std::string> Outdir(
//     "outdir", llvm::cl::Required,
//     llvm::cl::desc("Specify database output directory, depending on extension"),
//     ToolCategory, llvm::cl::value_desc("directory"));

// custom action
static meta::FileDataMap data_map;
class ReflectFrontendAction : public clang::ASTFrontendAction {
public:
  ReflectFrontendAction() {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef file) {
    // fronted opts
    auto &FO = compiler.getFrontendOpts();
    FO.SkipFunctionBodies = true;
    FO.ProgramAction = clang::frontend::ParseSyntaxOnly;

    // lang opts
    auto &LO = compiler.getLangOpts();
    LO.CommentOpts.ParseAllComments = true;

    return std::make_unique<meta::ASTConsumer>(data_map, Root);
  }
};

int main(int argc, const char **argv) {
  // copy args
  std::vector<const char *> args{};
  for (int i = 0; i < argc; ++i) {
    args.push_back(argv[i]);
  }

  // meta def
  args.insert(args.begin() + 1, "--extra-arg=-D__meta__");
  argc = args.size();

  // parse args
  auto ExpectedParser = meta::OptionsParser::create(
      argc,
      args.data(),
      llvm::cl::ZeroOrMore,
      ToolCategoryOption);
  if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  meta::OptionsParser &OptionsParser = ExpectedParser.get();

  // init time trace
  timeTraceProfilerInitialize(32, llvm::StringRef{args[0]});

  // run tool
  // auto start = std::chrono::high_resolution_clock::now();
  tooling::ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  int result = Tool.run(tooling::newFrontendActionFactory<ReflectFrontendAction>().get());
  // auto end = std::chrono::high_resolution_clock::now();
  // std::cout << "[" << Root << "]\n"
  //           << "Elapsed time: "
  //           << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
  //           << "ms\n";

  // serialize
  std::string OutPath;
  OutPath = Output;
  for (auto &pair : data_map) {
    using namespace llvm;
    if (pair.second.is_empty())
      continue;

    // replace extension to .h.meta
    SmallString<1024> MetaPath(OutPath + pair.first);
    sys::path::replace_extension(MetaPath, ".h.meta");

    // create meta dir
    SmallString<1024> MetaDir = MetaPath;
    sys::path::remove_filename(MetaDir);
    llvm::sys::fs::create_directories(MetaDir);

    // write meta file
    std::ofstream of(MetaPath.str().str());
    of << meta::serialize(pair.second);
  }

  // output time trace
  {
    llvm::SmallString<1024> time_trace_path(OutPath);
    llvm::sys::path::append(time_trace_path, "meta_time.json");
    std::error_code ec;
    llvm::raw_fd_stream fs(time_trace_path, ec);
    llvm::timeTraceProfilerWrite(fs);
    fs.close();
  }

  return result;
}