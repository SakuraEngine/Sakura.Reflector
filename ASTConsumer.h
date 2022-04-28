#pragma once

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "meta.h"

#define LOG(...)
#define LOG_PUSH_IDENT
#define LOG_POP_IDENT

namespace meta
{
    using namespace clang;

    enum ParseBehavior
    {
        PAR_Normal,
        PAR_Reflect,
        PAR_NoReflect
    };
    class ASTConsumer : public clang::ASTConsumer
    {
    public:
        ASTConsumer(Database& db) :
            db(db) {}
        void HandleTranslationUnit(ASTContext &ctx) override;
    protected:
        Database& db;
        ASTContext* _ASTContext;
        void HandleDecl(clang::NamedDecl* decl, std::vector<std::string>& attrStack, ParseBehavior behavior, Record* record);
    };
}