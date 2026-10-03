#include "doc/Effects.h"

#include <fstream>
#include <sstream>
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

const char* kCustomBlendTemplate =
    "//@name Fusion personnalisée\n"
    "//@category Fusion\n"
    "//@blend\n"
    "//@input A (dessous)\n"
    "//@input B (calque)\n"
    "//@input Masque\n"
    "//@halo 0\n"
    "//@param opacity float 1 0 1 \"Opacité\"\n"
    "\n"
    "// A, B : couleurs (non prémultipliées) et alpha ; m : masque.\n"
    "float3 Mix(float3 a, float3 b)\n"
    "{\n"
    "    return a * b;  // formule de mélange (ici Produit) : à modifier\n"
    "}\n"
    "\n"
    "float4 Effect(EffectContext ctx)\n"
    "{\n"
    "    float4 A = Input(0, ctx.px);\n"
    "    float4 B = Input(1, ctx.px);\n"
    "    float m = HasInput(2) ? Input(2, ctx.px).r : 1.0;\n"
    "    float ab = saturate(B.a * m * opacity);\n"
    "    // Composition source-over avec le mélange (comme les modes de fusion standard).\n"
    "    float a = ab + A.a * (1.0 - ab);\n"
    "    float3 rgb = ab * (1.0 - A.a) * B.rgb + ab * A.a * Mix(A.rgb, B.rgb) + (1.0 - ab) * A.a * A.rgb;\n"
    "    return float4(a > 0.0 ? rgb / a : 0.0, a);\n"
    "}\n";

int EffectHalo(const EffectDef& def, const std::vector<float4>& values) {
    if (def.inputs.empty()) return 0;  // generators never read inputs
    if (def.haloFull) return -1;
    if (!def.haloDeclared) return def.id.empty() ? -1 : 0;  // custom code: assume global
    float h = def.haloOffset;
    for (auto& [pid, scale] : def.haloTerms)
        for (size_t i = 0; i < def.params.size(); ++i)
            if (def.params[i].id == pid) h += std::abs(i < values.size() ? values[i].x : def.params[i].def.x) * scale;
    return (int)std::ceil(std::max(h, 0.0f));
}

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
    std::string group;  // current //@group
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
        else if (key == "blend") def->blend = true;
        else if (key == "graph") {
            // Raw remainder (JSON: spaces and quotes kept).
            size_t k = line.find("graph");
            def->graph = line.substr(k + 5);
            def->graph.erase(0, def->graph.find_first_not_of(" \t"));
            while (!def->graph.empty() && (def->graph.back() == '\r' || def->graph.back() == ' ')) def->graph.pop_back();
        } else if (key == "group") group = rest(1);
        else if (key == "halo" && t.size() > 1) {
            // //@halo full | //@halo <px> | //@halo <param> <scale> [<param> <scale>...] [<offset>]
            def->haloTerms.clear();
            def->haloFull = t[1] == "full";
            def->haloOffset = 0;
            for (size_t i = 1; i < t.size() && !def->haloFull;) {
                if (IsNumber(t[i])) { def->haloOffset = ToF(t[i]); ++i; }
                else { def->haloTerms.push_back({t[i], i + 1 < t.size() ? ToF(t[i + 1], 1) : 1.0f}); i += 2; }
            }
            def->haloDeclared = true;
        }
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
            } else if (type == "float3") {
                p.type = ParamType::Float3;
                p.def = {num(0, 0), num(1, 0), num(2, 0), 0};
                p.min = num(3, 0); p.max = num(4, 1);
            } else if (type == "float4") {
                p.type = ParamType::Float4;
                p.def = {num(0, 0), num(1, 0), num(2, 0), num(3, 0)};
                p.min = num(4, 0); p.max = num(5, 1);
            } else if (type == "curve") {
                p.type = ParamType::Curve;
            } else if (type == "lut") {
                p.type = ParamType::Lut;
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
            p.group = group;
            if (def->params.size() < 32) def->params.push_back(p);
        }
    }
    if (!generator && !explicitInputs) def->inputs.push_back("Image");
    // Older blend files (imported CyTexture blends): 3 inputs in a "Fusion" category.
    if (def->inputs.size() == 3 && def->category.find("Fusion") != std::string::npos) def->blend = true;
    def->body = body;
    return def;
}

std::string EffectParamFields(const EffectDef& def) {
    std::string s;
    int pad = 0;
    for (auto& p : def.params) {
        switch (p.type) {
            case ParamType::Float: s += "    float " + p.id + "; float3 _pad" + std::to_string(pad++) + ";\n"; break;
            case ParamType::Int:
            case ParamType::Bool:
            case ParamType::Enum: s += "    int " + p.id + "; int3 _pad" + std::to_string(pad++) + ";\n"; break;
            case ParamType::Color: s += "    float4 " + p.id + ";\n"; break;
            case ParamType::Float2: s += "    float2 " + p.id + "; float2 _pad" + std::to_string(pad++) + ";\n"; break;
            case ParamType::Float3: s += "    float3 " + p.id + "; float _pad" + std::to_string(pad++) + ";\n"; break;
            case ParamType::Float4: s += "    float4 " + p.id + ";\n"; break;
            case ParamType::Curve: s += "    float4 " + p.id + "[" + std::to_string(kCurveSlots) + "];\n"; break;
            case ParamType::Lut: s += "    float4 " + p.id + ";\n"; break;
        }
    }
    if (def.params.empty()) s += "    float4 _unused;\n";
    return s;
}

std::string BuildEffectSource(const EffectDef& def) {
    std::string s;
    s += "#define EFFECT_PREMULTIPLIED ";
    s += def.premultiplied ? "1\n" : "0\n";
    s += gpu::Shaders().File("effect_common.slang");
    s += "\ncbuffer EffectParams : register(b1)\n{\n";
    s += EffectParamFields(def);
    s += "};\n\n";
    for (auto& p : def.params) {
        if (p.type != ParamType::Curve) continue;
        s += "float " + p.id + "_eval(float x)\n{\n"
             "    float f = saturate(x) * 255.0;\n"
             "    uint i = min((uint)f, 254u);\n"
             "    uint j = i + 1;\n"
             "    return lerp(" + p.id + "[i >> 2][i & 3], " + p.id + "[j >> 2][j & 3], f - i);\n}\n";
    }
    int lutIndex = 0;
    for (auto& p : def.params) {
        if (p.type != ParamType::Lut || lutIndex >= kMaxLutParams) continue;
        // LUT stored as a (n*n) x n texture: x = r + b*n, y = g. Trilinear = 2 bilinear taps.
        std::string t = p.id + "_tex";
        s += "Texture2D<float4> " + t + " : register(t" + std::to_string(5 + lutIndex++) + ");\n";
        s += "float3 " + p.id + "_sample(float3 c)\n{\n"
             "    float n = " + p.id + ".x;\n"
             "    if (n < 2.0) return c;\n"
             "    float3 g = saturate(c) * (n - 1.0);\n"
             "    float b0 = floor(g.b), b1 = min(b0 + 1.0, n - 1.0);\n"
             "    float w = n * n;\n"
             "    float3 a = " + t + ".SampleLevel(sLinearClamp, float2((b0 * n + g.r + 0.5) / w, (g.g + 0.5) / n), 0).rgb;\n"
             "    float3 b = " + t + ".SampleLevel(sLinearClamp, float2((b1 * n + g.r + 0.5) / w, (g.g + 0.5) / n), 0).rgb;\n"
             "    return lerp(a, b, g.b - b0);\n}\n";
    }
    s += "#line 1\n";
    s += def.body;
    s += "\n[numthreads(8, 8, 1)]\n"
         "void main(uint3 id : SV_DispatchThreadID)\n"
         "{\n"
         "    if (any(id.xy >= gSize)) return;\n"
         "    EffectContext ctx;\n"
         "    ctx.px = gOrigin + int2(id.xy);\n"
         "    ctx.size = float2(gDocSize);\n"
         "    ctx.texel = 1.0 / ctx.size;\n"
         "    ctx.uv = (float2(ctx.px) + 0.5) * ctx.texel;\n"
         "    ctx.pass = gPass;\n"
         "    gOut[id.xy] = ToStorage(Effect(ctx));\n"
         "}\n";
    return s;
}

std::vector<float4> PackParams(const EffectDef& def, const std::vector<float4>& values, const std::vector<CurvePoints>* curves,
                               const std::vector<Lut3DPtr>* luts) {
    std::vector<float4> out;
    out.reserve(def.params.size() + 4);
    for (size_t i = 0; i < def.params.size(); ++i) {
        float4 v = i < values.size() ? values[i] : def.params[i].def;
        switch (def.params[i].type) {
            case ParamType::Int:
            case ParamType::Bool:
            case ParamType::Enum: {
                float4 slot;
                i32 iv = (i32)std::lround(v.x);
                memcpy(&slot.x, &iv, 4);
                out.push_back(slot);
                break;
            }
            case ParamType::Curve: {
                float lut[256];
                static const CurvePoints identity = {{0, 0}, {1, 1}};
                const CurvePoints& pts = curves && i < curves->size() && curves->at(i).size() >= 2 ? curves->at(i) : identity;
                BakeCurve(pts, lut);
                for (int k = 0; k < kCurveSlots; ++k) out.push_back({lut[k * 4], lut[k * 4 + 1], lut[k * 4 + 2], lut[k * 4 + 3]});
                break;
            }
            case ParamType::Lut: {
                // size, loaded, content hash (makes render caches see LUT changes).
                const Lut3D* l = luts && i < luts->size() ? luts->at(i).get() : nullptr;
                float4 slot = {l ? (float)l->size : 0.0f, l ? 1.0f : 0.0f, 0, 0};
                u64 h = l ? l->hash : 0;
                u32 lo = (u32)h, hi = (u32)(h >> 32);
                memcpy(&slot.z, &lo, 4);
                memcpy(&slot.w, &hi, 4);
                out.push_back(slot);
                break;
            }
            default: out.push_back(v); break;
        }
    }
    if (out.empty()) out.push_back({});
    return out;
}

Lut3DPtr MakeLut(std::string name, int size, std::vector<float> rgb) {
    auto l = std::make_shared<Lut3D>();
    l->name = std::move(name);
    l->size = size;
    l->rgb = std::move(rgb);
    Hasher h;
    h.Pod(size);
    h.Bytes(l->rgb.data(), l->rgb.size() * sizeof(float));
    l->hash = h.h;
    return l;
}

Lut3DPtr LoadCubeLut(const std::string& path, std::string* error) {
    std::ifstream f(Utf8Path(path));
    if (!f) { if (error) *error = "fichier introuvable"; return nullptr; }
    std::string line, title;
    int size = 0;
    float dmin[3] = {0, 0, 0}, dmax[3] = {1, 1, 1};
    std::vector<float> rgb;
    while (std::getline(f, line)) {
        size_t a = line.find_first_not_of(" \t\r");
        if (a == std::string::npos || line[a] == '#') continue;
        std::istringstream ss(line.substr(a));
        std::string key;
        if (std::isalpha((unsigned char)line[a])) {
            ss >> key;
            if (key == "TITLE") {
                size_t q0 = line.find('"'), q1 = line.rfind('"');
                if (q0 != std::string::npos && q1 > q0) title = line.substr(q0 + 1, q1 - q0 - 1);
            } else if (key == "LUT_3D_SIZE") {
                ss >> size;
            } else if (key == "LUT_1D_SIZE") {
                if (error) *error = "LUT 1D non supportée";
                return nullptr;
            } else if (key == "DOMAIN_MIN") {
                ss >> dmin[0] >> dmin[1] >> dmin[2];
            } else if (key == "DOMAIN_MAX") {
                ss >> dmax[0] >> dmax[1] >> dmax[2];
            }
            continue;
        }
        float r, g, b;
        if (ss >> r >> g >> b) { rgb.push_back(r); rgb.push_back(g); rgb.push_back(b); }
    }
    if (size < 2 || size > 128 || rgb.size() != (size_t)size * size * size * 3) {
        if (error) *error = "taille de LUT invalide";
        return nullptr;
    }
    // Normalize outputs from a non-unit domain.
    for (size_t i = 0; i < rgb.size(); ++i) {
        int c = (int)(i % 3);
        float span = dmax[c] - dmin[c];
        if (span > 0 && (dmin[c] != 0 || dmax[c] != 1)) rgb[i] = (rgb[i] - dmin[c]) / span;
    }
    if (title.empty()) title = Narrow(Utf8Path(path).stem().wstring());
    return MakeLut(title, size, std::move(rgb));
}

float EvalCurve(const CurvePoints& pts, float x) {
    size_t n = pts.size();
    if (n == 0) return x;
    if (n == 1 || x <= pts[0].x) return std::clamp(pts[0].y, 0.0f, 1.0f);
    if (x >= pts[n - 1].x) return std::clamp(pts[n - 1].y, 0.0f, 1.0f);
    // Secant slopes, then Fritsch-Carlson tangents.
    std::vector<float> d(n - 1), m(n);
    for (size_t i = 0; i + 1 < n; ++i) d[i] = (pts[i + 1].y - pts[i].y) / std::max(pts[i + 1].x - pts[i].x, 1e-6f);
    m[0] = d[0];
    m[n - 1] = d[n - 2];
    for (size_t i = 1; i + 1 < n; ++i) m[i] = d[i - 1] * d[i] <= 0 ? 0.0f : (d[i - 1] + d[i]) * 0.5f;
    for (size_t i = 0; i + 1 < n; ++i) {
        if (d[i] == 0) { m[i] = m[i + 1] = 0; continue; }
        float a = m[i] / d[i], b = m[i + 1] / d[i];
        float s = a * a + b * b;
        if (s > 9.0f) {
            float t = 3.0f / std::sqrt(s);
            m[i] = t * a * d[i];
            m[i + 1] = t * b * d[i];
        }
    }
    size_t k = 0;
    while (k + 2 < n && x > pts[k + 1].x) ++k;
    float h = std::max(pts[k + 1].x - pts[k].x, 1e-6f);
    float t = (x - pts[k].x) / h, t2 = t * t, t3 = t2 * t;
    float y = (2 * t3 - 3 * t2 + 1) * pts[k].y + (t3 - 2 * t2 + t) * h * m[k] + (-2 * t3 + 3 * t2) * pts[k + 1].y + (t3 - t2) * h * m[k + 1];
    return std::clamp(y, 0.0f, 1.0f);
}

void BakeCurve(const CurvePoints& pts, float out[256]) {
    for (int i = 0; i < 256; ++i) out[i] = EvalCurve(pts, i / 255.0f);
}

void EffectLibrary::Load(const std::string& dir_) {
    dir = dir_;
    effects.clear();
    byId.clear();
    std::error_code ec;
    std::vector<fs::path> paths;
    for (auto& e : fs::directory_iterator(Utf8Path(dir), ec))
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
