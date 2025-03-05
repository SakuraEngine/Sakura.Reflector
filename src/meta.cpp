#include "meta.h"
#include "llvm/Support/JSON.h"

namespace meta {
void serializeAttr(llvm::json::OStream &J, const std::vector<std::string> &attr) {
  J.attributeArray("attrs", [&](){
    for (auto& a : attr) {
      J.value("{" + a + "}");
    }
  });
}

void serialize(llvm::json::OStream &J, const Function &P, bool method);

void serialize(llvm::json::OStream &J, const Field &P) {
  J.attributeObject(P.name, [&] {
    J.attribute("type", P.type);
    J.attribute("arraySize", P.array_size);
    J.attribute("rawType", P.raw_type);
    J.attribute("access", P.access);
    serializeAttr(J, P.attrs);
    J.attribute("isFunctor", P.is_functor);
    J.attribute("isCallback", P.is_callback);
    J.attribute("isAnonymous", P.is_anonymous);
    J.attribute("isStatic", P.is_static);
    if (P.is_callback) {
      J.attributeBegin("functor");
      serialize(J, P.signature, false);
      J.attributeEnd();
    }
    J.attribute("defaultValue", P.default_value);
    J.attribute("comment", P.comment);
    J.attribute("line", P.line);
  });
}

void serialize(llvm::json::OStream &J, const Function &P, bool method) {

  J.object([&] {
    J.attribute("name", P.name);
    J.attribute("isStatic", P.is_static);
    J.attribute("isConst", P.is_const);
    J.attribute("isNothrow", P.is_nothrow);
    J.attribute("access", P.access);
    serializeAttr(J, P.attrs);
    J.attribute("comment", P.comment);
    J.attributeObject("parameters", [&] {
      for (auto param : P.parameters)
        serialize(J, param);
    });
    J.attribute("retType", P.ret_type);
    J.attribute("rawRetType", P.raw_ret_type);
    if (!method)
      J.attribute("fileName", P.file_name);
    J.attribute("line", P.line);
  });
}
} // namespace meta

std::string meta::serialize(const Database &P) {
  std::string str;
  llvm::raw_string_ostream output(str);
  llvm::json::OStream J(output);
  J.objectBegin();

  J.attributeObject("records", [&] {
    for (auto &record : P.records) {
      J.attributeObject(record.name, [&] {
        J.attributeArray("bases", [&] {
          for (auto &base : record.bases)
            J.value(base);
        });
        serializeAttr(J, record.attrs);
        J.attributeObject("fields", [&] {
          for (auto field : record.fields)
            serialize(J, field);
        });
        J.attributeArray("methods", [&] {
          for (auto f : record.methods)
            serialize(J, f, true);
        });
        J.attribute("isNested", record.is_nested);
        J.attribute("comment", record.comment);
        J.attribute("fileName", record.file_name);
        J.attribute("line", record.line);
      });
    }
  });

  J.attributeArray("functions", [&] {
    for (auto &f : P.functions)
      serialize(J, f, false);
  });

  J.attributeObject("enums", [&] {
    for (auto &e : P.enums) {
      J.attributeObject(e.name, [&] {
        serializeAttr(J, e.attrs);
        J.attributeObject("values", [&] {
          for (auto v : e.values) {
            J.attributeObject(v.name, [&] {
              serializeAttr(J, v.attrs);
              J.attribute("value", v.value);
              J.attribute("comment", v.comment);
              J.attribute("line", v.line);
            });
          }
        });
        J.attribute("isScoped", e.is_scoped);
        J.attribute("underlying_type", e.underlying_type);
        J.attribute("comment", e.comment);
        J.attribute("fileName", e.file_name);
        J.attribute("line", e.line);
      });
    }
  });
  J.objectEnd();
  return str;
}