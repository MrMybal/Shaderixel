#include "doc/Effects.h"
#include "gpu/ShaderSystem.h"

#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

namespace sx {

namespace {
EffectLibrary g_effects;

std::vector<std::string> Tokenize(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) ++i;
        if (i >= s.size()) break;
        if (s[i] == '"') {
            size_t j = s.find('"', i + 1);
            if (j == std::string::npos) j = s.size();
            out.push_back(s.substr(i + 1, j - i - 1));
            i = j + 1;
        } else {
            size_t j = i;
            while (j < s.size() && s[j] != ' ' && s[j] != '\t' && s[j] != '\r') ++j;
            out.push_back(s.substr(i, j - i));
            i = j;
        }
    }
    return out;
}

float ToF(const std::string& s, float fallback = 0) {
    char* end = nullptr;
    float v = strtof(s.c_str(), &end);
    return end == s.c_str() ? fallback : v;
}

bool IsNumber(const std::string& s) {
    char* end = nullptr;
    strtof(s.c_str(), &end);
    return end != s.c_str() && *end == 0;
}
}  // namespace

EffectLibrary& Effects() { return g_effects; }

const char* kCustomEffectTemplate =
    "//@name Effet personnalisé\n"
    "//@category Personnalisé\n"
    "//@param amount float 1 0 1 \"Quantité\"\n"
    "//@param tint color 1 0.5 0.2 1 \"Teinte\"\n"
    "\n"
    "// ctx.px (pixel), ctx.uv, ctx.size, ctx.texel, ctx.pass\n"
    "// Input(i, px), InputUV(i, uv), HasInput(i), Prev(px) ...\n"
    "float4 Effect(EffectContext ctx)\n"
    "{\n"
    "    float4 c = Input(0, ctx.px);\n"
    "    float3 tinted = c.rgb * tint.rgb;\n"
    "    return float4(lerp(c.rgb, tinted, amount), c.a);\n"
    "}\n";

std::vector<float4> EffectDef::DefaultParams() const {
    std::vector<float4> v;
    for (auto& p : params) v.push_back(p.def);
    return v;
}

EffectDefPtr ParseEffect(const std::string& id, const std::string& text) {
    auto def = std::make_shared<EffectDef>();
    def->id = id;
    def->name = id.empty() ? "Effet" : id;
    def->category = "Personnalisé";
    bool generator = false, explicitInputs = false;
    std::string body;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (line.rfind("//@", 0) != 0) {
            body += line;
            body += "\n";
            continue;
        }
        body += "\n";  // keep line numbers stable for error messages
        auto t = Tokenize(line.substr(3));
        if (t.empty()) continue;
        const std::string& key = t[0];
        auto rest = [&](size_t from) {
            std::string r;
            for (size_t i = from; i < t.size(); ++i) { if (!r.empty()) r += " "; r += t[i]; }
            return r;
        };
        if (key == "name") def->name = rest(1);
        else if (key == "category") def->category = rest(1);
        else if (key == "generator") generator = true;
        else if (key == "premultiplied") def->premultiplied = true;
        else if (key == "passes" && t.size() > 1) def->passes = std::clamp((int)ToF(t[1], 1), 1, 16);
        else if (key == "input" && t.size() > 1) {
            explicitInputs = true;
            if (def->inputs.size() < 4) def->inputs.push_back(rest(1));
        } else if (key == "param" && t.size() >= 3) {
            ParamDef p;
            p.id = t[1];
            p.label = p.id;
            const std::string& type = t[2];
            std::vector<std::string> nums;
            std::vector<std::string> strs;
            for (size_t i = 3; i < t.size(); ++i) (IsNumber(t[i]) ? nums : strs).push_back(t[i]);
            auto num = [&](size_t i, float fb) { return i < nums.size() ? ToF(nums[i], fb) : fb; };
            if (type == "float") {
                p.type = ParamType::Float;
                p.def = {num(0, 0), 0, 0, 0};
                p.min = num(1, 0); p.max = num(2, 1);
            } else if (type == "int") {
                p.type = ParamType::Int;
                p.def = {num(0, 0), 0, 0, 0};
                p.min = num(1, 0); p.max = num(2, 100);
            } else if (type == "bool") {
                p.type = ParamType::Bool;
                p.def = {num(0, 0), 0, 0, 0};
            } else if (type == "color") {
                p.type = ParamType::Color;
                p.def = {num(0, 1), num(1, 1), num(2, 1), num(3, 1)};
            } else if (type == "float2") {
                p.type = ParamType::Float2;
                p.def = {num(0, 0), num(1, 0), 0, 0};
                p.min = num(2, 0); p.max = num(3, 1);
            } else if (type == "enum") {
                p.type = ParamType::Enum;
                p.def = {num(0, 0), 0, 0, 0};
                if (!strs.empty()) {
                    std::string items = strs[0];
                    strs.erase(strs.begin());
                    size_t a = 0;
                    while (a <= items.size()) {
                        size_t b = items.find('|', a);
                        if (b == std::string::npos) b = items.size();
                        p.enumItems.push_back(items.substr(a, b - a));
                        a = b + 1;
                    }
                }
            } else {
                def->error += "type de paramètre inconnu: " + type + "\n";
                continue;
            }
            if (!strs.empty()) p.label = strs.back();
            if (def->params.size() < 32) def->params.push_back(p);
        }
    }
    if (!generator && !explicitInputs) def->inputs.push_back("Image");
    def->body = body;
    return def;
}

std::string BuildEffectSource(const EffectDef& def) {
    std::string s;
    s += "#define EFFECT_PREMULTIPLIED ";
    s += def.premultiplied ? "1\n" : "0\n";
    s += gpu::Shaders().File("effect_common.slang");
    s += "\ncbuffer EffectParams : register(b1)\n{\n";
    int pad = 0;
    for (auto& p : def.params) {
        switch (p.type) {
            case ParamType::Float: s += "    float " + p.id + "; float3 _pad" + std::to_string(pad++) + ";\n"; break;
            case ParamType::Int:
            case ParamType::Bool:
            case ParamType::Enum: s += "    int " + p.id + "; int3 _pad" + std::to_string(pad++) + ";\n"; break;
            case ParamType::Color: s += "    float4 " + p.id + ";\n"; break;
            case ParamType::Float2: s += "    float2 " + p.id + "; float2 _pad" + std::to_string(pad++) + ";\n"; break;
        }
    }
    if (def.params.empty()) s += "    float4 _unused;\n";
    s += "};\n\n#line 1\n";
    s += def.body;
    s += "\n[numthreads(8, 8, 1)]\n"
         "void main(uint3 id : SV_DispatchThreadID)\n"
         "{\n"
         "    if (any(id.xy >= gSize)) return;\n"
         "    EffectContext ctx;\n"
         "    ctx.px = int2(id.xy);\n"
         "    ctx.size = float2(gSize);\n"
         "    ctx.texel = 1.0 / ctx.size;\n"
         "    ctx.uv = (float2(id.xy) + 0.5) * ctx.texel;\n"
         "    ctx.pass = gPass;\n"
         "    gOut[id.xy] = ToStorage(Effect(ctx));\n"
         "}\n";
    return s;
}

std::vector<float4> PackParams(const EffectDef& def, const std::vector<float4>& values) {
    std::vector<float4> out(std::max<size_t>(def.params.size(), 1));
    for (size_t i = 0; i < def.params.size(); ++i) {
        float4 v = i < values.size() ? values[i] : def.params[i].def;
        switch (def.params[i].type) {
            case ParamType::Int:
            case ParamType::Bool:
            case ParamType::Enum: {
                i32 iv = (i32)std::lround(v.x);
                memcpy(&out[i].x, &iv, 4);
                break;
            }
            default: out[i] = v; break;
        }
    }
    return out;
}

void EffectLibrary::Load(const std::string& dir_) {
    dir = dir_;
    effects.clear();
    byId.clear();
    std::error_code ec;
    std::vector<fs::path> paths;
    for (auto& e : fs::directory_iterator(fs::u8path(dir), ec))
        if (e.path().extension() == ".slang") paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());
    for (auto& p : paths) {
        std::string stem = Narrow(p.stem().wstring());
        const std::string& text = gpu::Shaders().File("effects/" + Narrow(p.filename().wstring()));
        auto def = ParseEffect(stem, text);
        effects.push_back(def);
        byId[stem] = def;
    }
    shaderGeneration = gpu::Shaders().FileGeneration();
    ++generation;
    Log("Effect library: %zu effects", effects.size());
}

void EffectLibrary::ReloadIfChanged() {
    if (gpu::Shaders().FileGeneration() != shaderGeneration) Load(dir);
}

EffectDefPtr EffectLibrary::Find(const std::string& id) const {
    auto it = byId.find(id);
    return it == byId.end() ? nullptr : it->second;
}

std::vector<std::string> EffectLibrary::Categories() const {
    std::vector<std::string> cats;
    for (auto& e : effects)
        if (std::find(cats.begin(), cats.end(), e->category) == cats.end()) cats.push_back(e->category);
    return cats;
}

}  // namespace sx
