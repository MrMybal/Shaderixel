// Typed pins: values, node signatures, value operations and execution actions (see GraphTypes.h).
#include "doc/GraphTypes.h"
#include "core/Json.h"

#include <cmath>

namespace sx {

const char* PinTypeLabel(PinType t) {
    switch (t) {
        case PinType::RGBA: return "Image";
        case PinType::Channel: return "Canal";
        case PinType::Float: return "Float";
        case PinType::Int: return "Int";
        case PinType::Bool: return "Bool";
        case PinType::Vec2: return "Vec2";
        case PinType::Color: return "Couleur";
        case PinType::String: return "Texte";
        case PinType::Name: return "Nom";
        case PinType::Exec: return "Exec";
    }
    return "?";
}

Value MakeValue(PinType t, bool array) {
    Value v;
    v.type = t;
    v.array = array;
    if (t == PinType::Color && !array) v.f = {1, 1, 1, 1};
    return v;
}

namespace {
float4 NumTo(const float4& f, PinType from, PinType to) {
    if (to == PinType::Int) return {std::round(f.x), 0, 0, 0};
    if (to == PinType::Bool) return {(from == PinType::Color ? f.w : f.x) != 0.0f ? 1.0f : 0.0f, 0, 0, 0};
    if (to == PinType::Float) return {from == PinType::Color ? (f.x + f.y + f.z) / 3.0f : f.x, 0, 0, 0};
    if (to == PinType::Vec2) return (from == PinType::Float || from == PinType::Int || from == PinType::Bool) ? float4{f.x, f.x, 0, 0} : float4{f.x, f.y, 0, 0};
    if (to == PinType::Color) {
        if (from == PinType::Float || from == PinType::Int || from == PinType::Bool) return {f.x, f.x, f.x, 1};
        if (from == PinType::Vec2) return {f.x, f.y, 0, 1};
    }
    return f;
}
std::string NumText(float x) {
    char b[32];
    snprintf(b, sizeof(b), "%g", x);
    return b;
}
}  // namespace

Value ConvertValue(const Value& v, PinType to, bool array) {
    if (v.type == to && v.array == array) return v;
    Value r = MakeValue(to, array);
    if (!PinIsValue(to) || !PinIsValue(v.type)) return r;
    auto one = [&](const float4& f, const std::string& s, float4& of, std::string& os) {
        if (PinIsText(to)) os = PinIsText(v.type) ? s : NumText(f.x);
        else of = PinIsText(v.type) ? float4{(float)atof(s.c_str()), 0, 0, 0} : NumTo(f, v.type, to);
    };
    if (array) {
        if (v.array) {
            for (size_t i = 0; i < v.Size(); ++i) {
                float4 of;
                std::string os;
                one(i < v.fa.size() ? v.fa[i] : float4{}, i < v.sa.size() ? v.sa[i] : "", of, os);
                if (PinIsText(to)) r.sa.push_back(os);
                else r.fa.push_back(of);
            }
        } else {
            float4 of;
            std::string os;
            one(v.f, v.s, of, os);
            if (PinIsText(to)) r.sa.push_back(os);
            else r.fa.push_back(of);
        }
    } else {
        if (v.array) {
            if (v.Size()) one(v.fa.empty() ? float4{} : v.fa[0], v.sa.empty() ? "" : v.sa[0], r.f, r.s);
        } else {
            one(v.f, v.s, r.f, r.s);
        }
    }
    return r;
}

bool PinsCompatible(const PinDesc& out, const PinDesc& in) {
    if (PinIsImage(out.type) || PinIsImage(in.type)) return PinIsImage(out.type) && PinIsImage(in.type);
    if (out.type == PinType::Exec || in.type == PinType::Exec) return out.type == in.type;
    if (out.array != in.array && !in.array) return false;  // a single value can feed an array input (1 element)
    return true;  // numbers convert between them; text <-> numbers too (parsed / printed)
}

std::string ValueToText(const Value& v, size_t maxItems) {
    auto one = [&](const float4& f, const std::string& s) -> std::string {
        switch (v.type) {
            case PinType::Float: return NumText(f.x);
            case PinType::Int: return std::to_string((int)std::round(f.x));
            case PinType::Bool: return f.x != 0 ? "vrai" : "faux";
            case PinType::Vec2: return "(" + NumText(f.x) + ", " + NumText(f.y) + ")";
            case PinType::Color: return "(" + NumText(f.x) + ", " + NumText(f.y) + ", " + NumText(f.z) + ", " + NumText(f.w) + ")";
            case PinType::String: return "\"" + s + "\"";
            case PinType::Name: return s;
            default: return "";
        }
    };
    if (!v.array) return one(v.f, v.s);
    std::string r = "[";
    for (size_t i = 0; i < v.Size() && i < maxItems; ++i) {
        if (i) r += ", ";
        r += one(i < v.fa.size() ? v.fa[i] : float4{}, i < v.sa.size() ? v.sa[i] : "");
    }
    if (v.Size() > maxItems) r += ", ...";
    return r + "] (" + std::to_string(v.Size()) + ")";
}

// ------------------------------------------------------------------ value operations

namespace {
PinDesc P(const char* name, PinType t, bool array = false) { return {name, t, array}; }
Value F(float x) { Value v = MakeValue(PinType::Float); v.f = {x, 0, 0, 0}; return v; }
Value I(int x) { Value v = MakeValue(PinType::Int); v.f = {(float)x, 0, 0, 0}; return v; }
Value B(bool x) { Value v = MakeValue(PinType::Bool); v.f = {x ? 1.0f : 0.0f, 0, 0, 0}; return v; }
Value S(const std::string& s) { Value v = MakeValue(PinType::String); v.s = s; return v; }
Value N(const std::string& s) { Value v = MakeValue(PinType::Name); v.s = s; return v; }
Value C(float4 c) { Value v = MakeValue(PinType::Color); v.f = c; return v; }
Value V2(float x, float y) { Value v = MakeValue(PinType::Vec2); v.f = {x, y, 0, 0}; return v; }
using Vals = std::vector<Value>;

MathOpDef Unary(const char* id, const char* label, const char* kw, float (*f)(float)) {
    return {id, label, "Maths", kw, {P("X", PinType::Float)}, {F(0)}, {P("Résultat", PinType::Float)}, [f](const Vals& a) { return Vals{F(f(a[0].f.x))}; }};
}
MathOpDef Binary(const char* id, const char* label, const char* kw, float da, float db, float (*f)(float, float)) {
    return {id, label, "Maths", kw, {P("A", PinType::Float), P("B", PinType::Float)}, {F(da), F(db)}, {P("Résultat", PinType::Float)},
            [f](const Vals& a) { return Vals{F(f(a[0].f.x, a[1].f.x))}; }};
}
MathOpDef Compare(const char* id, const char* label, const char* kw, bool (*f)(float, float)) {
    return {id, label, "Logique", kw, {P("A", PinType::Float), P("B", PinType::Float)}, {F(0), F(0)}, {P("Résultat", PinType::Bool)},
            [f](const Vals& a) { return Vals{B(f(a[0].f.x, a[1].f.x))}; }};
}
u32 Hash32(u32 x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float4 HsvToRgb(float h, float s, float v) {
    h = h - std::floor(h);
    float r = std::clamp(std::abs(h * 6 - 3) - 1, 0.0f, 1.0f), g = std::clamp(2 - std::abs(h * 6 - 2), 0.0f, 1.0f), b = std::clamp(2 - std::abs(h * 6 - 4), 0.0f, 1.0f);
    return {((r - 1) * s + 1) * v, ((g - 1) * s + 1) * v, ((b - 1) * s + 1) * v, 1};
}
}  // namespace

const std::vector<MathOpDef>& MathOps() {
    static const std::vector<MathOpDef> ops = [] {
        std::vector<MathOpDef> o;
        o.push_back(Binary("add", "Additionner", "add plus +", 0, 0, [](float a, float b) { return a + b; }));
        o.push_back(Binary("sub", "Soustraire", "subtract minus -", 0, 0, [](float a, float b) { return a - b; }));
        o.push_back(Binary("mul", "Multiplier", "multiply times *", 1, 1, [](float a, float b) { return a * b; }));
        o.push_back(Binary("div", "Diviser", "divide /", 1, 1, [](float a, float b) { return b != 0 ? a / b : 0.0f; }));
        o.push_back(Binary("min", "Minimum", "min", 0, 0, [](float a, float b) { return std::min(a, b); }));
        o.push_back(Binary("max", "Maximum", "max", 0, 0, [](float a, float b) { return std::max(a, b); }));
        o.push_back(Binary("pow", "Puissance", "power pow exp", 1, 2, [](float a, float b) { return std::pow(a, b); }));
        o.push_back(Binary("mod", "Modulo", "mod fmod reste", 0, 1, [](float a, float b) { return b != 0 ? std::fmod(a, b) : 0.0f; }));
        o.push_back(Unary("abs", "Valeur absolue", "abs", [](float x) { return std::abs(x); }));
        o.push_back(Unary("neg", "Opposé", "negate -", [](float x) { return -x; }));
        o.push_back(Unary("floor", "Arrondi inférieur", "floor", [](float x) { return std::floor(x); }));
        o.push_back(Unary("ceil", "Arrondi supérieur", "ceil", [](float x) { return std::ceil(x); }));
        o.push_back(Unary("round", "Arrondi", "round", [](float x) { return std::round(x); }));
        o.push_back(Unary("sqrt", "Racine carrée", "sqrt", [](float x) { return std::sqrt(std::max(x, 0.0f)); }));
        o.push_back(Unary("sin", "Sinus (degrés)", "sin", [](float x) { return std::sin(x * 0.01745329f); }));
        o.push_back(Unary("cos", "Cosinus (degrés)", "cos", [](float x) { return std::cos(x * 0.01745329f); }));
        o.push_back({"clamp", "Limiter", "Maths", "clamp borner", {P("X", PinType::Float), P("Min", PinType::Float), P("Max", PinType::Float)}, {F(0), F(0), F(1)},
                     {P("Résultat", PinType::Float)}, [](const Vals& a) { return Vals{F(std::clamp(a[0].f.x, std::min(a[1].f.x, a[2].f.x), std::max(a[1].f.x, a[2].f.x)))}; }});
        o.push_back({"lerp", "Interpoler", "Maths", "lerp mix", {P("A", PinType::Float), P("B", PinType::Float), P("T", PinType::Float)}, {F(0), F(1), F(0.5f)},
                     {P("Résultat", PinType::Float)}, [](const Vals& a) { return Vals{F(a[0].f.x + (a[1].f.x - a[0].f.x) * a[2].f.x)}; }});
        o.push_back({"remap", "Remapper", "Maths", "remap range plage",
                     {P("X", PinType::Float), P("De min", PinType::Float), P("De max", PinType::Float), P("Vers min", PinType::Float), P("Vers max", PinType::Float)},
                     {F(0), F(0), F(1), F(0), F(100)}, {P("Résultat", PinType::Float)}, [](const Vals& a) {
                         float d = a[2].f.x - a[1].f.x;
                         float t = d != 0 ? (a[0].f.x - a[1].f.x) / d : 0.0f;
                         return Vals{F(a[3].f.x + (a[4].f.x - a[3].f.x) * t)};
                     }});
        o.push_back({"random", "Aléatoire (graine)", "Maths", "random rand seed alea", {P("Graine", PinType::Int), P("Min", PinType::Float), P("Max", PinType::Float)},
                     {I(1), F(0), F(1)}, {P("Résultat", PinType::Float)}, [](const Vals& a) {
                         float t = (Hash32((u32)(int)a[0].f.x * 2654435761u + 12345u) & 0xFFFFFF) / 16777216.0f;
                         return Vals{F(a[1].f.x + (a[2].f.x - a[1].f.x) * t)};
                     }});
        // Logic
        o.push_back(Compare("greater", "Plus grand (A > B)", "greater >", [](float a, float b) { return a > b; }));
        o.push_back(Compare("less", "Plus petit (A < B)", "less <", [](float a, float b) { return a < b; }));
        o.push_back(Compare("equal", "Égal (A = B)", "equal ==", [](float a, float b) { return std::abs(a - b) < 1e-6f; }));
        o.push_back({"and", "Et", "Logique", "and &&", {P("A", PinType::Bool), P("B", PinType::Bool)}, {B(true), B(true)}, {P("Résultat", PinType::Bool)},
                     [](const Vals& a) { return Vals{B(a[0].f.x != 0 && a[1].f.x != 0)}; }});
        o.push_back({"or", "Ou", "Logique", "or ||", {P("A", PinType::Bool), P("B", PinType::Bool)}, {B(false), B(false)}, {P("Résultat", PinType::Bool)},
                     [](const Vals& a) { return Vals{B(a[0].f.x != 0 || a[1].f.x != 0)}; }});
        o.push_back({"not", "Non", "Logique", "not !", {P("A", PinType::Bool)}, {B(false)}, {P("Résultat", PinType::Bool)}, [](const Vals& a) { return Vals{B(a[0].f.x == 0)}; }});
        o.push_back({"select", "Choisir (si)", "Logique", "select if ternary choose",
                     {P("Condition", PinType::Bool), P("Si vrai", PinType::Float), P("Si faux", PinType::Float)}, {B(true), F(1), F(0)}, {P("Résultat", PinType::Float)},
                     [](const Vals& a) { return Vals{a[0].f.x != 0 ? a[1] : a[2]}; }});
        // Vectors / colors
        o.push_back({"vec2", "Vec2 (X, Y)", "Vecteurs", "make vector2 vec2", {P("X", PinType::Float), P("Y", PinType::Float)}, {F(0), F(0)}, {P("Vec2", PinType::Vec2)},
                     [](const Vals& a) { return Vals{V2(a[0].f.x, a[1].f.x)}; }});
        o.push_back({"break_vec2", "Séparer Vec2", "Vecteurs", "break split vector2", {P("Vec2", PinType::Vec2)}, {V2(0, 0)}, {P("X", PinType::Float), P("Y", PinType::Float)},
                     [](const Vals& a) { return Vals{F(a[0].f.x), F(a[0].f.y)}; }});
        o.push_back({"color", "Couleur (R, V, B, A)", "Vecteurs", "make color rgba couleur",
                     {P("R", PinType::Float), P("V", PinType::Float), P("B", PinType::Float), P("A", PinType::Float)}, {F(1), F(1), F(1), F(1)}, {P("Couleur", PinType::Color)},
                     [](const Vals& a) { return Vals{C({a[0].f.x, a[1].f.x, a[2].f.x, a[3].f.x})}; }});
        o.push_back({"break_color", "Séparer couleur", "Vecteurs", "break split color rgba", {P("Couleur", PinType::Color)}, {C({1, 1, 1, 1})},
                     {P("R", PinType::Float), P("V", PinType::Float), P("B", PinType::Float), P("A", PinType::Float)},
                     [](const Vals& a) { return Vals{F(a[0].f.x), F(a[0].f.y), F(a[0].f.z), F(a[0].f.w)}; }});
        o.push_back({"hsv", "Couleur TSV (teinte 0-1)", "Vecteurs", "hsv hsb teinte hue", {P("Teinte", PinType::Float), P("Saturation", PinType::Float), P("Valeur", PinType::Float)},
                     {F(0), F(1), F(1)}, {P("Couleur", PinType::Color)}, [](const Vals& a) {
                         float4 c = HsvToRgb(a[0].f.x, a[1].f.x, a[2].f.x);
                         return Vals{C({c.x, c.y, c.z, 1})};
                     }});
        o.push_back({"mix_color", "Mélanger couleurs", "Vecteurs", "mix lerp color", {P("A", PinType::Color), P("B", PinType::Color), P("T", PinType::Float)},
                     {C({0, 0, 0, 1}), C({1, 1, 1, 1}), F(0.5f)}, {P("Couleur", PinType::Color)}, [](const Vals& a) {
                         float t = a[2].f.x;
                         float4 x = a[0].f, y = a[1].f;
                         return Vals{C({x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t})};
                     }});
        // Text
        o.push_back({"concat", "Concaténer", "Texte", "concat append join string", {P("A", PinType::String), P("B", PinType::String)}, {S(""), S("")},
                     {P("Texte", PinType::String)}, [](const Vals& a) { return Vals{S(a[0].s + a[1].s)}; }});
        o.push_back({"to_string", "Nombre en texte", "Texte", "to string format number", {P("X", PinType::Float)}, {F(0)}, {P("Texte", PinType::String)},
                     [](const Vals& a) { return Vals{S(NumText(a[0].f.x))}; }});
        o.push_back({"to_name", "Texte en nom", "Texte", "name fname", {P("Texte", PinType::String)}, {S("")}, {P("Nom", PinType::Name)}, [](const Vals& a) { return Vals{N(a[0].s)}; }});
        o.push_back({"text_length", "Longueur du texte", "Texte", "length len", {P("Texte", PinType::String)}, {S("")}, {P("Longueur", PinType::Int)},
                     [](const Vals& a) { return Vals{I((int)a[0].s.size())}; }});
        o.push_back({"to_int", "En entier", "Conversions", "int truncate", {P("X", PinType::Float)}, {F(0)}, {P("Int", PinType::Int)}, [](const Vals& a) { return Vals{I((int)a[0].f.x)}; }});
        o.push_back({"to_float", "En float", "Conversions", "float", {P("X", PinType::Int)}, {I(0)}, {P("Float", PinType::Float)}, [](const Vals& a) { return Vals{F(a[0].f.x)}; }});
        // Arrays
        o.push_back({"array_length", "Taille du tableau", "Tableaux", "array length count size", {P("Tableau", PinType::Float, true)}, {MakeValue(PinType::Float, true)},
                     {P("Taille", PinType::Int)}, [](const Vals& a) { return Vals{I((int)a[0].Size())}; }});
        o.push_back({"array_get", "Élément du tableau", "Tableaux", "array get index element", {P("Tableau", PinType::Float, true), P("Index", PinType::Int)},
                     {MakeValue(PinType::Float, true), I(0)}, {P("Élément", PinType::Float)}, [](const Vals& a) {
                         int i = (int)a[1].f.x;
                         return Vals{F(i >= 0 && i < (int)a[0].fa.size() ? a[0].fa[i].x : 0.0f)};
                     }});
        o.push_back({"array_get_color", "Couleur du tableau", "Tableaux", "array get color index", {P("Tableau", PinType::Color, true), P("Index", PinType::Int)},
                     {MakeValue(PinType::Color, true), I(0)}, {P("Couleur", PinType::Color)}, [](const Vals& a) {
                         int i = (int)a[1].f.x;
                         return Vals{C(i >= 0 && i < (int)a[0].fa.size() ? a[0].fa[i] : float4{0, 0, 0, 1})};
                     }});
        o.push_back({"array_get_text", "Texte du tableau", "Tableaux", "array get string index", {P("Tableau", PinType::String, true), P("Index", PinType::Int)},
                     {MakeValue(PinType::String, true), I(0)}, {P("Texte", PinType::String)}, [](const Vals& a) {
                         int i = (int)a[1].f.x;
                         return Vals{S(i >= 0 && i < (int)a[0].sa.size() ? a[0].sa[i] : "")};
                     }});
        o.push_back({"range", "Suite de nombres", "Tableaux", "range sequence linspace", {P("Début", PinType::Float), P("Nombre", PinType::Int), P("Pas", PinType::Float)},
                     {F(0), I(4), F(1)}, {P("Tableau", PinType::Float, true)}, [](const Vals& a) {
                         Value r = MakeValue(PinType::Float, true);
                         int n = std::clamp((int)a[1].f.x, 0, 4096);
                         for (int i = 0; i < n; ++i) r.fa.push_back({a[0].f.x + a[2].f.x * i, 0, 0, 0});
                         return Vals{r};
                     }});
        o.push_back({"array_sum", "Somme du tableau", "Tableaux", "array sum total", {P("Tableau", PinType::Float, true)}, {MakeValue(PinType::Float, true)},
                     {P("Somme", PinType::Float)}, [](const Vals& a) {
                         float s = 0;
                         for (auto& x : a[0].fa) s += x.x;
                         return Vals{F(s)};
                     }});
        return o;
    }();
    return ops;
}

const MathOpDef* FindMathOp(const std::string& id) {
    for (auto& o : MathOps()) if (o.id == id) return &o;
    return nullptr;
}

// ------------------------------------------------------------------ execution actions

const std::vector<ExecActionDef>& ExecActions() {
    static const std::vector<ExecActionDef> acts = [] {
        const PinDesc ex = P("", PinType::Exec);
        std::vector<ExecActionDef> a;
        a.push_back({"sequence", "Séquence", "sequence then puis", "Exécute ses sorties l'une après l'autre.", {ex}, {Value{}},
                     {P("Puis 1", PinType::Exec), P("Puis 2", PinType::Exec), P("Puis 3", PinType::Exec)}});
        a.push_back({"branch", "Branche (si)", "branch if condition", "Continue par Vrai ou Faux selon la condition.", {ex, P("Condition", PinType::Bool)}, {Value{}, B(true)},
                     {P("Vrai", PinType::Exec), P("Faux", PinType::Exec)}});
        a.push_back({"for_range", "Pour (de ... à)", "for loop range boucle", "Exécute Corps pour chaque index de Début à Fin (inclus).",
                     {ex, P("Début", PinType::Int), P("Fin", PinType::Int)}, {Value{}, I(0), I(3)},
                     {P("Corps", PinType::Exec), P("Index", PinType::Int), P("Terminé", PinType::Exec)}});
        a.push_back({"foreach", "Pour chaque (nombres)", "foreach for each array loop boucle tableau", "Exécute Corps pour chaque élément du tableau.",
                     {ex, P("Tableau", PinType::Float, true)}, {Value{}, MakeValue(PinType::Float, true)},
                     {P("Corps", PinType::Exec), P("Élément", PinType::Float), P("Index", PinType::Int), P("Terminé", PinType::Exec)}});
        a.push_back({"foreach_text", "Pour chaque (textes)", "foreach for each array string loop", "Exécute Corps pour chaque texte du tableau.",
                     {ex, P("Tableau", PinType::String, true)}, {Value{}, MakeValue(PinType::String, true)},
                     {P("Corps", PinType::Exec), P("Élément", PinType::String), P("Index", PinType::Int), P("Terminé", PinType::Exec)}});
        a.push_back({"bake", "Bake", "bake figer pixels store", "Garde l'image en pixels (Résultat) ; avant, l'image passe en direct.",
                     {ex, P("Image", PinType::RGBA)}, {Value{}, Value{}}, {ex, P("Résultat", PinType::RGBA)}, 1, 1});
        a.push_back({"upscale", "Upscale", "upscale agrandir esrgan", "Agrandit l'image (bicubique ou modèle IA via ComfyUI), résultat à la taille du document.",
                     {ex, P("Image", PinType::RGBA), P("Facteur", PinType::Int), P("IA (ComfyUI)", PinType::Bool), P("Modèle", PinType::Name)},
                     {Value{}, Value{}, I(2), B(false), N("4x-UltraSharp.pth")}, {ex, P("Résultat", PinType::RGBA)}, 1, 1});
        a.push_back({"comfyui", "ComfyUI", "comfyui ia ai stable diffusion img2img workflow", "Envoie l'image à ComfyUI (image vers image ou workflow API) ; le résultat revient.",
                     {ex, P("Image", PinType::RGBA), P("Prompt", PinType::String), P("Denoise", PinType::Float), P("Workflow", PinType::String)},
                     {Value{}, Value{}, S(""), F(0.5f), S("")}, {ex, P("Résultat", PinType::RGBA)}, 1, 1});
        a.push_back({"export_png", "Exporter en PNG", "export save png fichier", "Écrit l'image dans un fichier PNG.",
                     {ex, P("Image", PinType::RGBA), P("Fichier", PinType::String)}, {Value{}, Value{}, S("")}, {ex}});
        a.push_back({"call", "Appeler une fonction", "call appeler fonction event evenement run", "Lance l'événement d'un nœud de fonction du graphe (par son nom), puis continue.",
                     {ex, P("Fonction", PinType::Name), P("Événement", PinType::Name)}, {Value{}, N(""), N("")}, {ex}});
        a.push_back({"print", "Message", "print log message status", "Affiche un message dans la barre d'état.", {ex, P("Message", PinType::String)}, {Value{}, S("Bonjour")}, {ex}});
        return a;
    }();
    return acts;
}

const ExecActionDef* FindExecAction(const std::string& id) {
    for (auto& a : ExecActions()) if (a.id == id) return &a;
    return nullptr;
}

// ------------------------------------------------------------------ node signatures

void NodeSignature(const Node& n, std::vector<PinDesc>* ins, std::vector<PinDesc>* outs) {
    std::vector<PinDesc> i, o;
    switch (n.kind) {
        case NodeKind::Output: i = {P("Image", PinType::RGBA)}; break;
        case NodeKind::Surface: o = {P("RGBA", PinType::RGBA)}; break;
        case NodeKind::Blend:
            i = {P("Fond", PinType::RGBA), P("Calque", PinType::RGBA), P("Masque", PinType::Channel)};
            o = {P("RGBA", PinType::RGBA)};
            break;
        case NodeKind::Split:
            i = {P("RGBA", PinType::RGBA)};
            o = {P("R", PinType::Channel), P("V", PinType::Channel), P("B", PinType::Channel), P("A", PinType::Channel)};
            break;
        case NodeKind::Tool:
            i = {P("Entrée", PinType::RGBA)};
            o = {P("RGBA", PinType::RGBA)};
            break;
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            if (d) {
                for (auto& name : d->inputs) {
                    std::string l = name;
                    for (auto& c : l) c = (char)tolower((unsigned char)c);
                    static const char* scalar[] = {"r", "g", "v", "b", "a", "masque", "mask", "alpha", "hauteur", "height"};
                    bool ch = false;
                    for (const char* s : scalar) ch |= l == s;
                    i.push_back(P(name.c_str(), ch ? PinType::Channel : PinType::RGBA));
                }
                for (int k : n.paramPins) {
                    if (k < 0 || k >= (int)d->params.size()) continue;
                    const ParamDef& pd = d->params[k];
                    PinType t = PinType::Float;
                    switch (pd.type) {
                        case ParamType::Int: case ParamType::Enum: t = PinType::Int; break;
                        case ParamType::Bool: t = PinType::Bool; break;
                        case ParamType::Color: case ParamType::Float3: case ParamType::Float4: t = PinType::Color; break;
                        case ParamType::Float2: t = PinType::Vec2; break;
                        default: break;
                    }
                    i.push_back({pd.label, t, false});
                }
            } else {
                for (size_t k = 0; k < n.inputs.size(); ++k) i.push_back(P("Entrée", PinType::RGBA));
            }
            o = {P("RGBA", PinType::RGBA)};
            break;
        }
        case NodeKind::Function:
            if (n.sub) {
                for (const Node* p : FunctionPorts(*n.sub, NodeKind::SubInput)) i.push_back({p->name, p->value.type, p->value.array});
                for (const Node* p : FunctionPorts(*n.sub, NodeKind::SubOutput)) o.push_back({p->name, p->value.type, p->value.array});
            }
            break;
        case NodeKind::SubInput: o = {{n.name, n.value.type, n.value.array}}; break;
        case NodeKind::SubOutput: i = {{PinIsImage(n.value.type) ? std::string("Image") : std::string("Valeur"), n.value.type, n.value.array}}; break;
        case NodeKind::Value: o = {{"Valeur", n.value.type, n.value.array}}; break;
        case NodeKind::VarGet: o = {{n.op, n.value.type, n.value.array}}; break;
        case NodeKind::VarSet:
            i = {P("", PinType::Exec), {"Valeur", n.value.type, n.value.array}};
            o = {P("", PinType::Exec), {"Valeur", n.value.type, n.value.array}};
            break;
        case NodeKind::Event: o = {P("Exécuter", PinType::Exec)}; break;
        case NodeKind::Math:
            if (auto* d = FindMathOp(n.op)) { i = d->ins; o = d->outs; }
            break;
        case NodeKind::Action:
            if (auto* d = FindExecAction(n.op)) { i = d->ins; o = d->outs; }
            break;
    }
    if (ins) *ins = std::move(i);
    if (outs) *outs = std::move(o);
}

PinDesc NodeInputPin(const Node& n, int i) {
    std::vector<PinDesc> ins;
    NodeSignature(n, &ins, nullptr);
    return i >= 0 && i < (int)ins.size() ? ins[i] : PinDesc{};
}
PinDesc NodeOutputPin(const Node& n, int k) {
    std::vector<PinDesc> outs;
    NodeSignature(n, nullptr, &outs);
    return k >= 0 && k < (int)outs.size() ? outs[k] : PinDesc{};
}
int NodeInputCount(const Node& n) {
    std::vector<PinDesc> ins;
    NodeSignature(n, &ins, nullptr);
    return (int)ins.size();
}
PinType NodeInputType(const Node& n, int i) { return NodeInputPin(n, i).type; }
int NodeOutputCount(const Node& n) {
    std::vector<PinDesc> outs;
    NodeSignature(n, nullptr, &outs);
    return (int)outs.size();
}
PinType NodeOutputType(const Node& n, int k) { return NodeOutputPin(n, k).type; }
u8 NodeOutputChannel(const Node& n, int k) {
    if (n.kind == NodeKind::Split) return (u8)(k + 1);
    return (u8)(std::clamp(k, 0, 31) << 3);
}
int NodeOutputPortOf(const Node& src, u8 channel) {
    if (src.kind == NodeKind::Split) return channel ? channel - 1 : 0;
    return LinkPort(channel);
}
std::string NodeOutputName(const Node& n, int k) { return NodeOutputPin(n, k).name; }
std::string NodeInputName(const Node& n, int i) { return NodeInputPin(n, i).name; }

Node& AddTypedNode(Document& d, NodeKind kind, const std::string& op, PinType type, bool array, float2 pos) {
    Graph& g = d.S().graph;
    if ((kind == NodeKind::VarGet || kind == NodeKind::VarSet) && !g.Var(op)) {
        // Unknown variable: created (Float), or the first one of the graph.
        if (op.empty() && !g.variables.empty()) return AddTypedNode(d, kind, g.variables[0].name, type, array, pos);
        GraphVariable v;
        v.name = op.empty() ? "Variable" : op;
        v.value = MakeValue(PinIsValue(type) ? type : PinType::Float, array);
        g.variables.push_back(v);
        return AddTypedNode(d, kind, v.name, type, array, pos);
    }
    Node& n = d.AddNode(kind, pos);
    n.op = op;
    switch (kind) {
        case NodeKind::Value:
            n.value = MakeValue(type, array);
            n.name = std::string(array ? "Tableau " : "") + PinTypeLabel(type);
            break;
        case NodeKind::Math:
            if (auto* m = FindMathOp(op)) n.name = m->label;
            break;
        case NodeKind::Action:
            if (auto* a = FindExecAction(op)) n.name = a->label;
            break;
        case NodeKind::Event:
            n.name = op.empty() ? "Exécuter" : op;
            break;
        case NodeKind::VarGet:
        case NodeKind::VarSet: {
            const GraphVariable* v = d.S().graph.Var(op);
            n.value = v ? MakeValue(v->value.type, v->value.array) : MakeValue(PinType::Float);
            n.name = (kind == NodeKind::VarGet ? "Lire " : "Définir ") + op;
            break;
        }
        default: break;
    }
    n.inputs.assign(NodeInputCount(n), 0);
    return n;
}

namespace {
// Effect parameter behind input pin i (-1: an image input or no parameter).
int EffectParamOfPin(const Node& n, int i) {
    if (n.kind != NodeKind::Effect) return -1;
    auto def = n.fx.Def();
    if (!def) return -1;
    const int j = i - (int)def->inputs.size();
    if (j < 0 || j >= (int)n.paramPins.size()) return -1;
    const int k = n.paramPins[j];
    return k >= 0 && k < (int)def->params.size() ? k : -1;
}
}  // namespace

void NodeSetPinValue(Node& n, int i, const Value& v) {
    const PinDesc pin = NodeInputPin(n, i);
    const Value c = ConvertValue(v, pin.type, pin.array);
    if (const int k = EffectParamOfPin(n, i); k >= 0) {
        auto def = n.fx.Def();
        while ((int)n.fx.params.size() <= k) n.fx.params.push_back(def->params[n.fx.params.size()].def);
        n.fx.params[k] = c.f;
        return;
    }
    if ((int)n.pinValues.size() <= i) n.pinValues.resize(i + 1);
    n.pinValues[i] = c;
}

Value NodePinDefault(const Node& n, int i) {
    const PinDesc pin = NodeInputPin(n, i);
    if (const int k = EffectParamOfPin(n, i); k >= 0) {
        Value v = MakeValue(pin.type, false);
        v.f = k < (int)n.fx.params.size() ? n.fx.params[k] : n.fx.Def()->params[k].def;
        return v;
    }
    if (i >= 0 && i < (int)n.pinValues.size() && PinIsValue(n.pinValues[i].type)) return ConvertValue(n.pinValues[i], pin.type, pin.array);
    if (n.kind == NodeKind::Math)
        if (auto* d = FindMathOp(n.op); d && i < (int)d->defaults.size()) return d->defaults[i];
    if (n.kind == NodeKind::Action)
        if (auto* d = FindExecAction(n.op); d && i < (int)d->defaults.size() && PinIsValue(d->defaults[i].type)) return d->defaults[i];
    if (n.kind == NodeKind::Function && n.sub) {
        auto ins = FunctionPorts(*n.sub, NodeKind::SubInput);
        if (i < (int)ins.size()) return ins[i]->value;
    }
    return MakeValue(pin.type, pin.array);
}

// ------------------------------------------------------------------ JSON

Json ValueToJson(const Value& v) {
    Json j = Json::Object();
    j["type"] = PinTypeLabel(v.type);
    if (v.array) {
        j["array"] = true;
        Json a = Json::Array();
        if (PinIsText(v.type)) for (auto& s : v.sa) a.Push(s);
        else for (auto& f : v.fa) { Json e = Json::Array(); e.Push(f.x); e.Push(f.y); e.Push(f.z); e.Push(f.w); a.Push(e); }
        j["items"] = a;
    } else if (PinIsText(v.type)) {
        j["s"] = v.s;
    } else {
        Json e = Json::Array();
        e.Push(v.f.x); e.Push(v.f.y); e.Push(v.f.z); e.Push(v.f.w);
        j["f"] = e;
    }
    return j;
}

Value ValueFromJson(const Json& j) {
    Value v;
    const std::string t = j["type"].S("Image");
    for (int k = 0; k <= (int)PinType::Exec; ++k)
        if (t == PinTypeLabel((PinType)k)) v.type = (PinType)k;
    static const std::pair<const char*, PinType> kAliases[] = {{"Color", PinType::Color}, {"String", PinType::String}, {"Text", PinType::String},
                                                                {"Name", PinType::Name}, {"FName", PinType::Name}, {"Channel", PinType::Channel},
                                                                {"float", PinType::Float}, {"int", PinType::Int}, {"bool", PinType::Bool}};
    for (auto& [name, pt] : kAliases)
        if (t == name) v.type = pt;
    v.array = j["array"].B(false);
    auto f4 = [](const Json& e) { return e.IsArray() && e.Size() >= 4 ? float4{e.At(0).F(), e.At(1).F(), e.At(2).F(), e.At(3).F()} : float4{}; };
    if (v.array) {
        for (auto& e : j["items"].Elements()) {
            if (PinIsText(v.type)) v.sa.push_back(e.S(""));
            else v.fa.push_back(f4(e));
        }
    } else if (PinIsText(v.type)) {
        v.s = j["s"].S("");
    } else if (j.Has("f")) {
        v.f = j["f"].IsArray() ? f4(j["f"]) : float4{j["f"].F(), 0, 0, 0};
        if (j["f"].IsArray() && j["f"].Size() < 4)  // short forms: [x, y] / [r, g, b]
            for (size_t k = 0; k < j["f"].Size(); ++k) (&v.f.x)[k] = j["f"].At(k).F();
    }
    return v;
}

}  // namespace sx
