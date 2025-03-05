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
  std::string name;
  std::string access = "none";
  
  bool is_static;
  bool is_const;
  bool is_nothrow;
  
  std::string ret_type;
  std::string raw_ret_type;
  std::vector<struct Field> parameters;
  
  std::string comment;
  std::string file_name;
  int line;

  std::string attrs;
};

struct Field {
  std::string name;
  std::string access = "none";
  std::string type;
  std::string raw_type;
  
  size_t array_size = 0;
  std::string default_value;
  bool is_functor = false;
  bool is_callback = false;
  bool is_anonymous = false;
  bool is_static = false;
  Function signature;
  
  std::string comment;
  int line;
  
  std::string attrs;
};

struct Record {
  std::string name;

  bool is_nested;
  std::vector<std::string> bases;
  std::vector<Field> fields;
  std::vector<Function> methods;
  
  std::string file_name;
  std::string comment;
  int line;
  
  std::string attrs;
};

struct Enumerator {
  std::string name;

  uint64_t value;
  
  std::string comment;
  int line;
  
  std::string attrs;
};

struct Enum {
  std::string name;
  
  std::string underlying_type;
  bool is_scoped;
  std::vector<Enumerator> values;
  
  std::string file_name;
  std::string comment;
  int line;
  
  std::string attrs;
};

struct Identity {
  std::string fileName;
  unsigned line;
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
  bool is_empty() {
    return records.empty() && functions.empty() && enums.empty();
  }
};

using FileDataMap = std::unordered_map<std::string, Database>;

std::string serialize(const Database &P);
} // namespace meta