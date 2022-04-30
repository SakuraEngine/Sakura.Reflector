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
#include <fstream>
#include <memory>

namespace tooling = clang::tooling;
// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory ToolCategoryOption("meta options");
static llvm::cl::cat ToolCategory(ToolCategoryOption);

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp
    CommonHelp(tooling::CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp MoreHelp("\nMore help text...\n");

static llvm::cl::opt<std::string> Output(
    "output",
    llvm::cl::desc("Specify database output file, depending on extension"),
    ToolCategory, llvm::cl::value_desc("filename"));

static meta::Database db;
class ReflectFrontendAction : public clang::ASTFrontendAction {
public:
  ReflectFrontendAction() {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef file) {
    auto &LO = compiler.getLangOpts();
    LO.CommentOpts.ParseAllComments = true;
    return std::make_unique<meta::ASTConsumer>(db);
  }
};

int main(int argc, const char **argv) {
  std::vector<const char *> args(argc + 1);
  args[0] = argv[0];
  for (int i = 1; i < argc; ++i)
    args[i + 1] = argv[i];
  args[1] = "--extra-arg=-D__meta__";
  argc += 1;
  auto ExpectedParser = meta::OptionsParser::create(
      argc, args.data(), llvm::cl::ZeroOrMore, ToolCategoryOption);
  if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  meta::OptionsParser &OptionsParser = ExpectedParser.get();
  tooling::ClangTool Tool(OptionsParser.getCompilations(),
                          OptionsParser.getSourcePathList());
  int result = Tool.run(
      tooling::newFrontendActionFactory<ReflectFrontendAction>().get());
  std::string OutPath;
  if (Output.empty())
    OutPath = OptionsParser.getSourceDirectory() + "/meta.json";
  else
    OutPath = Output;
  std::ofstream of(OutPath);
  of << meta::serialize(db);
  return result;
}