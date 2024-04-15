#include "meta.h"
#include "llvm/Support/JSON.h"

///   json::OStream J(OS);
//    J.arrayBegin();
///   for (const Event &E : Events) {
///     J.objectBegin();
///     J.attribute("timestamp", int64_t(E.Time));
///     J.attributeBegin("participants");
///     for (const Participant &P : E.Participants)
///       J.value(P.toString());
///     J.attributeEnd();
///     J.objectEnd();
///   }
///   J.arrayEnd();
namespace meta {
void serializeAttr(llvm::json::OStream &J, const std::string &attr) {
  J.attributeBegin("attrs");
  J.rawValue("{ " + attr + " }");
  J.attributeEnd();
}

void serialize(llvm::json::OStream &J, const Function &P, bool method);

void serialize(llvm::json::OStream &J, const Field &P) {
  J.attributeObject(P.name, [&] {
    J.attribute("type", P.type);
    J.attribute("arraySize", P.arraySize);
    J.attribute("rawType", P.rawType);
    J.attribute("access", P.access);
    serializeAttr(J, P.attrs);
    J.attribute("isFunctor", P.isFunctor);
    J.attribute("isCallback", P.isCallback);
    J.attribute("isAnonymous", P.isAnonymous);
    if(P.isCallback) {
      J.attributeBegin("functor");
      serialize(J, P.signature, false);
      J.attributeEnd();
    }
    J.attribute("defaultValue", P.defaultValue);
    J.attribute("comment", P.comment);
    J.attribute("offset", P.offset);
    J.attribute("line", P.line);
  });
}

void serialize(llvm::json::OStream &J, const Function &P, bool method) {

  J.object([&] {
    J.attribute("name", P.name);
    J.attribute("isStatic", P.isStatic);
    J.attribute("isConst", P.isConst);
    J.attribute("isNothrow", P.isNothrow);
    J.attribute("access", P.access);
    serializeAttr(J, P.attrs);
    J.attribute("comment", P.comment);
    J.attributeObject("parameters", [&] {
      for (auto param : P.parameters)
        serialize(J, param);
    });
    J.attribute("retType", P.retType);
    J.attribute("rawRetType", P.rawRetType);
    if (!method)
      J.attribute("fileName", P.fileName);
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
        J.attributeArray("statics", [&] {
          for (auto field : record.statics)
            serialize(J, field);
        });
        J.attributeArray("methods", [&] {
          for (auto f : record.methods)
            serialize(J, f, true);
        });
        J.attribute("isNested", record.isNested);
        J.attribute("comment", record.comment);
        J.attribute("fileName", record.fileName);
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
        J.attribute("isScoped", e.isScoped);
        J.attribute("underlying_type", e.underlying_type);
        J.attribute("comment", e.comment);
        J.attribute("fileName", e.fileName);
        J.attribute("line", e.line);
      });
    }
  });

  serializeAttr(J, P.attrs);
  J.objectEnd();
  return str;
}