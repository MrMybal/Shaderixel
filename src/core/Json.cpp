#include "core/Json.h"

#include <cstdio>
#include <cstdlib>

namespace sx {

namespace {
const Json kNull;

void Escape(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    out += '"';
}

struct Parser {
    const char* p;
    const char* end;
    std::string err;

    void Ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }
    bool Fail(const char* m) { if (err.empty()) err = m; return false; }

    bool ParseString(std::string& s) {
        if (p >= end || *p != '"') return Fail("string expected");
        ++p;
        while (p < end && *p != '"') {
            char c = *p++;
            if (c != '\\') { s += c; continue; }
            if (p >= end) return Fail("bad escape");
            char e = *p++;
            switch (e) {
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'u': {
                    if (end - p < 4) return Fail("bad \\u");
                    unsigned cp = (unsigned)strtoul(std::string(p, p + 4).c_str(), nullptr, 16);
                    p += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        unsigned lo = (unsigned)strtoul(std::string(p + 2, p + 6).c_str(), nullptr, 16);
                        p += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    if (cp < 0x80) s += (char)cp;
                    else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 63)); }
                    else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
                    else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 63)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
                    break;
                }
                default: s += e; break;
            }
        }
        if (p >= end) return Fail("unterminated string");
        ++p;
        return true;
    }

    bool ParseValue(Json& out, int depth) {
        if (depth > 256) return Fail("too deep");
        Ws();
        if (p >= end) return Fail("unexpected end");
        char c = *p;
        if (c == '{') {
            ++p;
            out = Json::Object();
            Ws();
            if (p < end && *p == '}') { ++p; return true; }
            for (;;) {
                Ws();
                std::string key;
                if (!ParseString(key)) return false;
                Ws();
                if (p >= end || *p != ':') return Fail("':' expected");
                ++p;
                if (!ParseValue(out[key], depth + 1)) return false;
                Ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; return true; }
                return Fail("',' or '}' expected");
            }
        }
        if (c == '[') {
            ++p;
            out = Json::Array();
            Ws();
            if (p < end && *p == ']') { ++p; return true; }
            for (;;) {
                Json v;
                if (!ParseValue(v, depth + 1)) return false;
                out.Push(std::move(v));
                Ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; return true; }
                return Fail("',' or ']' expected");
            }
        }
        if (c == '"') {
            std::string s;
            if (!ParseString(s)) return false;
            out = Json(std::move(s));
            return true;
        }
        if (end - p >= 4 && std::string(p, p + 4) == "true") { p += 4; out = Json(true); return true; }
        if (end - p >= 5 && std::string(p, p + 5) == "false") { p += 5; out = Json(false); return true; }
        if (end - p >= 4 && std::string(p, p + 4) == "null") { p += 4; out = Json(); return true; }
        char* e = nullptr;
        double d = strtod(p, &e);
        if (e == p) return Fail("invalid value");
        p = e;
        out = Json(d);
        return true;
    }
};
}  // namespace

const Json& Json::operator[](const std::string& key) const {
    auto it = obj.find(key);
    return it == obj.end() ? kNull : it->second;
}

void Json::DumpTo(std::string& out, int indent, int depth) const {
    auto nl = [&](int d) {
        if (!indent) return;
        out += '\n';
        out.append((size_t)d * indent * 2, ' ');
    };
    switch (type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += num != 0 ? "true" : "false"; break;
        case Type::Number: {
            char buf[32];
            if (num == (double)(long long)num && std::abs(num) < 1e15) snprintf(buf, sizeof(buf), "%lld", (long long)num);
            else snprintf(buf, sizeof(buf), "%.9g", num);
            out += buf;
            break;
        }
        case Type::String: Escape(out, str); break;
        case Type::Array: {
            out += '[';
            // Short numeric arrays stay on one line.
            bool flat = arr.size() <= 4 && std::all_of(arr.begin(), arr.end(), [](const Json& j) { return j.type == Type::Number; });
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i) out += flat ? ", " : ",";
                if (!flat) nl(depth + 1);
                arr[i].DumpTo(out, indent, depth + 1);
            }
            if (!flat && !arr.empty()) nl(depth);
            out += ']';
            break;
        }
        case Type::Object: {
            out += '{';
            size_t i = 0;
            for (auto& [k, v] : obj) {
                if (i++) out += ',';
                nl(depth + 1);
                Escape(out, k);
                out += indent ? ": " : ":";
                v.DumpTo(out, indent, depth + 1);
            }
            if (!obj.empty()) nl(depth);
            out += '}';
            break;
        }
    }
}

std::string Json::Dump(int indent) const {
    std::string s;
    DumpTo(s, indent, 0);
    return s;
}

bool Json::Parse(const std::string& text, Json& out, std::string* error) {
    Parser ps{text.data(), text.data() + text.size(), {}};
    bool ok = ps.ParseValue(out, 0);
    if (!ok && error) *error = ps.err + " at offset " + std::to_string(ps.p - text.data());
    return ok;
}

Json ToJson(const float4& v) {
    Json j = Json::Array();
    j.Push(v.x); j.Push(v.y); j.Push(v.z); j.Push(v.w);
    return j;
}

float4 Float4FromJson(const Json& j, float4 def) {
    if (!j.IsArray()) return def;
    float4 r = def;
    for (size_t i = 0; i < 4 && i < j.Size(); ++i) r[(int)i] = j.At(i).F();
    return r;
}

Json ToJson(const float2& v) {
    Json j = Json::Array();
    j.Push(v.x); j.Push(v.y);
    return j;
}

float2 Float2FromJson(const Json& j) {
    if (!j.IsArray() || j.Size() < 2) return {};
    return {j.At(0).F(), j.At(1).F()};
}

}  // namespace sx
