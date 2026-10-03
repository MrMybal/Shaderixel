#include "doc/HlslImport.h"

#include <regex>
#include <sstream>

namespace sx {

namespace {
std::vector<std::string> Tokens(const std::string& s) {
    // Words and "quoted strings" (quotes removed).
    std::vector<std::string> t;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isspace((unsigned char)s[i])) ++i;
        if (i >= s.size()) break;
        if (s[i] == '"') {
            size_t e = s.find('"', i + 1);
            if (e == std::string::npos) e = s.size();
            t.push_back("\"" + s.substr(i + 1, e - i - 1));  // leading quote marks a string
            i = e + 1;
        } else {
            size_t e = i;
            while (e < s.size() && !isspace((unsigned char)s[e])) ++e;
            t.push_back(s.substr(i, e - i));
            i = e;
        }
    }
    return t;
}

bool IsNum(const std::string& s) {
    if (s.empty() || s[0] == '"') return false;
    char* end = nullptr;
    strtof(s.c_str(), &end);
    return end && *end == 0;
}

int TypeFromName(std::string t) {
    for (auto& c : t) c = (char)tolower((unsigned char)c);
    if (t == "float" || t == "half" || t == "float1") return kPinFloat;
    if (t == "float2" || t == "half2") return kPinFloat2;
    if (t == "float3" || t == "half3") return kPinFloat3;
    if (t == "float4" || t == "half4") return kPinFloat4;
    if (t == "int" || t == "uint") return kPinInt;
    if (t == "bool") return kPinBool;
    if (t == "color" || t == "couleur") return kPinColor;
    if (t == "texture" || t == "texture2d" || t.rfind("texture2d<", 0) == 0) return kPinTexture;
    return 0;
}

struct Annot {
    std::string name;
    int type = 0;
    std::vector<float> nums;
    std::vector<std::string> strs;
};

// Fills default / range / label / category of a pin from an //@in annotation.
void ApplyAnnot(EGPinDecl& p, const Annot& a) {
    if (a.type) p.type = a.type;
    int n = p.type == kPinFloat2 ? 2 : p.type == kPinFloat3 ? 3 : (p.type == kPinFloat4 || p.type == kPinColor) ? 4 : p.type == kPinTexture ? 0 : 1;
    for (int i = 0; i < n && i < (int)a.nums.size(); ++i) p.def[i] = a.nums[i];
    if (p.type == kPinColor && a.nums.size() == 3) p.def.w = 1;
    if ((int)a.nums.size() >= n + 2) {
        p.min = a.nums[n];
        p.max = a.nums[n + 1];
    } else if (p.type == kPinInt) {
        p.max = 10;
    }
    if (a.strs.size() >= 1) p.label = a.strs[0];
    if (a.strs.size() >= 2) p.category = a.strs[1];
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// T.Sample(S, uv) / T.SampleLevel(S, uv, lod) -> SXTex(T, uv); Texture2D / SamplerState -> uint.
std::string RewriteTextures(std::string code) {
    code = std::regex_replace(code, std::regex(R"((\w+)\s*\.\s*Sample(Level|Grad)?\s*\(\s*\w+\s*,)"), "SXTex($1,");
    code = std::regex_replace(code, std::regex(R"(\bTexture2D\s*(<[^>]*>)?\s+)"), "uint ");
    code = std::regex_replace(code, std::regex(R"(\bSamplerState\s+)"), "uint ");
    return code;
}
}  // namespace

HlslImport ParseHlslNode(const std::string& source) {
    HlslImport r;
    EGNode& n = r.node;
    n.op = "custom";
    std::vector<Annot> ins, outs;
    std::string mainName, code;
    std::istringstream in(source);
    for (std::string line; std::getline(in, line);) {
        std::string t = Trim(line);
        if (t.rfind("//@", 0) != 0) {
            code += line + "\n";
            continue;
        }
        auto tok = Tokens(t.substr(3));
        if (tok.empty()) continue;
        const std::string& key = tok[0];
        auto rest = [&] {
            std::string s;
            for (size_t i = 1; i < tok.size(); ++i) s += (s.empty() ? "" : " ") + (tok[i][0] == '"' ? tok[i].substr(1) : tok[i]);
            return s;
        };
        if (key == "name") r.name = rest();
        else if (key == "category") r.category = rest();
        else if (key == "main" && tok.size() > 1) mainName = tok[1];
        else if (key == "kind" && tok.size() > 1) r.kind = tok[1] == "blend" || tok[1] == "fusion" ? EGKind::Blend : EGKind::Effect;
        else if ((key == "in" || key == "out") && tok.size() > 1) {
            Annot a;
            a.name = tok[1];
            size_t i = 2;
            if (i < tok.size() && TypeFromName(tok[i])) a.type = TypeFromName(tok[i++]);
            for (; i < tok.size(); ++i) {
                if (IsNum(tok[i])) a.nums.push_back(strtof(tok[i].c_str(), nullptr));
                else if (tok[i] == "true") a.nums.push_back(1);
                else if (tok[i] == "false") a.nums.push_back(0);
                else a.strs.push_back(tok[i][0] == '"' ? tok[i].substr(1) : tok[i]);
            }
            (key == "in" ? ins : outs).push_back(a);
        }
    }
    auto findAnnot = [](std::vector<Annot>& v, const std::string& name) -> Annot* {
        for (auto& a : v) if (a.name == name) return &a;
        return nullptr;
    };

    // Functions of the code (return type, name, parameters).
    struct Fn { std::string ret, name, params; };
    std::vector<Fn> fns;
    {
        std::regex re(R"((\w+(?:<[^>]*>)?)\s+(\w+)\s*\(([^()]*)\)\s*\{)");
        static const char* kw[] = {"if", "for", "while", "switch", "return", "else"};
        for (std::sregex_iterator it(code.begin(), code.end(), re), end; it != end; ++it) {
            Fn f{(*it)[1], (*it)[2], (*it)[3]};
            bool keyword = false;
            for (auto* k : kw) if (f.name == k || f.ret == k) keyword = true;
            if (!keyword) fns.push_back(f);
        }
    }
    // A user function named main would clash with the kernel entry point.
    if (std::regex_search(code, std::regex(R"(\bmain\s*\()"))) {
        code = std::regex_replace(code, std::regex(R"(\bmain(\s*\())"), "SXUserMain$1");
        for (auto& f : fns) if (f.name == "main") f.name = "SXUserMain";
        if (mainName == "main") mainName = "SXUserMain";
    }
    const Fn* mainFn = nullptr;
    for (auto& f : fns) if (f.name == mainName) mainFn = &f;
    if (!mainFn && !fns.empty() && mainName.empty()) mainFn = &fns.back();
    if (!mainName.empty() && !mainFn) { r.error = "fonction //@main introuvable : " + mainName; return r; }

    if (mainFn) {
        // Inputs / outputs from the function signature; annotations add the details.
        std::vector<std::string> args;
        std::string outsInit;
        int retType = mainFn->ret == "void" ? 0 : TypeFromName(mainFn->ret);
        if (mainFn->ret != "void" && !retType) { r.error = "type de retour non géré : " + mainFn->ret; return r; }
        std::string retName = "Resultat";
        std::stringstream ps(mainFn->params);
        std::vector<std::string> outNames;
        for (std::string p; std::getline(ps, p, ',');) {
            p = Trim(p);
            if (p.empty()) continue;
            size_t colon = p.find(':');
            if (colon != std::string::npos) p = Trim(p.substr(0, colon));  // semantic
            auto tk = Tokens(p);
            bool isOut = false;
            size_t i = 0;
            while (i < tk.size() && (tk[i] == "in" || tk[i] == "out" || tk[i] == "inout" || tk[i] == "uniform" || tk[i] == "const")) {
                if (tk[i] == "out" || tk[i] == "inout") isOut = true;
                ++i;
            }
            if (i + 1 >= tk.size()) { r.error = "paramètre illisible : " + p; return r; }
            std::string type = tk[i], name = tk[i + 1];
            if (type == "SamplerState") { args.push_back("0u"); continue; }
            int pt = TypeFromName(type);
            if (!pt) { r.error = "type de paramètre non géré : " + type + " " + name; return r; }
            EGPinDecl pin;
            pin.name = name;
            pin.type = pt;
            if (isOut) {
                if (pt == kPinTexture) { r.error = "une texture ne peut pas être une sortie"; return r; }
                if (Annot* a = findAnnot(outs, name)) { if (a->type && a->type != kPinTexture) pin.type = a->type; if (!a->strs.empty()) pin.category = a->strs[0]; }
                n.outs.push_back(pin);
                outNames.push_back(name);
            } else {
                if (Annot* a = findAnnot(ins, name)) ApplyAnnot(pin, *a);
                else if (pt == kPinInt) pin.max = 10;
                n.pins.push_back(pin);
            }
            args.push_back(name);
        }
        if (retType) {
            // Name of the returned value: the //@out that is not an out parameter.
            for (auto& a : outs)
                if (std::find(outNames.begin(), outNames.end(), a.name) == outNames.end()) {
                    retName = a.name;
                    break;
                }
            EGPinDecl o;
            o.name = retName;
            o.type = retType;
            if (Annot* a = findAnnot(outs, retName)) { if (a->type && a->type != kPinTexture) o.type = a->type; if (!a->strs.empty()) o.category = a->strs[0]; }
            n.outs.insert(n.outs.begin(), o);
        }
        if (n.outs.empty()) { r.error = "la fonction " + mainFn->name + " ne renvoie rien (ni retour, ni paramètre out)"; return r; }
        std::string call = mainFn->name + "(";
        for (size_t i = 0; i < args.size(); ++i) call += (i ? ", " : "") + args[i];
        call += ")";
        n.decl = RewriteTextures(code);
        if (n.outs.size() == 1) n.text = "        return " + call + ";";
        else n.text = std::string("        ") + (retType ? retName + " = " : "") + call + ";";
    } else {
        // Body only: the annotations declare the pins.
        for (auto& a : ins) {
            EGPinDecl pin;
            pin.name = a.name;
            pin.type = a.type ? a.type : kPinFloat;
            ApplyAnnot(pin, a);
            n.pins.push_back(pin);
        }
        for (auto& a : outs) {
            EGPinDecl o;
            o.name = a.name;
            o.type = a.type && a.type != kPinTexture ? a.type : kPinFloat4;
            if (!a.strs.empty()) o.category = a.strs[0];
            n.outs.push_back(o);
        }
        if (n.outs.empty()) {
            EGPinDecl o;
            o.name = "Resultat";
            o.type = kPinFloat4;
            n.outs.push_back(o);
        }
        n.text = RewriteTextures(code);
        if (Trim(n.text).empty()) { r.error = "aucun code HLSL"; return r; }
    }
    n.name = r.name;
    n.inputs.resize(n.pins.size());
    return r;
}

Id AddHlslNode(EffectGraph& g, const HlslImport& imp, float2 pos) {
    EGNode node = imp.node;
    node.id = g.nextId++;
    node.pos = pos;
    node.inputs.assign(node.pins.size(), {});
    // Sources: textures (in order), UV, canvas size.
    float y = pos.y;
    int tex = 0;
    for (size_t i = 0; i < node.pins.size(); ++i) {
        const EGPinDecl& p = node.pins[i];
        std::string lower = p.name;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        EGNode src;
        if (p.type == kPinTexture && tex < 4) {
            src.op = "texture";
            src.index = tex++;
            src.name = p.name;
        } else if (p.type == kPinFloat2 && (lower == "uv" || lower == "texcoord" || lower == "coords")) {
            src.op = "uv";
        } else if (p.type == kPinFloat4 && lower.find("canvassize") != std::string::npos) {
            src.op = "canvassize";
        } else {
            continue;  // unconnected: a parameter of the effect
        }
        src.id = g.nextId++;
        src.pos = {pos.x - 320, y};
        y += 110;
        g.nodes.push_back(src);
        node.inputs[i] = {src.id, 0};
    }
    g.nodes.push_back(node);
    return node.id;
}

bool GraphFromHlsl(const std::string& source, EffectGraph& g, std::string* error) {
    HlslImport imp = ParseHlslNode(source);
    if (!imp.error.empty()) {
        if (error) *error = imp.error;
        return false;
    }
    g = EffectGraph{};
    g.name = imp.name;
    g.category = imp.category;
    g.kind = imp.kind;
    g.nodes.clear();
    g.nextId = 1;
    int textures = 0;
    for (auto& p : imp.node.pins) textures += p.type == kPinTexture;
    g.inputs = imp.kind == EGKind::Blend ? 3 : std::min(textures, 4);
    Id nid = AddHlslNode(g, imp, {400, 40});
    // Output: RGBA from a 4-component result, else RGB (+ alpha of the first image, or opaque).
    EGNode out;
    out.id = g.nextId++;
    out.op = "output";
    out.pos = {760, 40};
    out.inputs.resize(3);
    int t = PinValueSize(imp.node.outs[0].type);
    if (t == 4) out.inputs[0] = {nid, 0};
    else {
        out.inputs[1] = {nid, 0};
        EGNode alpha;
        alpha.id = g.nextId++;
        alpha.pos = {400, 300};
        if (g.inputs > 0) {
            alpha.op = "input";
            alpha.inputs.clear();
            out.inputs[2] = {alpha.id, 5};  // A of the first image
        } else {
            alpha.op = "const";
            alpha.value = {1, 0, 0, 0};
            out.inputs[2] = {alpha.id, 0};
        }
        g.nodes.push_back(alpha);
    }
    g.nodes.push_back(out);
    EGCompileResult cr = CompileEffectGraph(g);
    if (!cr.error.empty()) {
        if (error) *error = cr.error;
        return false;
    }
    return true;
}

}  // namespace sx
