#pragma once
// Minimal JSON DOM (parse + pretty write) for the document format and settings.

#include "core/Common.h"

#include <map>
#include <memory>

namespace sx {

class Json {
public:
    enum class Type : u8 { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool b) : type(Type::Bool), num(b ? 1 : 0) {}
    Json(int v) : type(Type::Number), num(v) {}
    Json(u32 v) : type(Type::Number), num(v) {}
    Json(u64 v) : type(Type::Number), num((double)v) {}
    Json(float v) : type(Type::Number), num(v) {}
    Json(double v) : type(Type::Number), num(v) {}
    Json(const char* s) : type(Type::String), str(s) {}
    Json(std::string s) : type(Type::String), str(std::move(s)) {}
    static Json Array() { Json j; j.type = Type::Array; return j; }
    static Json Object() { Json j; j.type = Type::Object; return j; }

    Type GetType() const { return type; }
    bool IsNull() const { return type == Type::Null; }
    bool IsObject() const { return type == Type::Object; }
    bool IsArray() const { return type == Type::Array; }

    // Object access (creates on write)
    Json& operator[](const std::string& key) { type = Type::Object; return obj[key]; }
    const Json& operator[](const std::string& key) const;
    bool Has(const std::string& key) const { return obj.count(key) != 0; }
    void Erase(const std::string& key) { obj.erase(key); }
    const std::map<std::string, Json>& Items() const { return obj; }

    // Array access
    void Push(Json v) { type = Type::Array; arr.push_back(std::move(v)); }
    size_t Size() const { return type == Type::Array ? arr.size() : obj.size(); }
    const Json& At(size_t i) const { return arr[i]; }
    const std::vector<Json>& Elements() const { return arr; }

    // Typed reads with defaults
    double Num(double def = 0) const { return type == Type::Number || type == Type::Bool ? num : def; }
    float F(float def = 0) const { return (float)Num(def); }
    int I(int def = 0) const { return (int)Num(def); }
    u32 U(u32 def = 0) const { return (u32)Num(def); }
    bool B(bool def = false) const { return type == Type::Bool || type == Type::Number ? num != 0 : def; }
    const std::string& S() const { return str; }
    std::string S(const std::string& def) const { return type == Type::String ? str : def; }

    std::string Dump(int indent = 1) const;
    static bool Parse(const std::string& text, Json& out, std::string* error = nullptr);

private:
    void DumpTo(std::string& out, int indent, int depth) const;
    Type type = Type::Null;
    double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;
};

// Helpers for engine types.
Json ToJson(const float4& v);
float4 Float4FromJson(const Json& j, float4 def = {});
Json ToJson(const float2& v);
float2 Float2FromJson(const Json& j);

}  // namespace sx
