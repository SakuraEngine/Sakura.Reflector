#include "ASTConsumer.h"
#include "meta.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attrs.inc"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Path.h"
#include <vector>

void meta::ASTConsumer::HandleTranslationUnit(ASTContext &ctx) {
  _ASTContext = &ctx;
  auto tuDecl = ctx.getTranslationUnitDecl();
  for (clang::DeclContext::decl_iterator i = tuDecl->decls_begin();
       i != tuDecl->decls_end(); ++i) {
    clang::NamedDecl *named_decl = llvm::dyn_cast<clang::NamedDecl>(*i);
    if (named_decl == 0)
      continue;

    // Filter out unsupported decls at the global namespace level
    clang::Decl::Kind kind = named_decl->getKind();
    std::vector<std::string> newStack;
    switch (kind) {
    case (clang::Decl::Namespace):
    case (clang::Decl::CXXRecord):
    case (clang::Decl::Function):
    case (clang::Decl::Enum):
    case (clang::Decl::ClassTemplate):
      HandleDecl(named_decl, newStack, PAR_NoReflect, nullptr, nullptr);
      break;
    default:
      break;
    }
  }
}

void Remove(std::string &str, const std::string &remove_str) {
  for (size_t i; (i = str.find(remove_str)) != std::string::npos;)
    str.replace(i, remove_str.length(), "");
}

void Join(std::string &a, const std::string &b) {
  if (!a.empty())
    a += ",";
  a += b;
}

std::string GetTypeName(clang::QualType type, clang::ASTContext *ctx) {
  type = type.getCanonicalType();
  auto baseName = type.getAsString(ctx->getLangOpts());
  Remove(baseName, "struct ");
  Remove(baseName, "class ");
  return baseName;
}

std::string GetRawTypeName(clang::QualType type, clang::ASTContext *ctx) {
  if (type->isPointerType() || type->isReferenceType())
    type = type->getPointeeType();
  type = type.getUnqualifiedType();
  auto baseName = type.getAsString(ctx->getLangOpts());
  Remove(baseName, "struct ");
  Remove(baseName, "class ");
  return baseName;
}

std::string ParseLeafAttribute(clang::NamedDecl *decl,
                               std::vector<std::string> &attrStack) {
  std::string attr;
  for (auto annotate : decl->specific_attrs<clang::AnnotateAttr>()) {
    auto text = annotate->getAnnotation();
    if (text.startswith("__push__")) {
      auto pushText = text.substr(sizeof("__push__"));
      attrStack.push_back(pushText.str());
    } else if (text.equals("__pop__"))
      attrStack.pop_back();
    else
      Join(attr, text.str());
  }
  for (auto &pattr : attrStack) {
    if (!attr.empty())
      attr += ", ";
    attr += pattr;
  }
  return attr;
}

std::string GetComment(clang::Decl *decl, clang::ASTContext *ctx,
                       clang::SourceManager &sm) {
  using namespace clang;
  std::string comment;
  const RawComment *rc = ctx->getRawCommentForDeclNoCache(decl);
  if (rc) {
    SourceRange range = rc->getSourceRange();

    PresumedLoc startPos = sm.getPresumedLoc(range.getBegin());
    PresumedLoc endPos = sm.getPresumedLoc(range.getEnd());

    comment = rc->getBriefText(*ctx);
  }
  return comment;
}

static llvm::SmallString<64>
calculateRelativeFilePath(const llvm::StringRef &Path,
                          const llvm::StringRef &CurrentPath) {
  if (!CurrentPath.startswith(Path))
    return {};
  return CurrentPath.substr(Path.size());
}

void meta::ASTConsumer::HandleDecl(clang::NamedDecl *decl,
                                   std::vector<std::string> &attrStack,
                                   ParseBehavior behavior, Record *record,
                                   const clang::ASTRecordLayout *layout) {
  if (decl->isInvalidDecl())
    return;
  clang::Decl::Kind kind = decl->getKind();
  std::string attr;
  clang::NamedDecl *attrDecl = decl;
  std::string fileName;
  std::string absPath;
  {
    Identity ident;
    auto location =
        _ASTContext->getSourceManager().getPresumedLoc(decl->getLocation());

    if (!location.isInvalid()) {
      SmallString<1024> AbsolutePath(
          tooling::getAbsolutePath(location.getFilename()));
      llvm::sys::path::remove_dots(AbsolutePath, true);
      fileName = absPath =
          llvm::sys::path::convert_to_slash(AbsolutePath.str());
      root = llvm::sys::path::convert_to_slash(root);
      fileName = calculateRelativeFilePath(root, fileName).str().str();
      if (fileName.empty())
        return;
      ident.fileName = absPath;
      ident.line = location.getLine();
    } else {
      return;
    }
    auto iter = parsed.find(ident);
    if (iter != parsed.end())
      return;
    parsed.insert(ident);
  }
  auto &db = datamap[fileName];
  if (auto templateDecl = llvm::dyn_cast<clang::ClassTemplateDecl>(decl)) {
    if (auto inner = templateDecl->getTemplatedDecl())
      attrDecl = inner;
  }
  ParseBehavior childBehavior =
      behavior == PAR_Normal ? PAR_NoReflect : behavior;
  for (auto annotate : attrDecl->specific_attrs<clang::AnnotateAttr>()) {
    auto text = annotate->getAnnotation();
    if (text.equals("__noreflect__"))
      return;
    if (text.equals("__reflect__"))
      behavior = childBehavior = PAR_Normal;
    else if (text.equals("__full_reflect__"))
      behavior = childBehavior = PAR_Reflect;
    else if (text.startswith("__push__")) {
      auto pushText = text.substr(sizeof("__push__") - 1);
      attrStack.push_back(pushText.str());
    } else if (text.equals("__pop__"))
      attrStack.pop_back();
    else
      Join(attr, text.str());
  }
  if (behavior == PAR_NoReflect)
    return;

  switch (kind) {
  case (clang::Decl::Namespace):
  case (clang::Decl::CXXRecord):
  case (clang::Decl::ClassTemplate):
    if (childBehavior == PAR_NoReflect)
      return;
  default:
    break;
  }
  for (auto &pattr : attrStack) {
    if (!attr.empty())
      attr += ", ";
    attr += pattr;
  }
  auto &sm = _ASTContext->getSourceManager();
  auto location = sm.getPresumedLoc(decl->getLocation());
  auto comment = GetComment(decl, _ASTContext, sm);
  switch (kind) {
  case (clang::Decl::Namespace): {
    clang::DeclContext *declContext = decl->castToDeclContext(decl);
    std::vector<std::string> newStack;
    for (clang::DeclContext::decl_iterator i = declContext->decls_begin();
         i != declContext->decls_end(); ++i) {
      clang::NamedDecl *named_decl = llvm::dyn_cast<clang::NamedDecl>(*i);
      if (named_decl)
        HandleDecl(named_decl, newStack, behavior, nullptr, nullptr);
    }
    return;
  } break;
  case (clang::Decl::CXXRecord): {
    clang::CXXRecordDecl *recordDecl =
        llvm::dyn_cast<clang::CXXRecordDecl>(decl);
    if (!recordDecl || recordDecl->isThisDeclarationADefinition() ==
                           clang::VarDecl::DeclarationOnly) {
      LOG("attribute on forward declaration is ignored.");
      return;
    }
    Record newRecord;
    newRecord.comment = comment;
    if (!recordDecl->isAnonymousStructOrUnion()) {
      newRecord.name = recordDecl->getQualifiedNameAsString();
      newRecord.fileName = absPath;
      newRecord.line = location.getLine();
      newRecord.attrs = attr;
      for (auto base : recordDecl->bases())
        newRecord.bases.push_back(GetTypeName(base.getType(), _ASTContext));
      record = &newRecord;
    } else if (!attr.empty()) {
      LOG("attribute on anonymous record is ignored.");
    }
    if (recordDecl->isUnion()) {
      LOG("union is not fully supported, use at your own risk.");
    }

    clang::DeclContext *declContext = decl->castToDeclContext(decl);
    std::vector<std::string> newStack;
    for (clang::DeclContext::decl_iterator i = declContext->decls_begin();
         i != declContext->decls_end(); ++i) {
      clang::NamedDecl *namedDecl = llvm::dyn_cast<clang::NamedDecl>(*i);
      if (!namedDecl)
        continue;
      HandleDecl(namedDecl, newStack, childBehavior, &newRecord,
                 &_ASTContext->getASTRecordLayout(recordDecl));
    }

    if (!recordDecl->isAnonymousStructOrUnion()) {
      db.records.push_back(std::move(newRecord));
    }
  } break;
  case (clang::Decl::Enum): {
    clang::EnumDecl *enumDecl = llvm::dyn_cast<clang::EnumDecl>(decl);
    if (!enumDecl)
      return;
    Enum newEnum;
    newEnum.comment = comment;
    newEnum.name = enumDecl->getQualifiedNameAsString();
    newEnum.fileName = absPath;
    newEnum.line = location.getLine();
    newEnum.attrs = attr;
    newEnum.underlying_type =
        enumDecl->isFixed()
            ? enumDecl->getIntegerType().getAsString(_ASTContext->getLangOpts())
            : "unfixed";
    std::vector<std::string> newStack;
    for (auto enumerator : enumDecl->enumerators()) {
      Enumerator newEnumerator;
      newEnumerator.comment = GetComment(enumerator, _ASTContext, sm);
      newEnumerator.name = enumerator->getQualifiedNameAsString();
      newEnumerator.attrs = ParseLeafAttribute(enumerator, newStack);
      newEnumerator.value = enumerator->getInitVal().getRawData()[0];
      newEnumerator.line = sm.getPresumedLineNumber(enumerator->getLocation());
      newEnum.values.push_back(newEnumerator);
    }
    db.enums.push_back(std::move(newEnum));
  } break;
  case (clang::Decl::Function):
  case (clang::Decl::CXXMethod): {
    clang::FunctionDecl *functionDecl =
        llvm::dyn_cast<clang::FunctionDecl>(decl);
    if (!functionDecl)
      return;
    Function newFunction;
    newFunction.comment = comment;
    newFunction.isStatic = functionDecl->isStatic();
    newFunction.name = functionDecl->getQualifiedNameAsString();
    if (!location.isInvalid()) {
      newFunction.fileName = absPath;
      newFunction.line = location.getLine();
    }
    newFunction.attrs = attr;
    if (auto methodDecl = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
      newFunction.isConst = methodDecl->isConst();
    } else {
      newFunction.isConst = false;
    }
    if (!functionDecl->isNoReturn()) {
      newFunction.retType =
          GetTypeName(functionDecl->getReturnType(), _ASTContext);
      newFunction.rawRetType =
          GetRawTypeName(functionDecl->getReturnType(), _ASTContext);
    }
    std::vector<std::string> newStack;
    for (auto param : functionDecl->parameters()) {
      Field newField;
      newField.comment = GetComment(param, _ASTContext, sm);
      newField.attrs = ParseLeafAttribute(param, newStack);
      newField.name = param->getNameAsString();
      newField.type = GetTypeName(param->getType(), _ASTContext);
      newField.rawType = GetRawTypeName(param->getType(), _ASTContext);
      newField.line = location.getLine();
      newField.offset = 0;
      newFunction.parameters.push_back(std::move(newField));
    }
    if (record)
      record->methods.push_back(std::move(newFunction));
    else
      db.functions.push_back(std::move(newFunction));
  }
  case (clang::Decl::Field): {
    clang::FieldDecl *fieldDecl = llvm::dyn_cast<clang::FieldDecl>(decl);
    if (!fieldDecl)
      return;
    Field newField;
    newField.comment = comment;
    newField.attrs = attr;
    newField.name = fieldDecl->getNameAsString();
    newField.line = location.getLine();
    if(fieldDecl->getType()->isConstantArrayType())
    {
      auto ftype = llvm::dyn_cast<clang::ConstantArrayType>(fieldDecl->getType());
      newField.arraySize = ftype->getSize().getZExtValue();
      newField.type = GetTypeName(ftype->getElementType(), _ASTContext);
      newField.rawType = GetRawTypeName(ftype->getElementType(), _ASTContext);
    }
    else
    {
      newField.type = GetTypeName(fieldDecl->getType(), _ASTContext);
      newField.rawType = GetRawTypeName(fieldDecl->getType(), _ASTContext);
    }
    if (record) {
      newField.offset = layout->getFieldOffset(fieldDecl->getFieldIndex()) / 8;
      record->fields.push_back(std::move(newField));
    } else
      LOG("field without record founded.");
  } break;
  case (clang::Decl::Var): {
    clang::VarDecl *varDecl = llvm::dyn_cast<clang::VarDecl>(decl);
    if (!varDecl || !varDecl->isStaticDataMember())
      return;
    Field newField;
    newField.comment = comment;
    newField.attrs = attr;
    newField.name = varDecl->getNameAsString();
    newField.type = GetTypeName(varDecl->getType(), _ASTContext);
    newField.line = location.getLine();
    newField.offset = 0;
    if (record)
      record->statics.push_back(std::move(newField));
    else
      LOG("field without record founded.");
  } break;
  case (clang::Decl::ClassTemplate): {
    // TODO
  } break;
  default:
    break;
  }
}