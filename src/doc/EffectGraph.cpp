#include "doc/EffectGraph.h"

#include <cstdio>
#include <set>
#include <unordered_map>

namespace sx {

// ------------------------------------------------------------------ registry

const std::vector<EGOpDef>& EffectGraphOps() {
    static const std::vector<EGOpDef> ops = {
        // Sources
        {"input", "Image d'entrée", "Entrées", {}, {""}, 4},
        {"sample", "Échantillonner l'entrée (UV)", "Entrées", {{"UV", -1}}, {""}, 4},
        {"uv", "Coordonnées UV", "Entrées", {}, {""}, 2},
        {"pixel", "Position du pixel", "Entrées", {}, {""}, 2},
        {"size", "Taille de l'image", "Entrées", {}, {""}, 2},
        {"const", "Constante", "Constantes", {}, {""}, 1, true},
        {"const2", "Constante 2D", "Constantes", {}, {""}, 2, true},
        {"color", "Couleur", "Constantes", {}, {""}, 4, true},
        {"param", "Paramètre (réel)", "Paramètres", {}, {""}, 1, true},
        {"paramcolor", "Paramètre (couleur)", "Paramètres", {}, {""}, 4, true},
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
        // Custom
        {"custom", "HLSL personnalisé", "Personnalisé", {{"A", 0}, {"B", 0}, {"C", 0}, {"D", 0}}, {""}, 4, true},
        // Output
        {"output", "Sortie", "Sortie", {{"Couleur", 0}}, {}, 0},
    };
    return ops;
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
        if (const EGOpDef* d = FindOp(n.op)) n.inputs.resize(d->inputs.size());
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

std::string Type(int n) { return n == 1 ? "float" : "float" + std::to_string(n); }

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
    "    float4 F4(float4 v) { return v; }\n"
    "};\n";

struct Gen {
    const EffectGraph& g;
    bool unreal;
    std::vector<std::string> lines;
    std::vector<std::string> customStructs;
    std::unordered_map<Id, std::vector<Val>> memo;
    std::set<Id> visiting;
    std::set<int> usedInputs;
    std::string error;
    std::vector<Id> errorNodes;
    std::unordered_map<Id, std::string> paramNames;

    std::string UV() const { return unreal ? "UV" : "ctx.uv"; }
    std::string Pixel() const { return unreal ? "(UV * Size)" : "float2(ctx.px)"; }
    std::string Size() const { return unreal ? "Size" : "ctx.size"; }
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
        return {F(def), 1};
    }

    std::vector<Val> Eval(Id id) {
        auto it = memo.find(id);
        if (it != memo.end()) return it->second;
        const EGNode* n = g.Find(id);
        if (!n) return {{"0.0", 1}};
        const EGOpDef* d = FindOp(n->op);
        if (!d) { Fail(id, "opération inconnue : " + n->op); return {{"0.0", 1}}; }
        if (!visiting.insert(id).second) { Fail(id, "cycle dans le graphe"); return {{"0.0", 1}}; }
        std::vector<Val> a;
        for (size_t i = 0; i < d->inputs.size(); ++i) a.push_back(Arg(*n, i, d->inputs[i].defValue));
        visiting.erase(id);

        int widest = 1;
        for (auto& v : a) widest = std::max(widest, v.n);
        auto W = [&](size_t i) { return Conv(a[i], widest); };
        auto S = [&](size_t i) { return Conv(a[i], 1); };
        std::vector<Val> out;
        const std::string& op = n->op;
        auto one = [&](std::string e, int k) { out.push_back({std::move(e), k}); };
        std::string idx = std::to_string(std::clamp(n->index, 0, 3));
        if (op == "input") {
            usedInputs.insert(std::clamp(n->index, 0, 3));
            one(unreal ? SampleAt(std::clamp(n->index, 0, 3), "UV") : "Input(" + idx + ", ctx.px)", 4);
        } else if (op == "sample") {
            usedInputs.insert(std::clamp(n->index, 0, 3));
            one(SampleAt(std::clamp(n->index, 0, 3), Conv(a[0], 2)), 4);
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
            one(Conv(a[0], 4), 4);
        } else {
            Fail(id, "opération non gérée : " + op);
            one("0.0", 1);
        }
        // Materialize every output into a local so shared subexpressions are computed once.
        if (op != "output" && op != "param" && op != "paramcolor" && op != "uv" && op != "const") {
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
        if (n.op != "param" && n.op != "paramcolor") continue;
        std::string base = SanitizeId(n.name, ++k), id = base;
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
    std::vector<Val> res = gen.Eval(out);
    r.error = gen.error;
    r.errorNodes = gen.errorNodes;

    std::string s;
    s += "//@name " + g.name + "\n";
    s += "//@category " + g.category + "\n";
    if (g.inputs == 0) s += "//@generator\n";
    for (int i = 0; i < g.inputs; ++i) s += "//@input " + std::string(i == 0 ? "Image" : "Image " + std::to_string(i + 1)) + "\n";
    if (g.premultiplied) s += "//@premultiplied\n";
    {
        bool global = false;
        for (auto& n : g.nodes) if (n.op == "sample" || n.op == "custom") global = true;
        s += global ? "//@halo full\n" : "//@halo 0\n";
    }
    for (auto& n : g.nodes) {
        auto it = gen.paramNames.find(n.id);
        if (it == gen.paramNames.end()) continue;
        std::string label = n.name.empty() ? it->second : n.name;
        if (n.op == "param")
            s += "//@param " + it->second + " float " + F(n.value.x) + " " + F(n.min) + " " + F(n.max) + " \"" + label + "\"\n";
        else
            s += "//@param " + it->second + " color " + F(n.value.x) + " " + F(n.value.y) + " " + F(n.value.z) + " " + F(n.value.w) + " \"" + label + "\"\n";
    }
    s += "// Généré par l'éditeur d'effets de Shaderixel.\n";
    s += kHelperStruct;
    for (auto& c : gen.customStructs) s += c;
    s += "float4 Effect(EffectContext ctx)\n{\n    SXH sxh;\n";
    for (auto& l : gen.lines) s += l + "\n";
    s += "    return " + (res.empty() ? std::string("float4(0, 0, 0, 0)") : Conv(res[0], 4)) + ";\n}\n";
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
        std::string label = n.name.empty() ? it->second : n.name;
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
