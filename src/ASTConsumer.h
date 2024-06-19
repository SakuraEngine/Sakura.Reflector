#pragma once

#include "meta.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecordLayout.h"
#include <unordered_set>

#define LOG(...)
#define LOG_PUSH_IDENT
#define LOG_POP_IDENT

namespace meta {
using namespace clang;

class ASTConsumer : public clang::ASTConsumer {
public:
  ASTConsumer(FileDataMap &datamap, std::string root)
      : datamap(datamap), root(root) {}
  void HandleTranslationUnit(ASTContext &ctx) override;
  ASTContext *GetContext() { return _ASTContext; }

  // helper functions
  void HandleFunctionPointer(clang::DeclaratorDecl *decl, meta::Field &field);
  void HandleDecl(clang::NamedDecl *decl, std::vector<std::string> &attrStack, Record *record, const clang::ASTRecordLayout *layout);

protected:
  FileDataMap &datamap;
  std::string root;
  std::unordered_set<meta::Identity, meta::IdentityHash> parsed;
  ASTContext *_ASTContext;
};
} // namespace meta