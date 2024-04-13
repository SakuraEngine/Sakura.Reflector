#pragma once

#include "meta.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "llvm/Support/JSON.h"
#include <algorithm>
#include <string>
#include <unordered_map>

namespace meta {

struct Function {
  bool isStatic;
  bool isConst;
  bool isNothrow;
  std::string name;
  std::string attrs;
  std::string comment;
  std::vector<struct Field> parameters;
  std::string retType;
  std::string rawRetType;
  std::string access = "none";
  std::string fileName;
  int line;
};

struct Field {
  std::string name;
  std::string type;
  std::string rawType;
  std::string attrs;
  std::string comment;
  std::string defaultValue;
  size_t arraySize = 0;
  size_t offset;
  bool isFunctor = false;
  bool isCallback = false;
  bool isAnonymous = false;
  Function signature;
  int line;
  std::string access = "none";
};

struct Record {
  std::vector<std::string> bases;
  std::string name;
  std::string attrs;
  std::string comment;
  std::vector<Field> fields;
  std::vector<Field> statics;
  std::vector<Function> methods;
  std::string fileName;
  int line;
  bool isNested;
};

struct Enumerator {
  std::string name;
  std::string attrs;
  uint64_t value;
  std::string comment;
  int line;
};

struct Enum {
  std::string name;
  bool isScoped;
  std::string underlying_type;
  std::string attrs;
  std::vector<Enumerator> values;
  std::string fileName;
  std::string comment;
  int line;
};

struct Identity {
  std::string fileName;
  int line;
  bool operator==(const Identity &other) const {
    return fileName == other.fileName && line == other.line;
  }
};

struct IdentityHash {
  size_t operator()(const Identity &id) const {
    return std::hash<std::string>{}(id.fileName) + id.line;
  }
};

struct Database {
  std::vector<Record> records;
  std::vector<Function> functions;
  std::vector<Enum> enums;
  std::string attrs;
  bool is_empty() {
    return records.empty() && functions.empty() && enums.empty() &&
           attrs.empty();
  }
};

using FileDataMap = std::unordered_map<std::string, Database>;

std::string serialize(const Database &P);
} // namespace meta