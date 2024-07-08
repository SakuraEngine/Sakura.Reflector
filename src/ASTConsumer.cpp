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
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Path.h"
#include <vector>

namespace help {
void str_remove_all(std::string &str, const std::string &remove_str) {
  for (size_t i; (i = str.find(remove_str)) != std::string::npos;) {
    str.replace(i, remove_str.length(), "");
  }
}
void str_join(std::string &a, const std::string &b, const std::string_view sep = ",") {
  if (!a.empty()) {
    a += sep;
  }
  a += b;
}
std::string get_type_name(clang::QualType type, clang::ASTContext *ctx) {
  type = type.getCanonicalType();
  auto baseName = type.getAsString(ctx->getLangOpts());
  str_remove_all(baseName, "struct ");
  str_remove_all(baseName, "class ");
  return baseName;
}
std::string get_raw_type_name(clang::QualType type, clang::ASTContext *ctx) {
  if (type->isPointerType() || type->isReferenceType())
    type = type->getPointeeType();
  type = type.getUnqualifiedType();
  auto baseName = type.getAsString(ctx->getLangOpts());
  str_remove_all(baseName, "struct ");
  str_remove_all(baseName, "class ");
  return baseName;
}
std::string get_access_string(clang::AccessSpecifier access) {
  switch (access) {
  case clang::AS_public:
    return "public";
  case clang::AS_protected:
    return "protected";
  case clang::AS_private:
    return "private";
  case clang::AS_none:
    return "none";
  }
}
std::string get_comment(clang::Decl *decl, clang::ASTContext *ctx, clang::SourceManager &sm) {
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
std::string relative_path(const llvm::StringRef &root, const llvm::StringRef &path) {
  if (!path.startswith(root))
    return {};
  return path.substr(root.size()).str();
}
std::string parse_attr(clang::NamedDecl *decl) {
  std::string attr;
  for (auto annotate : decl->specific_attrs<clang::AnnotateAttr>()) {
    auto text = annotate->getAnnotation();
    if (text.equals("__reflect__"))
      continue;
    help::str_join(attr, text.str());
  }
  return attr;
};
bool has_reflect_flag(clang::NamedDecl *decl) {
  for (auto annotate : decl->specific_attrs<clang::AnnotateAttr>()) {
    auto text = annotate->getAnnotation();
    if (text.equals("__reflect__"))
      return true;
  }
  return false;
}
std::string get_decl_file_name(clang::Decl *decl, const clang::PresumedLoc &location) {
  using namespace clang;
  if (location.isValid()) {
    SmallString<2048> AbsolutePath(tooling::getAbsolutePath(location.getFilename()));
    llvm::sys::path::remove_dots(AbsolutePath, true);
    return llvm::sys::path::convert_to_slash(AbsolutePath.str());
  } else {
    return "";
  }
}
} // namespace help

class ParmVisitor : public clang::RecursiveASTVisitor<ParmVisitor> {
public:
  bool VisitParmVarDecl(clang::ParmVarDecl *param_decl) {
    // root decl is a parameter, we visit child depth
    if (param_decl == root_decl) {
      // initialize depth by the current depth
      depth = param_decl->getFunctionScopeDepth() + 1;
      return true;
    }

    // only visit parameters in the same scope
    if (param_decl->getFunctionScopeDepth() != depth)
      return true;

    meta::Field param_data;

    // comment & location
    param_data.comment = help::get_comment(param_decl, consumer->transition_unit_ctx(), consumer->transition_unit_ctx()->getSourceManager());
    param_data.line = consumer->transition_unit_ctx()->getSourceManager().getPresumedLineNumber(param_decl->getLocation());

    // parse parameter data
    param_data.name = param_decl->getNameAsString();
    if (param_data.name.empty()) {
      param_data.name = "unnamed" + std::to_string(param_decl->getFunctionScopeIndex());
      param_data.isAnonymous = true;
    }
    param_data.attrs = help::parse_attr(param_decl);

    // parse array data
    if (param_decl->getType()->isConstantArrayType()) {
      auto ftype = llvm::dyn_cast<clang::ConstantArrayType>(param_decl->getType());
      param_data.arraySize = ftype->getSize().getZExtValue();
      param_data.type = help::get_type_name(ftype->getElementType(), consumer->transition_unit_ctx());
      param_data.rawType = help::get_raw_type_name(ftype->getElementType(), consumer->transition_unit_ctx());
    } else {
      param_data.arraySize = 0;
      param_data.type = help::get_type_name(param_decl->getType(), consumer->transition_unit_ctx());
      param_data.rawType = help::get_raw_type_name(param_decl->getType(), consumer->transition_unit_ctx());
    }

    // recursive handle function pointer
    consumer->_fill_function_pointer(param_decl, param_data);

    // parameter unused data
    param_data.access = help::get_access_string(clang::AS_none);
    param_data.arraySize = 0;
    param_data.defaultValue = "";

    // add parameter
    parameters.emplace_back(std::move(param_data));
    return true;
  }

  // input
  meta::ASTConsumer *consumer; // 用于获取 transition unit 和 递归调用
  clang::Decl *root_decl;      // 根部的 decl, 用于避免重复访问根节点（作为函数参数时）

  std::vector<meta::Field> parameters; // 填充的参数列表
  int depth = 0;
};

namespace meta {
ASTConsumer::ASTConsumer(FileDataMap &datamap, std::string root)
    : _datamap(datamap) {
  _root = llvm::sys::path::convert_to_slash(root);
}

// override
void ASTConsumer::HandleTranslationUnit(ASTContext &ctx) {
  // cache transition unit ctx
  _transition_unit_ctx = &ctx;

  // each translation unit decl
  auto transition_unit_decl = ctx.getTranslationUnitDecl();
  for (auto decl_it = transition_unit_decl->decls_begin(); decl_it != transition_unit_decl->decls_end(); ++decl_it) {
    clang::NamedDecl *child_decl = llvm::dyn_cast<clang::NamedDecl>(*decl_it);
    if (child_decl) {
      switch (child_decl->getKind()) {
      case (clang::Decl::Namespace):
        handle_namespace(child_decl);
        break;
      case (clang::Decl::CXXRecord):
        handle_record(child_decl);
        break;
      case (clang::Decl::Function):
        handle_function(child_decl);
        break;
      case (clang::Decl::Enum):
        handle_enum(child_decl);
        break;
      case (clang::Decl::ClassTemplate):
        handle_template(child_decl);
        break;
      default:
        break;
      }
    }
  }
}

// root level parse functions
void ASTConsumer::handle_namespace(clang::NamedDecl *decl) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // each child decl
  clang::DeclContext *decl_ctx = decl->castToDeclContext(decl);
  for (auto decl_it = decl_ctx->decls_begin(); decl_it != decl_ctx->decls_end(); ++decl_it) {
    clang::NamedDecl *child_decl = llvm::dyn_cast<clang::NamedDecl>(*decl_it);
    if (child_decl) {
      switch (child_decl->getKind()) {
      case (clang::Decl::Namespace):
        handle_namespace(child_decl);
        break;
      case (clang::Decl::CXXRecord):
        handle_record(child_decl);
        break;
      case (clang::Decl::Function):
        handle_function(child_decl);
        break;
      case (clang::Decl::Enum):
        handle_enum(child_decl);
        break;
      case (clang::Decl::ClassTemplate):
        handle_template(child_decl);
        break;
      default:
        break;
      }
    }
  }
}
void ASTConsumer::handle_record(clang::NamedDecl *decl) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // filter location
  clang::SourceManager &source_manager = _transition_unit_ctx->getSourceManager();
  clang::PresumedLoc location = source_manager.getPresumedLoc(decl->getLocation());
  std::string abs_file_name;
  std::string rel_file_name;
  unsigned line;
  if (!_filter_decl_location(
          decl,
          location,
          abs_file_name,
          rel_file_name,
          line)) {
    return;
  }

  // filter parsed identity
  if (!_filter_parsed_identity(decl, abs_file_name, line)) {
    return;
  }

  // filter reflect flag
  if (!_filter_reflect_flag(decl)) {
    return;
  }

  // get file data base
  auto &db = _get_file_db(rel_file_name);

  // filter record
  clang::CXXRecordDecl *record_decl = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
  if (!record_decl) {
    return;
  }

  // filter forward declaration
  if (record_decl->isThisDeclarationADefinition() == clang::VarDecl::DeclarationOnly) {
    // ignore forward declaration attributes
    // LOG("attribute on forward declaration is ignored.");
    return;
  }

  // filter nested record
  {
    clang::DeclContext *parent = record_decl->getDeclContext();
    while (parent) {
      if (auto parentRecord = llvm::dyn_cast<clang::CXXRecordDecl>(parent)) {
        return;
      }
      parent = parent->getParent();
    }
  }

  // filter anonymous record
  if (record_decl->isAnonymousStructOrUnion()) {
    // LOG("attribute on anonymous record is ignored.");
    return;
  }

  // filter union
  if (record_decl->isUnion()) {
    // LOG("union is not fully supported, use at your own risk.");
    return;
  }

  Record record_data = {};

  // parse comment & location
  record_data.comment = help::get_comment(record_decl, _transition_unit_ctx, source_manager);
  record_data.fileName = abs_file_name;
  record_data.line = line;

  // parse record data
  record_data.name = record_decl->getQualifiedNameAsString();
  record_data.attrs = help::parse_attr(record_decl);
  for (auto base : record_decl->bases()) {
    record_data.bases.push_back(help::get_type_name(base.getType(), _transition_unit_ctx));
    // TODO. base info
    base.isVirtual();
    base.getAccessSpecifier();
  }

  // dispatch child decl
  for (auto child_decl : decl->castToDeclContext(decl)->decls()) {
    auto named_child_decl = llvm::dyn_cast<clang::NamedDecl>(child_decl);
    if (named_child_decl) {
      switch (named_child_decl->getKind()) {
      case (clang::Decl::Field):
        handle_field(named_child_decl, record_data.fields.emplace_back());
        break;
      case (clang::Decl::Var):
        handle_static_field(named_child_decl, record_data.fields.emplace_back());
        break;
      case (clang::Decl::CXXMethod):
        handle_method(named_child_decl, record_data.methods.emplace_back());
        break;
      case (clang::Decl::Function):
        handle_static_method(named_child_decl, record_data.methods.emplace_back());
        break;
      case (clang::Decl::Record):
        // nested record is not supported now
        break;
      default:
        break;
      }
    }
  }

  // push record
  _get_file_db(rel_file_name).records.emplace_back(std::move(record_data));
}
void ASTConsumer::handle_enum(clang::NamedDecl *decl) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // filter location
  clang::SourceManager &source_manager = _transition_unit_ctx->getSourceManager();
  clang::PresumedLoc location = source_manager.getPresumedLoc(decl->getLocation());
  std::string abs_file_name;
  std::string rel_file_name;
  unsigned line;
  if (!_filter_decl_location(
          decl,
          location,
          abs_file_name,
          rel_file_name,
          line)) {
    return;
  }

  // filter parsed identity
  if (!_filter_parsed_identity(decl, abs_file_name, line)) {
    return;
  }

  // filter reflect flag
  if (!_filter_reflect_flag(decl)) {
    return;
  }

  // filter enum
  clang::EnumDecl *enum_decl = llvm::dyn_cast<clang::EnumDecl>(decl);
  if (!enum_decl) {
    return;
  }

  Enum enum_data;

  // parse comment & location
  enum_data.comment = help::get_comment(enum_decl, _transition_unit_ctx, source_manager);
  enum_data.fileName = abs_file_name;
  enum_data.line = line;

  // parse enum data
  enum_data.name = enum_decl->getQualifiedNameAsString();
  enum_data.isScoped = enum_decl->isScoped();
  enum_data.underlying_type = enum_decl->isFixed()
                                  ? enum_decl->getIntegerType().getAsString(_transition_unit_ctx->getLangOpts())
                                  : "unfixed";
  enum_data.attrs = help::parse_attr(enum_decl);

  // parse enum item
  for (auto enumerator : enum_decl->enumerators()) {
    Enumerator enumerator_data;
    // parse comment & location
    enumerator_data.comment = help::get_comment(enumerator, _transition_unit_ctx, source_manager);
    enumerator_data.line = source_manager.getPresumedLineNumber(enumerator->getLocation());

    // parse enum item data
    enumerator_data.name = enumerator->getQualifiedNameAsString();
    enumerator_data.value = enumerator->getInitVal().getRawData()[0];
    enumerator_data.attrs = help::parse_attr(enumerator);

    // push enum item
    enum_data.values.push_back(std::move(enumerator_data));
  }

  // push enum
  _get_file_db(rel_file_name).enums.push_back(std::move(enum_data));
}
void ASTConsumer::handle_function(clang::NamedDecl *decl) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // filter location
  clang::SourceManager &source_manager = _transition_unit_ctx->getSourceManager();
  clang::PresumedLoc location = source_manager.getPresumedLoc(decl->getLocation());
  std::string abs_file_name;
  std::string rel_file_name;
  unsigned line;
  if (!_filter_decl_location(
          decl,
          location,
          abs_file_name,
          rel_file_name,
          line)) {
    return;
  }

  // filter parsed identity
  if (!_filter_parsed_identity(decl, abs_file_name, line)) {
    return;
  }

  // filter reflect flag
  if (!_filter_reflect_flag(decl)) {
    return;
  }

  // filter function
  clang::FunctionDecl *func_decl = llvm::dyn_cast<clang::FunctionDecl>(decl);
  if (!func_decl) {
    return;
  }

  Function func_data;

  // comment & location
  func_data.comment = help::get_comment(func_decl, _transition_unit_ctx, source_manager);
  func_data.fileName = abs_file_name;
  func_data.line = line;

  // parse function data
  _fill_function_data(func_decl, func_data);

  // unused function data
  func_data.access = help::get_access_string(func_decl->getAccess());
  func_data.isConst = false;

  // push function
  _get_file_db(rel_file_name).functions.push_back(std::move(func_data));
}
void ASTConsumer::handle_template(clang::NamedDecl *decl) {
  // unsupported now
  return;

  // get template decl
  CXXRecordDecl *template_decl = nullptr;
  if (auto templateDecl = llvm::dyn_cast<clang::ClassTemplateDecl>(decl)) {
    if (auto inner = templateDecl->getTemplatedDecl())
      template_decl = inner;
  }
}

// leaf level parse functions
void ASTConsumer::handle_method(clang::NamedDecl *decl, Function &out_method) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // filter location
  clang::SourceManager &source_manager = _transition_unit_ctx->getSourceManager();
  clang::PresumedLoc location = source_manager.getPresumedLoc(decl->getLocation());
  std::string abs_file_name;
  std::string rel_file_name;
  unsigned line;
  if (!_filter_decl_location(
          decl,
          location,
          abs_file_name,
          rel_file_name,
          line)) {
    return;
  }

  // filter parsed identity
  if (!_filter_parsed_identity(decl, abs_file_name, line)) {
    return;
  }

  // filter method
  clang::CXXMethodDecl *method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
  if (!method_decl) {
    return;
  }

  // comment & location
  out_method.comment = help::get_comment(method_decl, _transition_unit_ctx, source_manager);
  out_method.fileName = abs_file_name;
  out_method.line = line;

  // parse method data
  _fill_function_data(method_decl, out_method);

  // access & const
  out_method.access = help::get_access_string(method_decl->getAccess());
  out_method.isConst = method_decl->isConst();
}
void ASTConsumer::handle_static_method(clang::NamedDecl *decl, Function &out_method) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // filter location
  clang::SourceManager &source_manager = _transition_unit_ctx->getSourceManager();
  clang::PresumedLoc location = source_manager.getPresumedLoc(decl->getLocation());
  std::string abs_file_name;
  std::string rel_file_name;
  unsigned line;
  if (!_filter_decl_location(
          decl,
          location,
          abs_file_name,
          rel_file_name,
          line)) {
    return;
  }

  // filter parsed identity
  if (!_filter_parsed_identity(decl, abs_file_name, line)) {
    return;
  }

  // filter static method
  clang::FunctionDecl *func_decl = llvm::dyn_cast<clang::FunctionDecl>(decl);
  if (!func_decl) {
    return;
  }

  // comment & location
  out_method.comment = help::get_comment(func_decl, _transition_unit_ctx, source_manager);
  out_method.fileName = abs_file_name;
  out_method.line = line;

  // parse function data
  _fill_function_data(func_decl, out_method);

  // access & const
  out_method.access = help::get_access_string(func_decl->getAccess());
  out_method.isConst = false;
}
void ASTConsumer::handle_field(clang::NamedDecl *decl, Field &out_field) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // filter location
  clang::SourceManager &source_manager = _transition_unit_ctx->getSourceManager();
  clang::PresumedLoc location = source_manager.getPresumedLoc(decl->getLocation());
  std::string abs_file_name;
  std::string rel_file_name;
  unsigned line;
  if (!_filter_decl_location(
          decl,
          location,
          abs_file_name,
          rel_file_name,
          line)) {
    return;
  }

  // filter parsed identity
  if (!_filter_parsed_identity(decl, abs_file_name, line)) {
    return;
  }

  // filter field
  clang::FieldDecl *field_decl = llvm::dyn_cast<clang::FieldDecl>(decl);
  if (!field_decl) {
    return;
  }

  // comment & location
  out_field.comment = help::get_comment(field_decl, _transition_unit_ctx, source_manager);
  out_field.line = line;

  // parse field data
  out_field.name = field_decl->getNameAsString();
  out_field.attrs = help::parse_attr(field_decl);
  out_field.access = help::get_access_string(field_decl->getAccess());
  out_field.isStatic = false;

  // parse array data
  if (field_decl->getType()->isConstantArrayType()) {
    auto ftype =
        llvm::dyn_cast<clang::ConstantArrayType>(field_decl->getType());
    out_field.arraySize = ftype->getSize().getZExtValue();
    out_field.type = help::get_type_name(ftype->getElementType(), _transition_unit_ctx);
    out_field.rawType = help::get_raw_type_name(ftype->getElementType(), _transition_unit_ctx);
  } else {
    out_field.arraySize = 0;
    out_field.type = help::get_type_name(field_decl->getType(), _transition_unit_ctx);
    out_field.rawType = help::get_raw_type_name(field_decl->getType(), _transition_unit_ctx);
  }

  // default value
  if (field_decl->hasInClassInitializer()) {
    llvm::raw_string_ostream s(out_field.defaultValue);
    auto defArg = field_decl->getInClassInitializer();
    defArg->printPretty(s, nullptr, _transition_unit_ctx->getPrintingPolicy());
  }

  // handle if field is function pointer
  _fill_function_pointer(field_decl, out_field);
}
void ASTConsumer::handle_static_field(clang::NamedDecl *decl, Field &out_field) {
  // filter invalid decl
  if (decl->isInvalidDecl())
    return;

  // filter location
  clang::SourceManager &source_manager = _transition_unit_ctx->getSourceManager();
  clang::PresumedLoc location = source_manager.getPresumedLoc(decl->getLocation());
  std::string abs_file_name;
  std::string rel_file_name;
  unsigned line;
  if (!_filter_decl_location(
          decl,
          location,
          abs_file_name,
          rel_file_name,
          line)) {
    return;
  }

  // filter parsed identity
  if (!_filter_parsed_identity(decl, abs_file_name, line)) {
    return;
  }

  // filter static field
  clang::VarDecl *var_decl = llvm::dyn_cast<clang::VarDecl>(decl);
  if (!var_decl || !var_decl->isStaticDataMember()) {
    return;
  }

  // comment & location
  out_field.comment = help::get_comment(var_decl, _transition_unit_ctx, source_manager);
  out_field.line = line;

  // field data
  out_field.name = var_decl->getNameAsString();
  out_field.attrs = help::parse_attr(var_decl);
  out_field.access = help::get_access_string(var_decl->getAccess());
  out_field.isStatic = true;

  // parse array data
  if (var_decl->getType()->isConstantArrayType()) {
    auto ftype =
        llvm::dyn_cast<clang::ConstantArrayType>(var_decl->getType());
    out_field.arraySize = ftype->getSize().getZExtValue();
    out_field.type = help::get_type_name(ftype->getElementType(), _transition_unit_ctx);
    out_field.rawType = help::get_raw_type_name(ftype->getElementType(), _transition_unit_ctx);
  } else {
    out_field.arraySize = 0;
    out_field.type = help::get_type_name(var_decl->getType(), _transition_unit_ctx);
    out_field.rawType = help::get_raw_type_name(var_decl->getType(), _transition_unit_ctx);
  }

  // handle if field is function pointer
  _fill_function_pointer(var_decl, out_field);
}

// helper functions
bool ASTConsumer::_filter_decl_location(clang::NamedDecl *decl,
                                        const clang::PresumedLoc &location,
                                        std::string &out_abs_file_name,
                                        std::string &out_rel_file_name,
                                        unsigned &out_line) {
  // filter location
  if (location.isInvalid()) {
    // llvm::outs() << "[No Location]";
    // decl->printName(llvm::outs());
    // llvm::outs() << "\n";
    return false;
  }

  // location info
  out_abs_file_name = help::get_decl_file_name(decl, location);
  out_rel_file_name = help::relative_path(_root, out_abs_file_name);
  out_line = location.getLine();

  // filter files not under root
  if (out_rel_file_name.empty()) {
    // llvm::outs() << "[No Filename]";
    // decl->printName(llvm::outs());
    // llvm::outs() << "\n";
    return false;
  }
  return true;
}
bool ASTConsumer::_filter_parsed_identity(clang::NamedDecl *decl, const std::string &file_name, unsigned line) {
  Identity ident;
  ident.fileName = file_name;
  ident.line = line;
  bool is_parsed = _parsed.find(ident) != _parsed.end();
  if (is_parsed && decl->getKind() != clang::Decl::Namespace) {
    return false;
  }
  _parsed.insert(ident);
  return true;
}
bool ASTConsumer::_filter_reflect_flag(clang::NamedDecl *decl) {
  bool has_reflect_entry = help::has_reflect_flag(decl);
  switch (decl->getKind()) {
  case (clang::Decl::CXXRecord):
  case (clang::Decl::Enum): // need reflect flag
    if (!has_reflect_entry) {
      return false;
    }
    break;
  case (clang::Decl::ClassTemplate):
  case (clang::Decl::ClassTemplateSpecialization):
  case (clang::Decl::FunctionTemplate): // current unsupported
    return false;
  default: // 其它情况不影响性能，可以不做过滤
    break;
  }
  return true;
}
Database &ASTConsumer::_get_file_db(const std::string &rel_file_name) {
  return _datamap[rel_file_name];
}
void ASTConsumer::_fill_function_data(clang::FunctionDecl *func_decl, Function &out_func_data) {
  // parse function data
  out_func_data.name = func_decl->getQualifiedNameAsString();
  out_func_data.isStatic = func_decl->isStatic();
  auto func_proto_type = func_decl->getType()->getAs<clang::FunctionProtoType>();
  out_func_data.isNothrow = func_proto_type ? func_proto_type->isNothrow() : false;
  out_func_data.attrs = help::parse_attr(func_decl);
  if (!func_decl->isNoReturn()) {
    out_func_data.retType = help::get_type_name(func_decl->getReturnType(), _transition_unit_ctx);
    out_func_data.rawRetType = help::get_raw_type_name(func_decl->getReturnType(), _transition_unit_ctx);
  }

  // parse parameters
  for (auto param : func_decl->parameters()) {
    Field param_data;

    // comment & location
    param_data.comment = help::get_comment(param, _transition_unit_ctx, _transition_unit_ctx->getSourceManager());
    param_data.line = _transition_unit_ctx->getSourceManager().getPresumedLineNumber(param->getLocation());

    // parse parameter data
    param_data.name = param->getNameAsString();
    if (param_data.name.empty()) {
      param_data.name = "unnamed" + std::to_string(out_func_data.parameters.size());
      param_data.isAnonymous = true;
    }
    param_data.attrs = help::parse_attr(param);

    // parse array data
    if (param->getType()->isConstantArrayType()) {
      auto ftype = llvm::dyn_cast<clang::ConstantArrayType>(param->getType());
      param_data.arraySize = ftype->getSize().getZExtValue();
      param_data.type = help::get_type_name(ftype->getElementType(), _transition_unit_ctx);
      param_data.rawType = help::get_raw_type_name(ftype->getElementType(), _transition_unit_ctx);
    } else {
      param_data.arraySize = 0;
      param_data.type = help::get_type_name(param->getType(), _transition_unit_ctx);
      param_data.rawType = help::get_raw_type_name(param->getType(), _transition_unit_ctx);
    }

    // parse default value
    if (param->hasDefaultArg()) {
      llvm::raw_string_ostream s(param_data.defaultValue);
      if (param->hasUninstantiatedDefaultArg()) {
        auto defArg = param->getUninstantiatedDefaultArg();
        defArg->printPretty(s, nullptr, _transition_unit_ctx->getPrintingPolicy());
      } else {
        auto defArg = param->getDefaultArg();
        defArg->printPretty(s, nullptr, _transition_unit_ctx->getPrintingPolicy());
      }
    }

    // parameter unused data
    param_data.access = help::get_access_string(clang::AS_none);

    // handle if function pointer
    _fill_function_pointer(param, param_data);

    // push parameter
    out_func_data.parameters.push_back(std::move(param_data));
  }
}
void ASTConsumer::_fill_function_pointer(clang::DeclaratorDecl *decl, Field &out_field) {
  // init
  clang::Decl *signature_decl = decl;
  clang::QualType signature_type = decl->getType();

  // decode typedef
  {
    auto typedef_type = signature_type->getAs<clang::TypedefType>();
    if (typedef_type) {
      signature_decl = typedef_type->getDecl();
      signature_type = typedef_type->getDecl()->getUnderlyingType();
    }
  }

  // decode std::function
  // TODO. remove it
  bool is_functor = false;
  {
    auto template_specialization_type = signature_type->getAs<clang::TemplateSpecializationType>();
    if (template_specialization_type) {
      auto arguments = template_specialization_type->template_arguments();
      if (arguments.size() == 0) {
        return;
      }
      if (arguments[0].getKind() != clang::TemplateArgument::Type) {
        return;
      }

      // update signature
      signature_type = arguments[0].getAsType();
      is_functor = true;
    }
  }

  // decode typedef again for functor
  // TODO. remove it
  {
    auto typedef_type = signature_type->getAs<clang::TypedefType>();
    if (typedef_type) {
      signature_decl = typedef_type->getDecl();
      signature_type = typedef_type->getDecl()->getUnderlyingType();
    }
  }

  // decay signature
  if (signature_type->isFunctionPointerType()) {
    signature_type = signature_type->castAs<clang::PointerType>()->getPointeeType();
  } else if (signature_type->isFunctionReferenceType()) {
    signature_type = signature_type->castAs<clang::ReferenceType>()->getPointeeType();
  } else if (signature_type->isConstantArrayType()) {
    signature_type = signature_type->getAsArrayTypeUnsafe()->getElementType();
  }

  // final type
  auto final_func_type = signature_type->getAs<clang::FunctionType>();
  if (!final_func_type) {
    return;
  }

  // visit parameters
  ParmVisitor param_visitor;
  param_visitor.consumer = this;
  param_visitor.root_decl = signature_decl;
  param_visitor.TraverseDecl(signature_decl);

  // fill field function info
  out_field.isFunctor = is_functor;
  out_field.isCallback = true;

  // signature comment & location
  out_field.signature.comment = help::get_comment(signature_decl, transition_unit_ctx(), transition_unit_ctx()->getSourceManager());
  out_field.signature.fileName = help::get_decl_file_name(signature_decl, transition_unit_ctx()->getSourceManager().getPresumedLoc(signature_decl->getLocation()));
  out_field.signature.line = transition_unit_ctx()->getSourceManager().getPresumedLineNumber(signature_decl->getLocation());

  // fill signature data
  out_field.signature.name = out_field.name;
  out_field.signature.attrs = out_field.attrs;
  auto func_proto_type = final_func_type->getAs<clang::FunctionProtoType>();
  out_field.signature.isNothrow = func_proto_type ? func_proto_type->isNothrow() : false;
  out_field.signature.retType = help::get_type_name(final_func_type->getReturnType(), transition_unit_ctx());
  out_field.signature.rawRetType = help::get_raw_type_name(final_func_type->getReturnType(), transition_unit_ctx());

  // fill parameters
  out_field.signature.parameters = std::move(param_visitor.parameters);

  // signature unused data
  out_field.signature.access = help::get_access_string(clang::AS_none);
  out_field.signature.isStatic = true;
  out_field.signature.isConst = false;
}
} // namespace meta