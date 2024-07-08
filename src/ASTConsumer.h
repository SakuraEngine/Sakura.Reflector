#pragma once

#include "meta.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecordLayout.h"
#include <unordered_set>

#define LOG(...)

namespace meta {
using namespace clang;

class ASTConsumer : public clang::ASTConsumer {
public:
  ASTConsumer(FileDataMap &datamap, std::string root)
      : datamap(datamap), root(root) {}

  // override
  void HandleTranslationUnit(ASTContext &ctx) override;

  // getter
  ASTContext *transition_unit_ctx() { return _transition_unit_ctx; }

  // parse functions
  void HandleFunctionPointer(clang::DeclaratorDecl *decl, meta::Field &field);
  void HandleDecl(clang::NamedDecl *decl, Record *record, const clang::ASTRecordLayout *layout);

protected:
  // config
  FileDataMap &datamap;
  std::string root;

  // 跳过前置声明的重复解析
  std::unordered_set<meta::Identity, meta::IdentityHash> parsed;

  // 编译单元的节点
  ASTContext *_transition_unit_ctx;
};
} // namespace meta