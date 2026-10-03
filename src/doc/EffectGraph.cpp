#include "doc/EffectGraph.h"

#include <cstdio>
#include <set>
#include <unordered_map>

namespace sx {

// ------------------------------------------------------------------ registry

const std::vector<EGOpDef>& EffectGraphOps() {
    static const std::vector<EGOpDef> ops = {
        // Sources
        {"input", "Image d'entrée", "Entrées", {}, {"RGBA", "RGB", "R", "G", "B", "A"}, 4},
        {"sample", "Échantillonner l'entrée (UV)", "Entrées", {{"UV", -1}}, {"RGBA", "RGB", "R", "G", "B", "A"}, 4},
        {"uv", "Coordonnées UV", "Entrées", {}, {""}, 2},
        {"texture", "Texture d'entrée", "Entrées", {}, {"Texture"}, 5, true},
        {"canvassize", "Taille du canevas (float4)", "Entrées", {}, {""}, 4},
        {"pixel", "Position du pixel", "Entrées", {}, {""}, 2},
        {"size", "Taille de l'image", "Entrées", {}, {""}, 2},
        {"const", "Constante", "Constantes", {}, {""}, 1, true},
        {"const2", "Constante 2D", "Constantes", {}, {""}, 2, true},
        {"color", "Couleur", "Constantes", {}, {""}, 4, true},
        {"param", "Paramètre (réel)", "Paramètres", {}, {""}, 1, true},
        {"paramcolor", "Paramètre (couleur)", "Paramètres", {}, {""}, 4, true},
        {"param2", "Paramètre (2D)", "Paramètres", {}, {""}, 2, true},
        // Math
        {"add", "Ajouter", "Maths", {{"A", 0}, {"B", 0}}, {""}},
        {"sub", "Soustraire", "Maths", {{"A", 0}, {"B", 0}}, {""}},
        {"mul", "Multiplier", "Maths", {{"A", 1}, {"B", 1}}, {""}},
        {"div", "Diviser", "Maths", {{"A", 1}, {"B", 1}}, {""}},
        {"min", "Minimum", "Maths", {{"A", 0}, {"B", 0}}, {""}},
        {"max", "Maximum", "Maths", {{"A", 0}, {"B", 0}}, {""}},
        {"pow", "Puissance", "Maths", {{"Base", 1}, {"Exp", 1}}, {""}},
        {"lerp", "Interpolation (lerp)", "Maths", {{"A", 0}, {"B", 1}, {"Alpha", 0.5f}}, {""}},
        {"clamp", "Limiter (clamp)", "Maths", {{"Valeur", 0}, {"Min", 0}, {"Max", 1}}, {""}},
        {"smoothstep", "Smoothstep", "Maths", {{"Min", 0}, {"Max", 1}, {"Valeur", 0}}, {""}},
        {"step", "Seuil (step)", "Maths", {{"Seuil", 0.5f}, {"Valeur", 0}}, {""}},
        {"oneminus", "1 - x", "Maths", {{"x", 0}}, {""}},
        {"abs", "Valeur absolue", "Maths", {{"x", 0}}, {""}},
        {"negate", "Opposé", "Maths", {{"x", 0}}, {""}},
        {"saturate", "Saturer (0-1)", "Maths", {{"x", 0}}, {""}},
        {"frac", "Partie fractionnaire", "Maths", {{"x", 0}}, {""}},
        {"floor", "Arrondi inférieur", "Maths", {{"x", 0}}, {""}},
        {"ceil", "Arrondi supérieur", "Maths", {{"x", 0}}, {""}},
        {"sqrt", "Racine carrée", "Maths", {{"x", 0}}, {""}},
        {"sin", "Sinus", "Maths", {{"x", 0}}, {""}},
        {"cos", "Cosinus", "Maths", {{"x", 0}}, {""}},
        {"exp2", "Exp2", "Maths", {{"x", 0}}, {""}},
        {"log2", "Log2", "Maths", {{"x", 1}}, {""}},
        {"if", "Si (A > B)", "Maths", {{"A", 0}, {"B", 0}, {"A > B", 1}, {"A <= B", 0}}, {""}},
        // Vectors
        {"dot", "Produit scalaire", "Vecteurs", {{"A", 0}, {"B", 0}}, {""}, 1},
        {"length", "Longueur", "Vecteurs", {{"x", 0}}, {""}, 1},
        {"distance", "Distance", "Vecteurs", {{"A", 0}, {"B", 0}}, {""}, 1},
        {"normalize", "Normaliser", "Vecteurs", {{"x", 0}}, {""}},
        {"split", "Séparer (R G B A)", "Vecteurs", {{"Entrée", 0}}, {"R", "G", "B", "A"}, 1},
        {"append", "Concaténer", "Vecteurs", {{"A", 0}, {"B", 0}}, {""}},
        {"make4", "Composer RGBA", "Vecteurs", {{"R", 0}, {"G", 0}, {"B", 0}, {"A", 1}}, {""}, 4},
        {"make3", "Composer RGB", "Vecteurs", {{"R", 0}, {"G", 0}, {"B", 0}}, {""}, 3},
        {"make2", "Composer vecteur 2", "Vecteurs", {{"X", 0}, {"Y", 0}}, {""}, 2},
        {"mask", "Masque de composantes", "Vecteurs", {{"Entrée", 0}}, {""}, 0, true},
        // Color
        {"luma", "Luminance", "Couleur", {{"Couleur", 0}}, {""}, 1},
        {"desaturate", "Désaturer", "Couleur", {{"Couleur", 0}, {"Quantité", 1}}, {""}, 3},
        {"rgb2hsv", "RVB vers TSV", "Couleur", {{"RVB", 0}}, {""}, 3},
        {"hsv2rgb", "TSV vers RVB", "Couleur", {{"TSV", 0}}, {""}, 3},
        {"tosrgb", "Linéaire vers sRGB", "Couleur", {{"Couleur", 0}}, {""}, 3},
        {"tolinear", "sRGB vers linéaire", "Couleur", {{"Couleur", 0}}, {""}, 3},
        // Procedural
        {"noise", "Bruit", "Procédural", {{"UV", -1}, {"Échelle", 8}}, {""}, 1},
        {"checker", "Damier", "Procédural", {{"UV", -1}, {"Cases", 8}}, {""}, 1},
        {"rotateuv", "Rotation UV", "Procédural", {{"UV", -1}, {"Angle", 0}, {"Centre", 0.5f}}, {""}, 2},
        // Blends (A below, B layer)
        {"opacity", "Opacité du calque", "Fusion", {}, {""}, 1},
        {"blendover", "Composer la fusion", "Fusion", {{"A (dessous)", 0}, {"B (calque)", 0}, {"Mélange RGB", 0}, {"Masque", 1}, {"Opacité", -2}}, {""}, 4},
        // Brushes
        {"brush", "Touche du pinceau", "Pinceau", {},
         {"Position locale", "UV", "Pixel", "Pression", "Couleur", "Dureté", "Flux", "Rayon", "Graine", "Temps", "Distance", "Direction"}, 0},
        {"roundtip", "Pointe ronde", "Pinceau", {{"Position locale", -3}, {"Dureté", -4}}, {""}, 1},
        {"random", "Aléatoire par touche", "Pinceau", {{"Canal", 0}}, {""}, 1},
        // Custom
        {"custom", "HLSL personnalisé", "Personnalisé", {{"A", 0}, {"B", 0}, {"C", 0}, {"D", 0}}, {""}, 4, true},
        // Output
        {"output", "Sortie", "Sortie", {{"RGBA", 0}, {"RGB", 0}, {"Alpha", 1}}, {}, 0},
    };
    return ops;
}

bool OpAllowed(const EGOpDef& op, EGKind kind) {
    std::string c = op.category, o = op.op;
    if (c == "Fusion") return kind == EGKind::Blend;
    if (c == "Pinceau") return kind == EGKind::Brush;
    if (kind == EGKind::Brush && (o == "input" || o == "sample" || o == "texture")) return false;
    return true;
}

int PinValueSize(int t) { return t == kPinInt || t == kPinBool ? 1 : t == kPinColor ? 4 : t; }
const char* PinHlslType(int t) {
    static const char* n[] = {"float", "float", "float2", "float3", "float4", "uint", "int", "bool", "float4"};
    return n[std::clamp(t, 0, (int)kPinTypeCount)];
}
const char* PinTypeName(int t) {
    static const char* n[] = {"?", "float", "float2", "float3", "float4", "Texture", "int", "bool", "couleur"};
    return n[std::clamp(t, 0, (int)kPinTypeCount)];
}

EGOpDef EffectiveOp(const EGNode& n) {
    const EGOpDef* d = FindOp(n.op);
    EGOpDef r = d ? *d : EGOpDef{"", "?", "", {}, {}, 0};
    if (n.op == "custom" && n.HasPins()) {
        r.inputs.clear();
        for (auto& p : n.pins) r.inputs.push_back({p.name.c_str(), p.type == kEGTexture ? -5.0f : -7.0f});  // -7: parameter when unconnected
        r.outputs.clear();
        for (auto& o : n.outs) r.outputs.push_back(o.name.c_str());
        r.fixedOutType = 0;
    }
    return r;
}

const EGOpDef* FindOp(const std::string& op) {
    for (auto& o : EffectGraphOps()) if (op == o.op) return &o;
    return nullptr;
}

// ------------------------------------------------------------------ graph

EGNode* EffectGraph::Find(Id id) {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}
const EGNode* EffectGraph::Find(Id id) const {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}
Id EffectGraph::OutputNode() const {
    for (auto& n : nodes) if (n.op == "output") return n.id;
    return 0;
}

bool EffectGraph::WouldCycle(Id from, Id to) const {
    std::vector<Id> stack{from};
    std::set<Id> seen;
    while (!stack.empty()) {
        Id cur = stack.back();
        stack.pop_back();
        if (cur == to) return true;
        if (!seen.insert(cur).second) continue;
        if (auto* n = Find(cur)) for (auto& l : n->inputs) if (l.node) stack.push_back(l.node);
    }
    return false;
}

Json EffectGraph::ToJson() const {
    Json j = Json::Object();
    j["name"] = name;
    j["category"] = category;
    j["inputs"] = inputs;
    j["premultiplied"] = premultiplied;
    if (kind != EGKind::Effect) j["kind"] = (int)kind;
    if (kind == EGKind::Brush) {
        j["spacing"] = spacing;
        j["airbrush"] = airbrush;
        j["follow"] = follow;
        j["scatter"] = scatter;
        j["sizeJitter"] = sizeJitter;
        j["angleJitter"] = angleJitter;
        j["opacityJitter"] = opacityJitter;
    }
    j["nextId"] = nextId;
    Json arr = Json::Array();
    for (auto& n : nodes) {
        Json o = Json::Object();
        o["id"] = n.id;
        o["op"] = n.op;
        o["pos"] = sx::ToJson(n.pos);
        Json in = Json::Array();
        for (auto& l : n.inputs) {
            Json p = Json::Array();
            p.Push(l.node);
            p.Push(l.output);
            in.Push(p);
        }
        o["in"] = in;
        o["value"] = sx::ToJson(n.value);
        if (n.min != 0 || n.max != 1) { o["min"] = n.min; o["max"] = n.max; }
        if (!n.name.empty()) o["name"] = n.name;
        if (!n.text.empty()) o["text"] = n.text;
        if (n.index) o["index"] = n.index;
        auto pinsJson = [](const std::vector<EGPinDecl>& v) {
            // [name, type, default, min, max, category, label] (trailing defaults omitted)
            Json a = Json::Array();
            for (auto& p : v) {
                Json e = Json::Array();
                e.Push(p.name);
                e.Push(p.type);
                bool extra = !(p.def == float4{}) || p.min != 0 || p.max != 1 || !p.category.empty() || !p.label.empty();
                if (extra) {
                    e.Push(sx::ToJson(p.def));
                    e.Push(p.min);
                    e.Push(p.max);
                    e.Push(p.category);
                    e.Push(p.label);
                }
                a.Push(e);
            }
            return a;
        };
        if (!n.pins.empty()) o["pins"] = pinsJson(n.pins);
        if (!n.outs.empty()) o["outs"] = pinsJson(n.outs);
        if (!n.decl.empty()) o["decl"] = n.decl;
        arr.Push(o);
    }
    j["nodes"] = arr;
    return j;
}

bool EffectGraph::FromJson(const Json& j, EffectGraph& g) {
    if (!j.IsObject()) return false;
    g = EffectGraph{};
    g.name = j["name"].S("Effet visuel");
    g.category = j["category"].S("Graphes");
    g.inputs = std::clamp(j["inputs"].I(1), 0, 4);
    g.premultiplied = j["premultiplied"].B(false);
    g.kind = (EGKind)std::clamp(j["kind"].I(0), 0, 2);
    g.spacing = j["spacing"].F(0.1f);
    g.airbrush = j["airbrush"].F(0);
    g.follow = j["follow"].F(0);
    g.scatter = j["scatter"].F(0);
    g.sizeJitter = j["sizeJitter"].F(0);
    g.angleJitter = j["angleJitter"].F(0);
    g.opacityJitter = j["opacityJitter"].F(0);
    g.nextId = j["nextId"].U(1);
    for (auto& o : j["nodes"].Elements()) {
        EGNode n;
        n.id = o["id"].U();
        n.op = o["op"].S("");
        n.pos = Float2FromJson(o["pos"]);
        for (auto& p : o["in"].Elements()) n.inputs.push_back({p.Size() > 0 ? p.At(0).U() : 0, p.Size() > 1 ? p.At(1).I() : 0});
        n.value = Float4FromJson(o["value"]);
        n.min = o["min"].F(0);
        n.max = o["max"].F(1);
        n.name = o["name"].S("");
        n.text = o["text"].S("");
        n.index = o["index"].I(0);
        auto readPins = [](const Json& arr, std::vector<EGPinDecl>& out) {
            for (auto& p : arr.Elements()) {
                EGPinDecl d;
                d.name = p.At(0).S("");
                d.type = std::clamp(p.At(1).I(4), 1, (int)kPinTypeCount);
                if (p.Size() > 2) d.def = Float4FromJson(p.At(2));
                if (p.Size() > 3) d.min = p.At(3).F(0);
                if (p.Size() > 4) d.max = p.At(4).F(1);
                if (p.Size() > 5) d.category = p.At(5).S("");
                if (p.Size() > 6) d.label = p.At(6).S("");
                out.push_back(d);
            }
        };
        readPins(o["pins"], n.pins);
        readPins(o["outs"], n.outs);
        for (auto& p : n.outs) if (p.type == kPinTexture) p.type = kPinFloat4;
        n.decl = o["decl"].S("");
        n.inputs.resize(EffectiveOp(n).inputs.size());
        g.nextId = std::max(g.nextId, n.id + 1);
        g.nodes.push_back(n);
    }
    return true;
}

bool EffectGraph::Deserialize(const std::string& s, EffectGraph& out) {
    Json j;
    return Json::Parse(s, j) && FromJson(j, out);
}

EffectGraph EffectGraph::Default() {
    EffectGraph g;
    EGNode in;
    in.id = g.nextId++;
    in.op = "input";
    in.pos = {40, 80};
    EGNode out;
    out.id = g.nextId++;
    out.op = "output";
    out.pos = {420, 80};
    out.inputs = {{in.id, 0}};
    g.nodes = {in, out};
    return g;
}

namespace {
EGNode& AddNode(EffectGraph& g, const char* op, float2 pos, int index = 0) {
    EGNode n;
    n.id = g.nextId++;
    n.op = op;
    n.pos = pos;
    n.index = index;
    if (const EGOpDef* d = FindOp(op)) n.inputs.resize(d->inputs.size());
    g.nodes.push_back(n);
    return g.nodes.back();
}
}  // namespace

EffectGraph EffectGraph::DefaultBlend() {
    // A x B (Produit) composed over A with the layer's mask and opacity.
    EffectGraph g;
    g.kind = EGKind::Blend;
    g.name = "Fusion visuelle";
    g.category = "Mes fusions";
    g.inputs = 3;
    Id a = AddNode(g, "input", {40, 40}, 0).id, b = AddNode(g, "input", {40, 230}, 1).id, m = AddNode(g, "input", {40, 420}, 2).id;
    Id mul = AddNode(g, "mul", {300, 120}).id;
    g.Find(mul)->inputs = {{a, 1}, {b, 1}};
    Id over = AddNode(g, "blendover", {540, 140}).id;
    g.Find(over)->inputs = {{a, 0}, {b, 0}, {mul, 0}, {m, 2}, {}};
    Id out = AddNode(g, "output", {820, 160}).id;
    g.Find(out)->inputs = {{over, 0}, {}, {}};
    return g;
}

EffectGraph EffectGraph::DefaultBrush() {
    // Round tip with the brush color.
    EffectGraph g;
    g.kind = EGKind::Brush;
    g.name = "Pinceau visuel";
    g.category = "Mes pinceaux";
    g.inputs = 0;
    Id br = AddNode(g, "brush", {40, 40}).id;
    Id tip = AddNode(g, "roundtip", {340, 60}).id;
    g.Find(tip)->inputs = {{br, 0}, {br, 5}};
    Id out = AddNode(g, "output", {620, 60}).id;
    g.Find(out)->inputs = {{}, {br, 4}, {tip, 0}};
    return g;
}

// ------------------------------------------------------------------ codegen

namespace {
struct Val {
    std::string expr;
    int n = 1;
};

std::string F(float v) {
    char b[48];
    snprintf(b, sizeof(b), "%.7g", v);
    std::string s = b;
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    return s;
}

std::string Type(int n) { return n == kEGTexture ? "uint" : n == 1 ? "float" : "float" + std::to_string(n); }

std::string Conv(const Val& v, int n) {
    if (v.n == n) return v.expr;
    if (v.n == 1) return "((" + Type(n) + ")(" + v.expr + "))";
    if (v.n > n) {
        static const char* sw[] = {"", ".x", ".xy", ".xyz"};
        return "(" + v.expr + ")" + sw[n];
    }
    // widen
    if (v.n == 2 && n == 3) return "float3(" + v.expr + ", 0.0)";
    if (v.n == 2 && n == 4) return "float4(" + v.expr + ", 0.0, 1.0)";
    return "float4(" + v.expr + ", 1.0)";  // 3 -> 4
}

std::string SanitizeId(const std::string& s, int fallback) {
    std::string out;
    for (char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9' && !out.empty())) out += c;
    }
    if (out.empty()) out = "p" + std::to_string(fallback);
    return "P_" + out;  // prefix avoids clashes with HLSL keywords / intrinsics
}

// Helpers shared by both backends (pure math, usable inside an Unreal Custom node).
const char* kHelperStruct =
    "struct SXH\n{\n"
    "    float Hash(float2 p) { float3 p3 = frac(float3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return frac((p3.x + p3.y) * p3.z); }\n"
    "    float Noise(float2 p) { float2 i = floor(p); float2 f = frac(p); float2 u = f * f * (3.0 - 2.0 * f);\n"
    "        return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), u.x), lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), u.x), u.y); }\n"
    "    float3 RGBToHSV(float3 c) { float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);\n"
    "        float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));\n"
    "        float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));\n"
    "        float d = q.x - min(q.w, q.y); return float3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)), d / (q.x + 1e-10), q.x); }\n"
    "    float3 HSVToRGB(float3 c) { float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n"
    "        float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www); return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y); }\n"
    "    float ToSrgb1(float c) { c = max(c, 0.0); return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }\n"
    "    float ToLinear1(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }\n"
    "    float3 ToSrgb(float3 c) { return float3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b)); }\n"
    "    float3 ToLinear(float3 c) { return float3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b)); }\n"
    "    float4 F4(float3 v) { return float4(v, 1.0); }\n"
    "    float4 Over(float4 A, float4 B, float3 m, float k) { float ab = saturate(B.a * k); float a = ab + A.a * (1.0 - ab);\n"
    "        float3 rgb = ab * (1.0 - A.a) * B.rgb + ab * A.a * m + (1.0 - ab) * A.a * A.rgb; return float4(a > 0.0 ? rgb / a : (float3)0.0, a); }\n"
    "    float4 F4(float4 v) { return v; }\n"
    "};\n";

struct Gen {
    const EffectGraph& g;
    bool unreal;
    std::vector<std::string> lines;
    std::vector<std::string> customStructs;
    std::vector<std::string> customDecls;
    bool textures = false;  // a custom node reads textures: Texture2DSample helpers needed
    std::string opacityVar = "opacity";
    // Unconnected custom pins exposed as effect parameters.
    struct PinParam { std::string id; EGPinDecl pin; };
    std::vector<PinParam> pinParams;  // blends: the layer opacity (an "Opacity" parameter of the graph, else a generated one)
    std::unordered_map<Id, std::vector<Val>> memo;
    std::set<Id> visiting;
    std::set<int> usedInputs;
    std::string error;
    std::vector<Id> errorNodes;
    std::unordered_map<Id, std::string> paramNames;

    bool brush() const { return g.kind == EGKind::Brush; }
    std::string UV() const { return unreal ? "UV" : brush() ? "b.uv" : "ctx.uv"; }
    std::string Pixel() const { return unreal ? "(UV * Size)" : brush() ? "b.px" : "float2(ctx.px)"; }
    std::string Size() const { return unreal ? "Size" : brush() ? "b.docSize" : "ctx.size"; }
    std::string SampleAt(int i, const std::string& uv) const {
        if (unreal) return "Texture2DSample(Tex" + std::to_string(i) + ", Tex" + std::to_string(i) + "Sampler, " + uv + ")";
        return "InputUV(" + std::to_string(i) + ", " + uv + ")";
    }

    Val Arg(const EGNode& n, size_t i, float def) {
        if (i < n.inputs.size() && n.inputs[i].node) {
            auto outs = Eval(n.inputs[i].node);
            int k = n.inputs[i].output;
            if (k >= 0 && k < (int)outs.size()) return outs[k];
        }
        if (def == -1) return {UV(), 2};  // pins defaulting to the image UV
        if (def == -2) return {opacityVar, 1};  // layer opacity (blends)
        if (def == -5) return {"31u", kEGTexture};  // unconnected texture: reads white
        if (def == -7) return {"0.0", 1};           // custom pin: parameter (see the custom node)
        if (def == -3) return {brush() ? std::string("b.local") : "(" + UV() + " * 2.0 - 1.0)", 2};
        if (def == -4) return {brush() ? "b.hardness" : "0.8", 1};
        return {F(def), 1};
    }

    std::vector<Val> Eval(Id id) {
        auto it = memo.find(id);
        if (it != memo.end()) return it->second;
        const EGNode* n = g.Find(id);
        if (!n) return {{"0.0", 1}};
        if (!FindOp(n->op)) { Fail(id, "opération inconnue : " + n->op); return {{"0.0", 1}}; }
        const EGOpDef effective = EffectiveOp(*n);
        const EGOpDef* d = &effective;
        if (!visiting.insert(id).second) { Fail(id, "cycle dans le graphe"); return {{"0.0", 1}}; }
        std::vector<Val> a;
        for (size_t i = 0; i < d->inputs.size(); ++i) a.push_back(Arg(*n, i, d->inputs[i].defValue));
        visiting.erase(id);
        const bool pinned = n->op == "custom" && n->HasPins();
        for (size_t i = 0; i < a.size(); ++i) {
            bool wantTex = pinned && n->pins[i].type == kEGTexture;
            if ((a[i].n == kEGTexture) != wantTex) {
                Fail(id, wantTex ? "une broche texture attend une « Texture d'entrée »" : "une texture ne se branche que sur une broche texture d'un nœud HLSL");
                a[i] = wantTex ? Val{"31u", kEGTexture} : Val{"0.0", 1};
            }
        }

        int widest = 1;
        for (auto& v : a) widest = std::max(widest, v.n);
        auto W = [&](size_t i) { return Conv(a[i], widest); };
        auto S = [&](size_t i) { return Conv(a[i], 1); };
        std::vector<Val> out;
        const std::string& op = n->op;
        auto one = [&](std::string e, int k) { out.push_back({std::move(e), k}); };
        std::string idx = std::to_string(std::clamp(n->index, 0, 3));
        // Image reads: one load, then RGBA, RGB, R, G, B, A outputs.
        auto channels = [&](const std::string& load) {
            std::string v = "n" + std::to_string(id) + "_img";
            lines.push_back("    float4 " + v + " = " + load + ";");
            one(v, 4);
            one(v + ".rgb", 3);
            for (const char* c : {".r", ".g", ".b", ".a"}) one(v + c, 1);
        };
        if ((op == "input" || op == "sample") && brush()) {
            Fail(id, "un pinceau n'a pas d'image d'entrée");
            one("float4(0.0, 0.0, 0.0, 0.0)", 4);
        } else if (op == "brush") {
            if (!brush()) Fail(id, "nœud réservé aux pinceaux");
            one("b.local", 2); one("b.uv", 2); one("b.px", 2); one("b.pressure", 1); one("b.color", 3); one("b.hardness", 1);
            one("b.flow", 1); one("b.radius", 1); one("b.seed", 1); one("b.time", 1); one("b.distance", 1); one("b.direction", 2);
        } else if (op == "roundtip") {
            one("(1.0 - smoothstep(saturate(" + S(1) + ") * 0.999, 1.0, length(" + Conv(a[0], 2) + ")))", 1);
        } else if (op == "random") {
            if (!brush()) Fail(id, "nœud réservé aux pinceaux");
            one("Rand(b.seed, " + S(0) + " + 11.0)", 1);
        } else if (op == "texture") {
            int k = std::clamp(n->index, 0, 3);
            usedInputs.insert(k);
            one(std::to_string(k) + "u", kEGTexture);
        } else if (op == "canvassize") {
            one(unreal ? "float4(Size, 1.0 / Size)" : brush() ? "float4(b.docSize, 1.0 / b.docSize)" : "float4(ctx.size, ctx.texel)", 4);
        } else if (op == "param2") {
            one(paramNames[id], 2);
        } else if (op == "custom" && pinned) {
            // Declared pins: Run(<pins>) returns the single output, or writes several `out` parameters.
            std::string sname = "SXC" + std::to_string(id), params;
            for (size_t i = 0; i < n->pins.size(); ++i) {
                if (i) params += ", ";
                params += std::string(PinHlslType(n->pins[i].type)) + " " + n->pins[i].name;
                if (n->pins[i].type == kEGTexture) textures = true;
            }
            const bool single = n->outs.size() <= 1;
            int retType = n->outs.empty() ? kPinFloat4 : n->outs[0].type;
            if (!single)
                for (auto& o : n->outs) params += std::string(params.empty() ? "" : ", ") + "out " + PinHlslType(o.type) + " " + o.name;
            std::string st = "struct " + sname + "\n{\n    " + (single ? std::string(PinHlslType(retType)) : std::string("void")) + " Run(" + params +
                             ")\n    {\n" + n->text + "\n    }\n};\n";
            std::string qualified = sname;
            if (!n->decl.empty() && !unreal) {
                // Declarations live in a namespace with the node: their names never clash with
                // the prelude (Luma...) or with another node's helpers.
                std::string ns = "SXD" + std::to_string(id);
                customDecls.push_back("// " + n->name + "\nnamespace " + ns + "\n{\n" + n->decl + "\n" + st + "}\n");
                qualified = ns + "::" + sname;
            } else {
                if (!n->decl.empty()) customDecls.push_back("// " + n->name + "\n" + n->decl + "\n");
                customStructs.push_back(st);
            }
            std::string inst = "sxc" + std::to_string(id), args;
            lines.push_back("    " + qualified + " " + inst + ";");
            auto toPin = [&](const Val& v, int t) -> std::string {
                if (t == kEGTexture) return v.expr;
                if (t == kPinInt) return "(int)(" + Conv(v, 1) + ")";
                if (t == kPinBool) return "(" + Conv(v, 1) + " > 0.5)";
                return Conv(v, PinValueSize(t));
            };
            for (size_t i = 0; i < a.size(); ++i) {
                const EGPinDecl& pin = n->pins[i];
                bool linked = i < n->inputs.size() && n->inputs[i].node;
                std::string expr;
                if (!linked && g.kind == EGKind::Blend && _stricmp(pin.name.c_str(), "opacity") == 0 && PinValueSize(pin.type) == 1) {
                    expr = opacityVar;  // blends: the layer opacity
                } else if (!linked && pin.type != kEGTexture) {
                    // Unconnected: a parameter of the effect (default, range, category).
                    std::string pid = "P_n" + std::to_string(id) + "_" + SanitizeId(pin.name, (int)i).substr(2);
                    if (std::find_if(pinParams.begin(), pinParams.end(), [&](const PinParam& q) { return q.id == pid; }) == pinParams.end())
                        pinParams.push_back({pid, pin});
                    expr = pin.type == kPinBool ? "(" + pid + " != 0)" : pin.type == kPinInt ? pid
                                                                             : Conv({pid, PinValueSize(pin.type)}, PinValueSize(pin.type));
                } else {
                    expr = toPin(a[i], pin.type);
                }
                args += (i ? ", " : "") + expr;
            }
            if (single) one(inst + ".Run(" + args + ")", PinValueSize(retType));
            else {
                for (size_t k = 0; k < n->outs.size(); ++k) {
                    std::string var = "n" + std::to_string(id) + "_o" + std::to_string(k);
                    lines.push_back("    " + std::string(PinHlslType(n->outs[k].type)) + " " + var + ";");
                    args += std::string(args.empty() ? "" : ", ") + var;
                }
                lines.push_back("    " + inst + ".Run(" + args + ");");
                for (size_t k = 0; k < n->outs.size(); ++k) {
                    std::string var = "n" + std::to_string(id) + "_o" + std::to_string(k);
                    // int / bool outputs continue as float values in the graph.
                    if (n->outs[k].type == kPinInt || n->outs[k].type == kPinBool) var = "(float)(" + var + ")";
                    out.push_back({var, PinValueSize(n->outs[k].type)});
                }
            }
            if (single && (retType == kPinInt || retType == kPinBool)) out.back().expr = "(float)(" + out.back().expr + ")";
        } else if (op == "opacity") {
            one(opacityVar, 1);
        } else if (op == "blendover") {
            one("sxh.Over(" + Conv(a[0], 4) + ", " + Conv(a[1], 4) + ", " + Conv(a[2], 3) + ", " + S(3) + " * " + S(4) + ")", 4);
        } else if (op == "input") {
            usedInputs.insert(std::clamp(n->index, 0, 3));
            channels(unreal ? SampleAt(std::clamp(n->index, 0, 3), "UV") : "Input(" + idx + ", ctx.px)");
        } else if (op == "sample") {
            usedInputs.insert(std::clamp(n->index, 0, 3));
            channels(SampleAt(std::clamp(n->index, 0, 3), Conv(a[0], 2)));
        } else if (op == "uv") one(UV(), 2);
        else if (op == "pixel") one(Pixel(), 2);
        else if (op == "size") one(Size(), 2);
        else if (op == "const") one(F(n->value.x), 1);
        else if (op == "const2") one("float2(" + F(n->value.x) + ", " + F(n->value.y) + ")", 2);
        else if (op == "color") one("float4(" + F(n->value.x) + ", " + F(n->value.y) + ", " + F(n->value.z) + ", " + F(n->value.w) + ")", 4);
        else if (op == "param") one(paramNames[id], 1);
        else if (op == "paramcolor") one(unreal ? "sxh.F4(" + paramNames[id] + ")" : paramNames[id], 4);
        else if (op == "add") one("(" + W(0) + " + " + W(1) + ")", widest);
        else if (op == "sub") one("(" + W(0) + " - " + W(1) + ")", widest);
        else if (op == "mul") one("(" + W(0) + " * " + W(1) + ")", widest);
        else if (op == "div") one("(" + W(0) + " / " + W(1) + ")", widest);
        else if (op == "min") one("min(" + W(0) + ", " + W(1) + ")", widest);
        else if (op == "max") one("max(" + W(0) + ", " + W(1) + ")", widest);
        else if (op == "pow") one("pow(max(" + W(0) + ", 0.0), " + W(1) + ")", widest);
        else if (op == "lerp") {
            int k = std::max(a[0].n, a[1].n);
            std::string t = a[2].n == 1 ? a[2].expr : Conv(a[2], k);
            one("lerp(" + Conv(a[0], k) + ", " + Conv(a[1], k) + ", " + t + ")", k);
        } else if (op == "clamp") one("clamp(" + W(0) + ", " + W(1) + ", " + W(2) + ")", widest);
        else if (op == "smoothstep") one("smoothstep(" + W(0) + ", " + W(1) + ", " + W(2) + ")", widest);
        else if (op == "step") one("step(" + W(0) + ", " + W(1) + ")", widest);
        else if (op == "oneminus") one("(1.0 - " + a[0].expr + ")", a[0].n);
        else if (op == "abs" || op == "frac" || op == "floor" || op == "ceil" || op == "sin" || op == "cos" || op == "exp2" || op == "saturate" || op == "normalize")
            one(op + "(" + a[0].expr + ")", a[0].n);
        else if (op == "negate") one("(-" + a[0].expr + ")", a[0].n);
        else if (op == "sqrt") one("sqrt(max(" + a[0].expr + ", 0.0))", a[0].n);
        else if (op == "log2") one("log2(max(" + a[0].expr + ", 1e-6))", a[0].n);
        else if (op == "if") one("((" + S(0) + " > " + S(1) + ") ? " + Conv(a[2], std::max(a[2].n, a[3].n)) + " : " + Conv(a[3], std::max(a[2].n, a[3].n)) + ")", std::max(a[2].n, a[3].n));
        else if (op == "dot") { int k = std::max(a[0].n, a[1].n); one("dot(" + Conv(a[0], k) + ", " + Conv(a[1], k) + ")", 1); }
        else if (op == "length") one("length(" + a[0].expr + ")", 1);
        else if (op == "distance") { int k = std::max(a[0].n, a[1].n); one("distance(" + Conv(a[0], k) + ", " + Conv(a[1], k) + ")", 1); }
        else if (op == "split") {
            std::string v = Conv(a[0], 4);
            for (const char* c : {".x", ".y", ".z", ".w"}) out.push_back({"(" + v + ")" + c, 1});
        } else if (op == "append") {
            int k = std::min(a[0].n + a[1].n, 4);
            int nb = k - a[0].n;
            one("float" + std::to_string(k) + "(" + a[0].expr + ", " + Conv(a[1], std::max(nb, 1)) + ")", k);
            if (k == 1) out.back() = a[0];
        } else if (op == "make4") one("float4(" + S(0) + ", " + S(1) + ", " + S(2) + ", " + S(3) + ")", 4);
        else if (op == "make3") one("float3(" + S(0) + ", " + S(1) + ", " + S(2) + ")", 3);
        else if (op == "make2") one("float2(" + S(0) + ", " + S(1) + ")", 2);
        else if (op == "mask") {
            std::string sw;
            const char* comps = "xyzw";
            int bits = n->index ? n->index : 7;
            for (int i = 0; i < 4; ++i) if (bits & (1 << i)) sw += comps[i];
            if (sw.empty()) sw = "x";
            one("(" + Conv(a[0], 4) + ")." + sw, (int)sw.size());
        } else if (op == "luma") one("dot(" + Conv(a[0], 3) + ", float3(0.2126, 0.7152, 0.0722))", 1);
        else if (op == "desaturate") {
            std::string c = Conv(a[0], 3);
            one("lerp(" + c + ", (float3)dot(" + c + ", float3(0.2126, 0.7152, 0.0722)), " + S(1) + ")", 3);
        } else if (op == "rgb2hsv") one("sxh.RGBToHSV(" + Conv(a[0], 3) + ")", 3);
        else if (op == "hsv2rgb") one("sxh.HSVToRGB(" + Conv(a[0], 3) + ")", 3);
        else if (op == "tosrgb") one("sxh.ToSrgb(" + Conv(a[0], 3) + ")", 3);
        else if (op == "tolinear") one("sxh.ToLinear(" + Conv(a[0], 3) + ")", 3);
        else if (op == "noise") one("sxh.Noise(" + Conv(a[0], 2) + " * " + S(1) + ")", 1);
        else if (op == "checker") {
            std::string c = "floor(" + Conv(a[0], 2) + " * " + S(1) + ")";
            one("frac((" + c + ").x * 0.5 + (" + c + ").y * 0.5) * 2.0", 1);
        } else if (op == "rotateuv") {
            std::string c = Conv(a[2], 2), uv = Conv(a[0], 2), ang = S(1);
            std::string dd = "(" + uv + " - " + c + ")";
            one("(" + c + " + float2(cos(" + ang + ") * " + dd + ".x - sin(" + ang + ") * " + dd + ".y, sin(" + ang + ") * " + dd + ".x + cos(" + ang + ") * " + dd + ".y))", 2);
        } else if (op == "custom") {
            std::string sname = "SXC" + std::to_string(id);
            customStructs.push_back("struct " + sname + "\n{\n    float4 Run(float4 A, float4 B, float4 C, float4 D)\n    {\n" + n->text + "\n    }\n};\n");
            std::string inst = "sxc" + std::to_string(id);
            lines.push_back("    " + sname + " " + inst + ";");
            one(inst + ".Run(" + Conv(a[0], 4) + ", " + Conv(a[1], 4) + ", " + Conv(a[2], 4) + ", " + Conv(a[3], 4) + ")", 4);
        } else if (op == "output") {
            // RGBA, then the RGB and Alpha pins override their part when connected.
            auto linked = [&](size_t i) { return i < n->inputs.size() && n->inputs[i].node; };
            std::string c = Conv(a[0], 4);
            if (linked(1)) c = "float4(" + Conv(a[1], 3) + ", (" + c + ").a)";
            if (linked(2)) c = "float4((" + c + ").rgb, " + S(2) + ")";
            if (!linked(0) && !linked(1) && linked(2)) c = "float4(0.0, 0.0, 0.0, " + S(2) + ")";
            one(c, 4);
        } else {
            Fail(id, "opération non gérée : " + op);
            one("0.0", 1);
        }
        // Materialize every output into a local so shared subexpressions are computed once.
        const bool multiOut = pinned && n->outs.size() > 1;
        if (op != "output" && op != "param" && op != "paramcolor" && op != "param2" && op != "uv" && op != "const" && op != "brush" && op != "opacity" &&
            op != "texture" && !multiOut) {
            for (size_t k = 0; k < out.size(); ++k) {
                std::string var = "n" + std::to_string(id) + "_" + std::to_string(k);
                lines.push_back("    " + Type(out[k].n) + " " + var + " = " + out[k].expr + ";");
                out[k].expr = var;
            }
        }
        memo[id] = out;
        return out;
    }

    void Fail(Id id, const std::string& m) {
        if (error.empty()) error = m;
        errorNodes.push_back(id);
    }
};

void AssignParamNames(const EffectGraph& g, Gen& gen) {
    std::set<std::string> used;
    int k = 0;
    for (auto& n : g.nodes) {
        if (n.op != "param" && n.op != "paramcolor" && n.op != "param2") continue;
        std::string base = SanitizeId(n.name, ++k), id = base;
        if (n.index == 1 && !n.name.empty() && base == "P_" + n.name) base = id = n.name;  // imported: the original identifier
        for (int i = 2; used.count(id); ++i) id = base + "_" + std::to_string(i);
        used.insert(id);
        gen.paramNames[n.id] = id;
    }
}
}  // namespace

EGCompileResult CompileEffectGraph(const EffectGraph& g) {
    EGCompileResult r;
    Id out = g.OutputNode();
    if (!out) {
        r.error = "le graphe n'a pas de nœud de sortie";
        return r;
    }
    Gen gen{g, false};
    AssignParamNames(g, gen);
    bool ownOpacity = false;
    for (auto& n : g.nodes)
        if (n.op == "param" && _stricmp(gen.paramNames[n.id].c_str(), "opacity") == 0) {
            gen.opacityVar = gen.paramNames[n.id];  // driven by the layer opacity (case-insensitive id)
            ownOpacity = true;
        }
    std::vector<Val> res = gen.Eval(out);
    r.error = gen.error;
    r.errorNodes = gen.errorNodes;
    {
        Gen typer{g, false};
        typer.paramNames = gen.paramNames;
        typer.opacityVar = gen.opacityVar;
        for (auto& n : g.nodes) {
            auto outs = typer.Eval(n.id);
            for (size_t k = 0; k < outs.size(); ++k) r.outTypes[(u64)n.id * 16 + k] = outs[k].n;
        }
    }

    std::string s;
    const bool brush = g.kind == EGKind::Brush, blend = g.kind == EGKind::Blend;
    s += "//@name " + g.name + "\n";
    s += "//@category " + g.category + "\n";
    if (brush) {
        s += "//@spacing " + F(std::clamp(g.spacing, 0.01f, 4.0f)) + "\n";
        if (g.airbrush > 0) s += "//@airbrush " + F(g.airbrush) + "\n";
        if (g.scatter > 0) s += "//@extent " + F(1.0f + g.scatter) + "\n";
    } else if (blend) {
        s += "//@blend\n//@input A (dessous)\n//@input B (calque)\n//@input Masque\n";
    } else {
        if (g.inputs == 0) s += "//@generator\n";
        for (int i = 0; i < g.inputs; ++i) s += "//@input " + std::string(i == 0 ? "Image" : "Image " + std::to_string(i + 1)) + "\n";
    }
    if (g.premultiplied && !brush) s += "//@premultiplied\n";
    if (!brush) {
        bool global = false;
        for (auto& n : g.nodes) if (n.op == "sample" || n.op == "custom") global = true;
        s += global ? "//@halo full\n" : "//@halo 0\n";
    }
    if (blend && !ownOpacity) s += "//@param opacity float 1 0 1 \"Opacité\"\n";  // driven by the layer opacity
    for (auto& n : g.nodes) {
        auto it = gen.paramNames.find(n.id);
        if (it == gen.paramNames.end()) continue;
        std::string label = !n.text.empty() ? n.text : n.name.empty() ? it->second : n.name;  // text: display label (imported)
        if (n.op == "param")
            s += "//@param " + it->second + " float " + F(n.value.x) + " " + F(n.min) + " " + F(n.max) + " \"" + label + "\"\n";
        else if (n.op == "param2")
            s += "//@param " + it->second + " float2 " + F(n.value.x) + " " + F(n.value.y) + " " + F(n.min) + " " + F(n.max) + " \"" + label + "\"\n";
        else
            s += "//@param " + it->second + " color " + F(n.value.x) + " " + F(n.value.y) + " " + F(n.value.z) + " " + F(n.value.w) + " \"" + label + "\"\n";
    }
    {
        // Parameters of unconnected custom pins, grouped by category.
        std::vector<std::string> cats;
        for (auto& pp : gen.pinParams)
            if (std::find(cats.begin(), cats.end(), pp.pin.category) == cats.end()) cats.push_back(pp.pin.category);
        std::stable_sort(cats.begin(), cats.end(), [](const std::string& a, const std::string& b) { return a.empty() && !b.empty(); });
        for (auto& cat : cats) {
            s += "//@group " + cat + "\n";
            for (auto& pp : gen.pinParams) {
                if (pp.pin.category != cat) continue;
                const EGPinDecl& p = pp.pin;
                std::string label = "\"" + (p.label.empty() ? p.name : p.label) + "\"";
                switch (p.type) {
                    case kPinFloat: s += "//@param " + pp.id + " float " + F(p.def.x) + " " + F(p.min) + " " + F(p.max) + " " + label + "\n"; break;
                    case kPinInt: s += "//@param " + pp.id + " int " + F(std::round(p.def.x)) + " " + F(p.min) + " " + F(p.max) + " " + label + "\n"; break;
                    case kPinBool: s += "//@param " + pp.id + " bool " + F(p.def.x > 0.5f ? 1.0f : 0.0f) + " " + label + "\n"; break;
                    case kPinFloat2: s += "//@param " + pp.id + " float2 " + F(p.def.x) + " " + F(p.def.y) + " " + F(p.min) + " " + F(p.max) + " " + label + "\n"; break;
                    case kPinFloat3:
                        s += "//@param " + pp.id + " float3 " + F(p.def.x) + " " + F(p.def.y) + " " + F(p.def.z) + " " + F(p.min) + " " + F(p.max) + " " + label + "\n";
                        break;
                    case kPinFloat4:
                        s += "//@param " + pp.id + " float4 " + F(p.def.x) + " " + F(p.def.y) + " " + F(p.def.z) + " " + F(p.def.w) + " " + F(p.min) + " " + F(p.max) + " " +
                             label + "\n";
                        break;
                    default:
                        s += "//@param " + pp.id + " color " + F(p.def.x) + " " + F(p.def.y) + " " + F(p.def.z) + " " + F(p.def.w) + " " + label + "\n";
                        break;
                }
            }
        }
        if (!gen.pinParams.empty()) s += "//@group\n";
    }
    s += "// Généré par l'éditeur visuel de Shaderixel.\n";
    s += kHelperStruct;
    if (gen.textures) {
        // Textures of custom nodes: read at any UV; unconnected = white (like Unreal).
        s += "float4 SXTex(uint i, float2 uv) { return i < 4u && HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }\n";
        s += "#define Texture2DSample(T, S, UVS) SXTex(T, UVS)\n#define Texture2DSampleLevel(T, S, UVS, LOD) SXTex(T, UVS)\n";
    }
    for (auto& c : gen.customDecls) s += c;
    for (auto& c : gen.customStructs) s += c;
    s += brush ? "float4 Tip(Brush b)\n{\n    SXH sxh;\n" : "float4 Effect(EffectContext ctx)\n{\n    SXH sxh;\n";
    for (auto& l : gen.lines) s += l + "\n";
    s += "    return " + (res.empty() ? std::string("float4(0, 0, 0, 0)") : Conv(res[0], 4)) + ";\n}\n";
    if (brush && (g.follow != 0 || g.scatter != 0 || g.sizeJitter != 0 || g.angleJitter != 0 || g.opacityJitter != 0)) {
        // Placement of each dab: follow the stroke, scatter, random size / angle / opacity.
        s += "\nvoid Dynamics(inout Dab d)\n{\n";
        s += "    d.angle = " + F(g.follow) + " * atan2(d.direction.y, d.direction.x) + (Rand(d.seed, 4.0) - 0.5) * 6.2831853 * " + F(g.angleJitter) + ";\n";
        s += "    d.pos += (Rand2(d.seed) - 0.5) * 2.0 * d.radius * " + F(g.scatter) + ";\n";
        s += "    d.radius *= lerp(1.0, Rand(d.seed, 3.0), " + F(g.sizeJitter) + ");\n";
        s += "    d.opacity *= lerp(1.0, Rand(d.seed, 6.0), " + F(g.opacityJitter) + ");\n";
        s += "}\n";
    }
    r.code = s;
    return r;
}

EGUnrealExport ExportEffectGraphToUnreal(const EffectGraph& g, const std::string& materialName, const std::string& packagePath) {
    EGUnrealExport ex;
    Id out = g.OutputNode();
    if (!out) {
        ex.error = "le graphe n'a pas de nœud de sortie";
        return ex;
    }
    Gen gen{g, true};
    AssignParamNames(g, gen);
    std::vector<Val> res = gen.Eval(out);
    if (!gen.error.empty()) {
        ex.error = gen.error;
        return ex;
    }
    std::string code;
    code += kHelperStruct;
    if (gen.textures) {
        ex.error = "les textures des nœuds HLSL personnalisés ne sont pas encore exportées vers Unreal";
        return ex;
    }
    for (auto& c : gen.customDecls) code += c;
    for (auto& c : gen.customStructs) code += c;
    code += "SXH sxh;\n";
    for (auto& l : gen.lines) code += l.substr(4) + "\n";
    code += "return " + Conv(res[0], 4) + ";\n";
    ex.customCode = code;

    auto pyStr = [](const std::string& s) {
        std::string o = "\"\"\"";
        for (char c : s) {
            if (c == '\\') o += "\\\\";
            else if (c == '"') o += "\\\"";
            else o += c;
        }
        return o + "\"\"\"";
    };
    std::string py;
    py += "# Généré par Shaderixel : crée le matériau '" + materialName + "' dans " + packagePath + "\n";
    py += "# Unreal Editor > Outils > Exécuter un script Python (plugin Python Editor Script activé).\n";
    py += "import unreal\n\n";
    py += "CODE = " + pyStr(code) + "\n\n";
    py += "tools = unreal.AssetToolsHelpers.get_asset_tools()\n";
    py += "mel = unreal.MaterialEditingLibrary\n";
    py += "path = '" + packagePath + "'\n";
    py += "name = '" + materialName + "'\n";
    py += "if unreal.EditorAssetLibrary.does_asset_exist(path + '/' + name):\n";
    py += "    unreal.EditorAssetLibrary.delete_asset(path + '/' + name)\n";
    py += "mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())\n";
    py += "mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)\n";
    py += "mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)\n\n";
    py += "custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 0)\n";
    py += "custom.set_editor_property('code', CODE)\n";
    py += "custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)\n";
    py += "custom.set_editor_property('description', 'Shaderixel " + materialName + "')\n";
    py += "inputs = []\n";
    py += "def add_input(n):\n    ci = unreal.CustomInput()\n    ci.set_editor_property('input_name', n)\n    inputs.append(ci)\n\n";
    py += "add_input('UV')\nadd_input('Size')\n";
    for (int i : gen.usedInputs) py += "add_input('Tex" + std::to_string(i) + "')\n";
    for (auto& n : g.nodes) {
        auto it = gen.paramNames.find(n.id);
        if (it != gen.paramNames.end()) py += "add_input('" + it->second + "')\n";
    }
    py += "custom.set_editor_property('inputs', inputs)\n\n";
    py += "uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, -200)\n";
    py += "mel.connect_material_expressions(uv, '', custom, 'UV')\n";
    py += "size = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, -100)\n";
    py += "size.set_editor_property('parameter_name', 'ImageSize')\n";
    py += "size.set_editor_property('default_value', unreal.LinearColor(1024, 1024, 0, 0))\n";
    py += "mask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -700, -100)\n";
    py += "mask.set_editor_property('r', True)\nmask.set_editor_property('g', True)\nmask.set_editor_property('b', False)\nmask.set_editor_property('a', False)\n";
    py += "mel.connect_material_expressions(size, '', mask, '')\n";
    py += "mel.connect_material_expressions(mask, '', custom, 'Size')\n";
    int y = 0;
    for (int i : gen.usedInputs) {
        std::string t = "tex" + std::to_string(i);
        py += t + " = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -900, " + std::to_string(y) + ")\n";
        py += t + ".set_editor_property('parameter_name', 'Input" + std::to_string(i) + "')\n";
        py += "mel.connect_material_expressions(" + t + ", '', custom, 'Tex" + std::to_string(i) + "')\n";
        y += 150;
    }
    for (auto& n : g.nodes) {
        auto it = gen.paramNames.find(n.id);
        if (it == gen.paramNames.end()) continue;
        std::string v = "p" + std::to_string(n.id);
        std::string label = !n.text.empty() ? n.text : n.name.empty() ? it->second : n.name;  // text: display label (imported)
        for (auto& c : label) if (c == '\'') c = ' ';
        if (n.op == "param") {
            py += v + " = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, " + std::to_string(y) + ")\n";
            py += v + ".set_editor_property('default_value', " + F(n.value.x) + ")\n";
        } else {
            py += v + " = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, " + std::to_string(y) + ")\n";
            py += v + ".set_editor_property('default_value', unreal.LinearColor(" + F(n.value.x) + ", " + F(n.value.y) + ", " + F(n.value.z) + ", " + F(n.value.w) + "))\n";
        }
        py += v + ".set_editor_property('parameter_name', '" + label + "')\n";
        py += "mel.connect_material_expressions(" + v + ", '', custom, '" + it->second + "')\n";
        y += 120;
    }
    py += "\nrgb = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -250, 0)\n";
    py += "rgb.set_editor_property('r', True)\nrgb.set_editor_property('g', True)\nrgb.set_editor_property('b', True)\nrgb.set_editor_property('a', False)\n";
    py += "mel.connect_material_expressions(custom, '', rgb, '')\n";
    py += "alpha = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -250, 150)\n";
    py += "alpha.set_editor_property('a', True)\nalpha.set_editor_property('r', False)\nalpha.set_editor_property('g', False)\nalpha.set_editor_property('b', False)\n";
    py += "mel.connect_material_expressions(custom, '', alpha, '')\n";
    py += "mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)\n";
    py += "mel.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)\n";
    py += "mel.layout_material_expressions(mat)\n";
    py += "mel.recompile_material(mat)\n";
    py += "unreal.EditorAssetLibrary.save_asset(path + '/' + name)\n";
    py += "unreal.log('Shaderixel: material ' + name + ' created')\n";
    ex.python = py;
    return ex;
}

}  // namespace sx
