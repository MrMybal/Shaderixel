// Actions (recorded macros) and batch processing. A step is a command with JSON
// parameters, recorded from the menus / dialogs while recording, replayed on any
// document; the batch runs a chain of actions on every image of a folder (or the
// open documents) and writes the results. The editor is in ActionsEditor.cpp.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/Effects.h"
#include "io/WicLoad.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <climits>
#include <cstdarg>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sx {

namespace {

const char* kCmds[][2] = {
    {"image_size", "Taille de l'image"},
    {"fit_crop", "Recadrer au format (centré)"},
    {"canvas_size", "Taille de la zone de travail"},
    {"rotate", "Rotation / symétrie"},
    {"crop_selection", "Recadrer selon la sélection"},
    {"filter", "Filtre"},
    {"new_layer", "Nouveau calque"},
    {"duplicate_layer", "Dupliquer le calque"},
    {"merge_down", "Fusionner vers le bas"},
    {"merge_visible", "Fusionner les calques visibles"},
    {"flatten", "Aplatir l'image"},
    {"select", "Sélection"},
    {"fill", "Remplir"},
    {"erase", "Effacer"},
    {"layer_props", "Propriétés du calque"},
    {"export", "Exporter / convertir"},
    {"photo_raw", "Photo RAW (développement)"},
    {"op", "Opération (outil IA / MCP)"},
};

const char* kRotate[] = {"90° horaire", "90° antihoraire", "180°", "symétrie horizontale", "symétrie verticale"};
const char* kInterp[] = {"au plus proche", "bilinéaire", "bicubique"};

std::string DirOf(const std::string& path) {
    auto p = Utf8Path(path).parent_path();
    return Narrow(p.wstring());
}

std::string Fmt(const char* f, ...) {
    char b[512];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof(b), f, a);
    va_end(a);
    return b;
}

std::string ParamValueText(const ParamDef& d, const Json& a) {
    const float x = a.Size() > 0 ? a.At(0).F(d.def.x) : d.def.x, y = a.Size() > 1 ? a.At(1).F() : d.def.y, z = a.Size() > 2 ? a.At(2).F() : d.def.z;
    switch (d.type) {
        case ParamType::Int: return std::to_string((int)std::lround(x));
        case ParamType::Bool: return x > 0.5f ? "oui" : "non";
        case ParamType::Enum: {
            int i = (int)std::lround(x);
            return i >= 0 && i < (int)d.enumItems.size() ? d.enumItems[i] : std::to_string(i);
        }
        case ParamType::Color: return Fmt("RVB %.2f %.2f %.2f", x, y, z);
        case ParamType::Float2: return Fmt("%.2f, %.2f", x, y);
        case ParamType::Float3:
        case ParamType::Float4: return Fmt("%.2f, %.2f, %.2f", x, y, z);
        case ParamType::Curve:
        case ParamType::Lut: return "(courbe)";
        default: return Fmt("%.2f", x);
    }
}

}  // namespace

int ActionCmdCount() { return (int)(sizeof(kCmds) / sizeof(kCmds[0])); }
const char* ActionCmdId(int i) { return kCmds[i][0]; }

const char* ActionCmdLabel(const std::string& c) {
    for (auto& k : kCmds) if (c == k[0]) return k[1];
    return c.c_str();
}

Json ActionDefaultParams(const std::string& c) {
    Json p = Json::Object();
    if (c == "image_size") { p["mode"] = "fit"; p["w"] = 1920; p["h"] = 1920; p["interp"] = 2; }
    else if (c == "fit_crop") { p["w"] = 1024; p["h"] = 1024; p["square"] = false; p["interp"] = 2; }
    else if (c == "canvas_size") { p["w"] = 64; p["h"] = 64; p["relative"] = true; p["anchor"] = 4; }
    else if (c == "export") { p["format"] = "png"; p["folder"] = ""; p["suffix"] = ""; p["quality"] = 92; }
    else if (c == "select") p["what"] = "all";
    else if (c == "rotate") p["op"] = 0;
    else if (c == "filter") { p["effect"] = "black_white"; p["destructive"] = true; }
    else if (c == "layer_props") { p["opacity"] = 1.0f; }
    else if (c == "new_layer") p["name"] = "";
    else if (c == "op") { p["name"] = "shaderixel.layer_add"; p["args"] = Json::Object(); }
    else if (c == "photo_raw") { p["settings"] = Json::Object(); p["auto"] = false; p["lens"] = true; }
    return p;
}

std::string ActionStepSummary(const ActionStep& s) {
    const Json& p = s.params;
    std::string b;
    if (s.cmd == "image_size") {
        std::string m = p["mode"].S("pixels");
        if (m == "percent") b = Fmt("%.1f %%", p["pct"].F(100));
        else b = Fmt("%s%d x %d px", m == "fit" ? "max " : "", p["w"].I(), p["h"].I());
    } else if (s.cmd == "fit_crop") {
        if (p["square"].B() || p["w"].I() <= 0) b = p["size"].I() > 0 ? Fmt("carré %d px", p["size"].I()) : "carré";
        else b = Fmt("%d x %d px", p["w"].I(), p["h"].I());
    } else if (s.cmd == "canvas_size") {
        b = Fmt("%s%d x %d px", p["relative"].B() ? "+" : "", p["w"].I(), p["h"].I());
    } else if (s.cmd == "rotate") {
        b = kRotate[std::clamp(p["op"].I(), 0, 4)];
    } else if (s.cmd == "filter") {
        auto def = Effects().Find(p["effect"].S());
        b = (def ? def->name : p["effect"].S()) + (p["destructive"].B() ? " (pixels)" : "");
    } else if (s.cmd == "select") {
        b = p["what"].S() == "all" ? "tout" : p["what"].S() == "none" ? "aucune" : "intervertir";
    } else if (s.cmd == "export") {
        std::string f = p["format"].S("png");
        for (auto& ch : f) ch = (char)toupper((unsigned char)ch);
        if (f == "JPG") f += Fmt(" %d", p["quality"].I(92));
        b = f + (p["folder"].S().empty() ? "" : " -> " + p["folder"].S());
    } else if (s.cmd == "op" || s.cmd == "new_layer") {
        b = p["name"].S();
    } else if (s.cmd == "layer_props") {
        b = Fmt("opacité %.0f %%", p["opacity"].F(1) * 100);
    } else if (s.cmd == "photo_raw") {
        b = Fmt("%zu réglages", p["settings"].Items().size()) + (p["auto"].B() ? ", auto" : "") + (p["lens"].B() ? ", objectif" : "");
    }
    return b.empty() ? ActionCmdLabel(s.cmd) : std::string(ActionCmdLabel(s.cmd)) + " : " + b;
}

std::string ActionStepDescription(const ActionStep& s) {
    const Json& p = s.params;
    const std::string& c = s.cmd;
    auto interp = [&] { return std::string(" Rééchantillonnage ") + kInterp[std::clamp(p["interp"].I(2), 0, 2)] + "."; };
    if (c == "image_size") {
        std::string m = p["mode"].S("pixels");
        if (m == "percent") {
            float k = p["pct"].F(100);
            return Fmt("Redimensionne toute l'image à %.1f %% de sa taille (par exemple 4000 x 3000 devient %d x %d).", k, (int)std::lround(40 * k),
                       (int)std::lround(30 * k)) + interp();
        }
        if (m == "fit")
            return Fmt("Réduit l'image pour qu'elle tienne dans %d x %d px en gardant ses proportions. ", p["w"].I(), p["h"].I()) +
                   (p["enlarge"].B() ? "Les images plus petites sont agrandies." : "Les images déjà plus petites ne sont pas modifiées.") + interp();
        return Fmt("Redimensionne toute l'image à exactement %d x %d px ; l'image est déformée si ses proportions sont différentes.", p["w"].I(), p["h"].I()) + interp();
    }
    if (c == "fit_crop") {
        if (p["square"].B() || p["w"].I() <= 0) {
            std::string r = "Recadre l'image en carré centré dont le côté est le plus petit des deux côtés de l'image";
            if (p["size"].I() > 0) r += Fmt(", puis la met à %d x %d px", p["size"].I(), p["size"].I());
            return r + ". Les bords qui dépassent sont coupés." + interp();
        }
        return Fmt("Remplit exactement %d x %d px : l'image est mise à l'échelle pour couvrir tout ce format en gardant ses proportions, puis ce qui dépasse "
                   "est coupé de part et d'autre (centré). Rien n'est déformé.", p["w"].I(), p["h"].I()) + interp();
    }
    if (c == "canvas_size") {
        static const char* an[] = {"en haut à gauche", "en haut", "en haut à droite", "à gauche", "au centre", "à droite", "en bas à gauche", "en bas", "en bas à droite"};
        std::string r = p["relative"].B() ? Fmt("Agrandit la zone de travail de %d px en largeur et %d px en hauteur", p["w"].I(), p["h"].I())
                                          : Fmt("Met la zone de travail à %d x %d px", p["w"].I(), p["h"].I());
        return r + ", sans redimensionner les pixels. L'image reste ancrée " + an[std::clamp(p["anchor"].I(4), 0, 8)] +
               " ; les zones ajoutées sont transparentes, ce qui sort du cadre est coupé.";
    }
    if (c == "rotate") {
        int op = std::clamp(p["op"].I(), 0, 4);
        if (op <= 1) return std::string("Fait pivoter toute l'image (tous les calques) de 90° dans le sens ") + (op == 0 ? "horaire" : "antihoraire") +
                            " ; la largeur et la hauteur s'échangent.";
        if (op == 2) return "Fait pivoter toute l'image (tous les calques) de 180°.";
        return std::string("Retourne toute l'image (tous les calques) en miroir ") + (op == 3 ? "horizontal (gauche <-> droite)." : "vertical (haut <-> bas).");
    }
    if (c == "crop_selection") return "Recadre l'image sur le rectangle qui englobe la sélection active. Sans sélection, l'étape ne fait rien.";
    if (c == "filter") {
        auto def = Effects().Find(p["effect"].S());
        if (!def) return "Applique le filtre « " + p["effect"].S() + " », introuvable dans la bibliothèque : l'étape échouera.";
        std::string r = "Applique le filtre « " + def->name + " » (" + EffectFamily(*def) + ") " +
                        (p["destructive"].B() ? "directement sur les pixels du calque actif (définitif)."
                                              : "au calque actif comme filtre non destructif, modifiable ensuite dans le graphe du calque.");
        const Json& ps = p["params"];
        std::string list;
        int n = 0;
        for (size_t i = 0; i < def->params.size() && n < 8; ++i, ++n) {
            Json a = ps.IsArray() && i < ps.Size() ? ps.At(i) : Json::Array();
            if (!list.empty()) list += " ; ";
            list += def->params[i].label + " = " + ParamValueText(def->params[i], a);
        }
        if (!list.empty()) r += " Réglages : " + list + (ps.Size() ? "." : " (valeurs par défaut).");
        return r;
    }
    if (c == "new_layer") return "Ajoute un calque de pixels vide" + (p["name"].S().empty() ? std::string() : " nommé « " + p["name"].S() + " »") + " au-dessus du calque actif.";
    if (c == "duplicate_layer") return "Duplique le calque actif (la copie est placée au-dessus et devient active).";
    if (c == "merge_down") return "Fusionne le calque actif avec le calque situé juste en dessous.";
    if (c == "merge_visible") return "Fusionne tous les calques visibles en un seul ; les calques masqués sont conservés.";
    if (c == "flatten") return "Aplatit l'image : tous les calques sont fusionnés en un seul (rien ne change s'il n'y en a qu'un).";
    if (c == "select") {
        std::string w = p["what"].S();
        return w == "all" ? "Sélectionne toute l'image." : w == "none" ? "Supprime la sélection." : "Intervertit la sélection (ce qui était sélectionné ne l'est plus et inversement).";
    }
    if (c == "fill") {
        const Json& col = p["color"];
        std::string what = col.Size() >= 3 ? Fmt("la couleur RVB %.2f %.2f %.2f", col.At(0).F(), col.At(1).F(), col.At(2).F()) : "la couleur de premier plan";
        return "Remplit la sélection du calque actif (tout le calque sans sélection) avec " + what + ".";
    }
    if (c == "erase") return "Efface (rend transparente) la sélection du calque actif, ou tout le calque sans sélection.";
    if (c == "layer_props") {
        std::string r = "Règle le calque actif :";
        if (p.Has("opacity")) r += Fmt(" opacité %.0f %%", p["opacity"].F(1) * 100);
        if (p.Has("blend")) r += std::string(", mode ") + BlendModeName((BlendMode)std::clamp(p["blend"].I(), 0, (int)BlendMode::Count - 1));
        if (p.Has("visible")) r += p["visible"].B(true) ? ", visible" : ", masqué";
        return r + ".";
    }
    if (c == "export") {
        std::string f = p["format"].S("png");
        std::string F = f;
        for (auto& ch : F) ch = (char)toupper((unsigned char)ch);
        std::string what = f == "sxl" ? "Enregistre une copie du document (calques compris) en SXL"
                         : f == "psd" ? "Exporte l'image en PSD (calques compris)"
                                      : "Exporte l'image complète (tous les calques visibles, aplatie) en " + F;
        if (f == "jpg") what += Fmt(" qualité %d (la transparence devient noire)", p["quality"].I(92));
        std::string where = p["folder"].S().empty() ? "dans le dossier de l'image d'origine" : "dans « " + p["folder"].S() + " »";
        return what + ", " + where + ", sous le nom <nom de l'image>" + p["suffix"].S() + "." + f +
               ". Le document ouvert n'est pas modifié. Dans un traitement par lots, le dossier et le nom viennent des réglages du lot.";
    }
    if (c == "photo_raw") {
        std::string r = "Développe la photo du calque actif avec le filtre Photo RAW : " + Fmt("%zu réglages", p["settings"].Items().size()) +
                        " repris (les autres à leur valeur par défaut)";
        if (p["auto"].B()) r += ", puis balance des blancs et tons automatiques calculés sur chaque image";
        if (p["lens"].B()) r += ", et correction d'objectif lensfun d'après les métadonnées de la photo (si l'objectif est dans la base)";
        return r + ".";
    }
    if (c == "op") return "Exécute l'opération « " + p["name"].S() + " » (pont IA / MCP) avec les arguments " + p["args"].Dump(0) + ".";
    return "Commande inconnue : " + c + " (l'étape échouera).";
}

Json ActionToJson(const Action& a) {
    Json o = Json::Object();
    o["name"] = a.name;
    Json st = Json::Array();
    for (auto& s : a.steps) {
        Json so = Json::Object();
        so["cmd"] = s.cmd;
        so["params"] = s.params.IsObject() ? s.params : Json::Object();
        so["enabled"] = s.enabled;
        st.Push(so);
    }
    o["steps"] = st;
    return o;
}

Action ActionFromJson(const Json& a) {
    Action act;
    act.name = a["name"].S();
    for (auto& s : a["steps"].Elements()) act.steps.push_back({s["cmd"].S(), s["params"], s["enabled"].B(true)});
    return act;
}

const std::vector<Action>& BuiltinActions() {
    static std::vector<Action> list = [] {
        std::vector<Action> L;
        auto step = [](const char* cmd, std::initializer_list<std::pair<const char*, Json>> kv) {
            Json p = Json::Object();
            for (auto& [k, v] : kv) p[k] = v;
            return ActionStep{cmd, p, true};
        };
        auto exp = [&](const char* fmt, const char* suffix = "", int q = 92) {
            return step("export", {{"format", fmt}, {"folder", ""}, {"suffix", suffix}, {"quality", q}});
        };
        // Conversions: the image written next to the original (or in the batch folder).
        L.push_back({"Convertir en PNG", {exp("png")}, "Conversions"});
        L.push_back({"Convertir en JPG (qualité 90)", {exp("jpg", "", 90)}, "Conversions"});
        L.push_back({"Convertir en JPG léger (qualité 75)", {exp("jpg", "", 75)}, "Conversions"});
        L.push_back({"Convertir en TGA", {exp("tga")}, "Conversions"});
        L.push_back({"Convertir en BMP", {exp("bmp")}, "Conversions"});
        L.push_back({"Convertir en PSD", {exp("psd")}, "Conversions"});
        L.push_back({"Convertir en SXL (document Shaderixel)", {exp("sxl")}, "Conversions"});
        // Sizes.
        auto fit = [&](int px) { return step("image_size", {{"mode", "fit"}, {"w", px}, {"h", px}, {"interp", 2}}); };
        L.push_back({"Réduire de moitié (50 %)", {step("image_size", {{"mode", "percent"}, {"pct", 50}, {"interp", 2}})}, "Taille"});
        L.push_back({"Doubler la taille (200 %)", {step("image_size", {{"mode", "percent"}, {"pct", 200}, {"interp", 2}})}, "Taille"});
        L.push_back({"Adapter à 2048 px max", {fit(2048)}, "Taille"});
        L.push_back({"Adapter à 1024 px max", {fit(1024)}, "Taille"});
        L.push_back({"Adapter à 512 px max", {fit(512)}, "Taille"});
        L.push_back({"Carré centré (côté le plus court)", {step("fit_crop", {{"square", true}, {"size", 0}, {"interp", 2}})}, "Taille"});
        L.push_back({"Carré 1024 x 1024 (centré)", {step("fit_crop", {{"square", true}, {"size", 1024}, {"interp", 2}})}, "Taille"});
        L.push_back({"Format 16:9 - 1920 x 1080 (centré)", {step("fit_crop", {{"w", 1920}, {"h", 1080}, {"interp", 2}})}, "Taille"});
        L.push_back({"Pixel art x4 (au plus proche)", {step("image_size", {{"mode", "percent"}, {"pct", 400}, {"interp", 0}})}, "Taille"});
        // Orientation.
        for (int op = 0; op < 5; ++op) {
            std::string n = op < 3 ? std::string("Rotation ") + kRotate[op] : std::string(kRotate[op]);
            n[0] = (char)toupper((unsigned char)n[0]);
            L.push_back({n, {step("rotate", {{"op", op}})}, "Orientation"});
        }
        // Color.
        L.push_back({"Noir et blanc", {step("filter", {{"effect", "black_white"}, {"destructive", true}})}, "Couleur"});
        L.push_back({"Inverser les couleurs", {step("filter", {{"effect", "invert"}, {"destructive", true}})}, "Couleur"});
        L.push_back({"Netteté légère", {step("filter", {{"effect", "sharpen"}, {"destructive", true}})}, "Couleur"});
        // Combined.
        L.push_back({"Aplatir l'image", {step("flatten", {})}, "Préparation"});
        L.push_back({"Vignette 256 px (JPG)", {fit(256), exp("jpg", "_vignette", 85)}, "Préparation"});
        L.push_back({"Web : 1920 px max + JPG", {fit(1920), exp("jpg", "_web", 88)}, "Préparation"});
        L.push_back({"Miniature carrée 512 px (PNG)", {step("fit_crop", {{"square", true}, {"size", 512}, {"interp", 2}}), exp("png", "_carre")}, "Préparation"});
        return L;
    }();
    return list;
}

std::string App::ActionsPath() const {
    return DirOf(settingsPath) + "/shaderixel_actions.json";
}

const Action* App::FindActionKey(const std::string& key) const {
    if (key.size() < 2) return nullptr;
    const std::string name = key.substr(2);
    if (key[0] == 'b') {
        for (auto& a : BuiltinActions()) if (a.name == name) return &a;
    } else {
        for (auto& a : actions) if (a.name == name) return &a;
    }
    return nullptr;
}

void App::LoadActions() {
    actions.clear();
    std::ifstream f(Utf8Path(ActionsPath()), std::ios::binary);
    Json j;
    if (f) {
        std::stringstream ss;
        ss << f.rdbuf();
        Json::Parse(ss.str(), j);
    }
    for (auto& a : j["actions"].Elements()) actions.push_back(ActionFromJson(a));
    if (!j.Has("actions")) {
        // Starter actions.
        Action web{"Taille web (1920 px) + JPG", {}};
        Json p = Json::Object();
        p["mode"] = "fit"; p["w"] = 1920; p["h"] = 1920; p["interp"] = 2;
        web.steps.push_back({"image_size", p, true});
        web.steps.push_back({"flatten", Json::Object(), true});
        Json e = Json::Object();
        e["format"] = "jpg"; e["suffix"] = "_web"; e["folder"] = ""; e["quality"] = 88;
        web.steps.push_back({"export", e, true});
        actions.push_back(web);
        Action rot{"Rotation 90° horaire", {}};
        Json r = Json::Object();
        r["op"] = 0;
        rot.steps.push_back({"rotate", r, true});
        actions.push_back(rot);
    }
}

void App::SaveActions() {
    Json j = Json::Object();
    Json arr = Json::Array();
    for (auto& a : actions) arr.Push(ActionToJson(a));
    j["actions"] = arr;
    std::ofstream f(Utf8Path(ActionsPath()), std::ios::binary);
    f << j.Dump();
}

void App::ImportActions() {
    std::string path = platform::OpenFileDialog(hwnd, L"Actions Shaderixel\0*.sxactions;*.json\0Tous les fichiers\0*.*\0");
    if (path.empty()) return;
    std::ifstream f(Utf8Path(path), std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    Json j;
    if (!Json::Parse(ss.str(), j)) { Status("Import impossible : fichier invalide"); return; }
    std::vector<Json> list;
    if (j.Has("actions")) for (auto& a : j["actions"].Elements()) list.push_back(a);
    else if (j.Has("steps")) list.push_back(j);
    int n = 0;
    for (auto& a : list) {
        Action act = ActionFromJson(a);
        if (act.name.empty()) act.name = "Action importée";
        std::string base = act.name;
        for (int k = 2; FindActionKey("u:" + act.name); ++k) act.name = base + " (" + std::to_string(k) + ")";
        actions.push_back(std::move(act));
        ++n;
    }
    SaveActions();
    Status(std::to_string(n) + " action(s) importée(s)");
}

void App::ExportActions(int index) {
    std::string path = platform::SaveFileDialog(hwnd, L"Actions Shaderixel\0*.sxactions\0", L"sxactions");
    if (path.empty()) return;
    Json j = Json::Object();
    Json arr = Json::Array();
    for (int i = 0; i < (int)actions.size(); ++i) if (index < 0 || i == index) arr.Push(ActionToJson(actions[i]));
    j["actions"] = arr;
    std::ofstream f(Utf8Path(path), std::ios::binary);
    f << j.Dump();
    Status("Actions exportées : " + path);
}

void App::FinalizeRecordedFilter() {
    // The filter parameters are those of the layer filter once tuned (at the next step / stop).
    if (recordFilterLayer == 0 || actionRecordInto < 0 || actionRecordInto >= (int)actions.size()) return;
    auto& steps = actions[actionRecordInto].steps;
    DocView* v = Active();
    if (!steps.empty() && steps.back().cmd == "filter" && v) {
        if (Layer* l = v->doc->FindLayer(recordFilterLayer)) {
            for (auto it = l->filters.rbegin(); it != l->filters.rend(); ++it)
                if (it->effectId == steps.back().params["effect"].S()) {
                    Json ps = Json::Array();
                    for (auto& q : it->params) {
                        Json a = Json::Array();
                        a.Push(q.x); a.Push(q.y); a.Push(q.z); a.Push(q.w);
                        ps.Push(a);
                    }
                    steps.back().params["params"] = ps;
                    break;
                }
        }
    }
    recordFilterLayer = 0;
}

void App::RecordAction(const std::string& cmd, Json params) {
    if (!actionRecording || actionReplaying || actionRecordInto < 0 || actionRecordInto >= (int)actions.size()) return;
    FinalizeRecordedFilter();
    if (!params.IsObject()) params = Json::Object();
    actions[actionRecordInto].steps.push_back({cmd, params, true});
    if (cmd == "filter" && !params["destructive"].B()) {
        if (DocView* v = Active())
            if (Layer* l = v->doc->ActiveLayer()) recordFilterLayer = l->id;
    }
    SaveActions();
}

void App::StopRecording() {
    if (!actionRecording) return;
    FinalizeRecordedFilter();
    actionRecording = false;
    SaveActions();
    Status("Enregistrement de l'action terminé");
}

bool App::StartActionSteps(DocView& v, const std::string& name, const std::vector<ActionStep>& steps, int first, int last, const std::string& key,
                           std::function<void(bool)> done) {
    if (actionRun.active) { Status("Une action est déjà en cours"); return false; }
    if (actionRecording) StopRecording();
    actionRun = ActionRun{};
    actionRun.active = true;
    actionRun.view = &v;
    actionRun.name = name;
    actionRun.key = key;
    first = std::max(first, 0);
    last = std::min(last, (int)steps.size() - 1);
    for (int i = first; i <= last; ++i)
        if (steps[i].enabled || first == last) {  // a single step runs even if it is disabled
            actionRun.steps.push_back(steps[i]);
            actionRun.srcIndex.push_back(i);
        }
    actionRun.done = std::move(done);
    return true;
}

bool App::StartAction(DocView& v, int index, std::function<void(bool)> done) {
    if (index < 0 || index >= (int)actions.size()) return false;
    return StartActionSteps(v, actions[index].name, actions[index].steps, 0, INT_MAX, "u:" + actions[index].name, std::move(done));
}

void App::WriteImageAs(DocView& v, const std::string& out, const std::string& fmt, int quality) {
    std::filesystem::create_directories(Utf8Path(out).parent_path());
    if (fmt == "sxl") SaveDocument(v, out, true);
    else if (fmt == "psd") ExportPsd(out, 8);
    else {
        exportQuality = std::clamp(quality, 1, 100);
        ExportImage(out);
        exportQuality = 92;
    }
}

int App::ExecActionStep(DocView& v, const ActionStep& s) {
    Document& d = *v.doc;
    const Json& p = s.params;
    const int W = (int)d.Width(), H = (int)d.Height();
    auto& R = actionRun;
    if (s.cmd == "image_size") {
        std::string mode = p["mode"].S("pixels");
        int w = p["w"].I(W), h = p["h"].I(H);
        if (mode == "percent") {
            w = std::max(1, (int)std::lround(W * p["pct"].F(100) / 100.0));
            h = std::max(1, (int)std::lround(H * p["pct"].F(100) / 100.0));
        } else if (mode == "fit") {
            double k = std::min((double)p["w"].I(W) / W, (double)p["h"].I(H) / H);
            if (k >= 1.0 && !p["enlarge"].B()) return 0;
            w = std::max(1, (int)std::lround(W * k));
            h = std::max(1, (int)std::lround(H * k));
        }
        w = std::clamp(w, 1, 16384);
        h = std::clamp(h, 1, 16384);
        if (w == W && h == H) return 0;
        float m[6] = {(float)W / w, 0, 0, 0, (float)H / h, 0};
        ApplyCanvasOp(v, (u32)w, (u32)h, m, p["interp"].I(2), "Taille de l'image");
        return 0;
    }
    if (s.cmd == "fit_crop") {
        // Scale to cover w x h, then cut what goes beyond (centered): nothing is distorted.
        int w = p["w"].I(0), h = p["h"].I(0);
        if (p["square"].B() || w <= 0 || h <= 0) w = h = p["size"].I(0) > 0 ? p["size"].I(0) : std::min(W, H);
        w = std::clamp(w, 1, 16384);
        h = std::clamp(h, 1, 16384);
        if (w == W && h == H) return 0;
        const double k = std::max((double)w / W, (double)h / H);
        const double ox = (W * k - w) * 0.5, oy = (H * k - h) * 0.5;
        float m[6] = {(float)(1 / k), 0, (float)(ox / k), 0, (float)(1 / k), (float)(oy / k)};
        ApplyCanvasOp(v, (u32)w, (u32)h, m, p["interp"].I(2), "Recadrer au format");
        return 0;
    }
    if (s.cmd == "canvas_size") {
        int w = p["w"].I(W), h = p["h"].I(H);
        if (p["relative"].B()) { w += W; h += H; }
        w = std::clamp(w, 1, 16384);
        h = std::clamp(h, 1, 16384);
        int anchor = p["anchor"].I(4), ax = anchor % 3, ay = anchor / 3;
        float ox = std::floor(ax * (w - W) * 0.5f), oy = std::floor(ay * (h - H) * 0.5f);
        float m[6] = {1, 0, -ox, 0, 1, -oy};
        ApplyCanvasOp(v, (u32)w, (u32)h, m, 0, "Taille de la zone de travail");
        return 0;
    }
    if (s.cmd == "rotate") { RotateCanvas(v, p["op"].I()); return 0; }
    if (s.cmd == "crop_selection") {
        if (!R.wait) {
            Surface* sel = d.Selection();
            if (!sel) return 0;  // nothing selected: skipped
            R.wait = v.renderer.ComputeBounds(*ctx, sel->tex.get(), nullptr, false, true);
            return 1;
        }
        if (!R.wait->ready) return 1;
        const u32* b = (const u32*)R.wait->data.data();
        if (R.wait->data.size() >= 16 && b[0] < b[2] && b[1] < b[3]) ApplyCrop(v, {(int)b[0], (int)b[1], (int)b[2], (int)b[3]});
        R.wait.reset();
        return 0;
    }
    if (s.cmd == "filter") {
        auto def = Effects().Find(p["effect"].S());
        if (!def) { R.error = "effet introuvable : " + p["effect"].S(); return 2; }
        EffectInstance fx;
        fx.effectId = def->id;
        fx.params = def->DefaultParams();
        const Json& ps = p["params"];
        for (size_t i = 0; ps.IsArray() && i < ps.Size() && i < fx.params.size(); ++i) {
            const Json& a = ps.At(i);
            if (a.Size() == 0) continue;
            fx.params[i] = {a.At(0).F(), a.Size() > 1 ? a.At(1).F() : 0, a.Size() > 2 ? a.At(2).F() : 0, a.Size() > 3 ? a.At(3).F() : 0};
        }
        if (p["destructive"].B()) {
            Id target = d.PaintTarget();
            if (!d.GetSurface(target)) { R.error = "filtre : aucun calque de pixels"; return 2; }
            if (v.renderer.ApplyEffectToSurface(d, *ctx, v.history, target, fx, def->name)) return 0;
            return ++R.retries > 600 ? (R.error = "filtre non compilé : " + def->name, 2) : 1;  // still compiling
        }
        ApplyLibraryEffect(v, def->id, FxApply::Filter);
        if (Layer* l = d.ActiveLayer(); l && !l->filters.empty() && l->filters.back().effectId == def->id) l->filters.back().params = fx.params;
        d.SyncGraph();
        return 0;
    }
    if (s.cmd == "new_layer") {
        if (d.Mode() != DocMode::Layers) return 0;
        Edit e(d, v.history, "Nouveau calque");
        std::string n = p["name"].S();
        d.AddPixelLayer(n.empty() ? "Calque " + std::to_string(d.S().layers.size() + 1) : n);
        e.Commit();
        return 0;
    }
    if (s.cmd == "duplicate_layer") {
        if (!d.ActiveLayer()) return 0;
        Edit e(d, v.history, "Dupliquer le calque");
        d.DuplicateLayer(d.S().activeLayer);
        e.Commit();
        return 0;
    }
    if (s.cmd == "merge_down") { MergeDown(v); return 0; }
    if (s.cmd == "merge_visible") { MergeVisible(v, false); return 0; }
    if (s.cmd == "flatten") {
        if (d.Mode() == DocMode::Layers && d.S().layers.size() > 1) MergeVisible(v, true);
        return 0;
    }
    if (s.cmd == "select") {
        std::string w = p["what"].S();
        if (w == "all") v.renderer.SelectShape(d, *ctx, v.history, SelShape::All, {}, SelMode::Replace, false, 0, "Tout sélectionner");
        else if (w == "none") v.renderer.Deselect(d, v.history);
        else if (w == "invert") v.renderer.SelectionFilter(d, *ctx, v.history, 3, 0, "Intervertir");
        return 0;
    }
    if (s.cmd == "fill" || s.cmd == "erase") {
        float4 keep = fg;
        const Json& c = p["color"];
        if (c.Size() >= 3) fg = {c.At(0).F(), c.At(1).F(), c.At(2).F(), c.Size() > 3 ? c.At(3).F(1) : 1.0f};
        FillOrErase(v, s.cmd == "erase");
        fg = keep;
        return 0;
    }
    if (s.cmd == "layer_props") {
        Layer* l = d.ActiveLayer();
        if (!l) return 0;
        Edit e(d, v.history, "Propriétés du calque");
        if (p.Has("opacity")) l->opacity = std::clamp(p["opacity"].F(1), 0.0f, 1.0f);
        if (p.Has("blend")) l->blend = (BlendMode)std::clamp(p["blend"].I(), 0, (int)BlendMode::Count - 1);
        if (p.Has("visible")) l->visible = p["visible"].B(true);
        d.SyncGraph();
        e.Commit();
        return 0;
    }
    if (s.cmd == "export") {
        if (!R.exporting) {
            std::string fmt = p["format"].S("png");
            std::string folder = p["folder"].S();
            if (!R.exportFolder.empty()) folder = R.exportFolder;  // batch destination wins
            // In a batch, the name comes from its pattern, with the sizes as they are now ({l} {h}).
            std::string stem = batch.running && batch.view == &v ? BatchName(batch.index, &d)
                             : !R.exportStem.empty() ? R.exportStem : d.path.empty() ? d.Name() : Narrow(Utf8Path(d.path).stem().wstring());
            if (folder.empty()) folder = d.path.empty() ? DirOf(settingsPath) : DirOf(d.path);
            std::string out = folder + "/" + stem + p["suffix"].S() + "." + fmt;
            WriteImageAs(v, out, fmt, p["quality"].I(92));
            R.lastExport = out;
            R.exporting = true;
            return 1;
        }
        if (!exports.empty() || !saves.empty() || psdExport) return 1;
        R.exporting = false;
        return 0;
    }
    if (s.cmd == "photo_raw") return PhotoRawStep(v, p, R.error, R.retries);
    if (s.cmd == "op") {
        const AiOperation* op = bridge.Find(p["name"].S());
        if (!op) { R.error = "opération inconnue : " + p["name"].S(); return 2; }
        AiResult r = op->run(p["args"], DirOf(settingsPath));
        if (!r.errorCode.empty()) { R.error = r.errorMessage; return 2; }
        return 0;
    }
    R.error = "commande inconnue : " + s.cmd;
    return 2;
}

void App::UpdateActionRun() {
    auto& R = actionRun;
    if (!R.active) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == R.view;
    auto finish = [&](bool ok) {
        auto done = std::move(R.done);
        std::string name = R.name, err = R.error;
        if (!R.lastExport.empty()) actionLastExport = R.lastExport;
        R = ActionRun{};
        if (!ok) Status("Action « " + name + " » interrompue : " + err);
        else if (!done) Status("Action « " + name + " » terminée");
        if (done) done(ok);
    };
    if (!alive) { R.error = "document fermé"; finish(false); return; }
    actionReplaying = true;
    // Steps run back to back in the frame until one waits (readback, export, compilation).
    while (R.next < R.steps.size()) {
        int r = ExecActionStep(*R.view, R.steps[R.next]);
        if (r == 1) break;
        if (r == 2) { actionReplaying = false; finish(false); return; }
        ++R.next;
        R.retries = 0;
    }
    actionReplaying = false;
    if (R.next >= R.steps.size()) finish(true);
}

// ------------------------------------------------------------------ batch

namespace {

const char* kBatchExt[] = {"", "png", "jpg", "tga", "bmp", "psd", "sxl", "", "tif"};

std::string Lower(std::string s) {
    for (auto& ch : s) ch = (char)tolower((unsigned char)ch);
    return s;
}

}  // namespace

std::vector<std::string> App::BatchScan() const {
    auto& B = batch;
    std::vector<std::string> out;
    // Extension filter: "png jpg" / "png;jpg" / ".png, .jpg"; empty = every image.
    std::vector<std::string> want;
    {
        std::string cur;
        for (char ch : B.extFilter + " ") {
            if (ch == ' ' || ch == ';' || ch == ',') {
                if (!cur.empty()) want.push_back(Lower(cur[0] == '.' ? cur : "." + cur));
                cur.clear();
            } else if (ch != '*') cur += ch;
        }
    }
    static const char* exts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".psd", ".hdr", ".exr", ".sxl", ".gif", ".tif", ".tiff", ".webp"};
    auto consider = [&](const std::filesystem::path& p) {
        std::string e = Lower(Narrow(p.extension().wstring()));
        bool image = IsRawExtension(e);
        for (auto* x : exts) image |= e == x;
        if (!image) return;
        if (!want.empty() && std::find(want.begin(), want.end(), e) == want.end() && !(e == ".jpeg" && std::find(want.begin(), want.end(), ".jpg") != want.end())) return;
        out.push_back(Narrow(p.wstring()));
    };
    std::error_code ec;
    if (B.source.empty()) return out;
    if (B.subfolders) {
        for (auto& e : std::filesystem::recursive_directory_iterator(Utf8Path(B.source), ec)) if (e.is_regular_file()) consider(e.path());
    } else {
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(B.source), ec)) if (e.is_regular_file()) consider(e.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string App::BatchName(size_t i, const Document* d) const {
    // Output name from the pattern: {nom} {n} {n3} {l} {h} {dossier} {ext} {date}.
    auto& B = batch;
    const std::string file = i < B.files.size() ? B.files[i] : "";
    auto fp = Utf8Path(file);
    std::string stem = B.sourceKind == 1 && d ? (d->path.empty() ? d->Name() : Narrow(Utf8Path(d->path).stem().wstring())) : Narrow(fp.stem().wstring());
    std::string ext = Lower(Narrow(fp.extension().wstring()));
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    char date[16] = "";
    {
        time_t t = time(nullptr);
        tm lt;
        localtime_s(&lt, &t);
        strftime(date, sizeof(date), "%Y%m%d", &lt);
    }
    char n3[16];
    snprintf(n3, sizeof(n3), "%03zu", i + 1);
    const std::pair<const char*, std::string> tok[] = {
        {"{nom}", stem}, {"{n3}", n3}, {"{n}", std::to_string(i + 1)}, {"{l}", d ? std::to_string(d->Width()) : "L"}, {"{h}", d ? std::to_string(d->Height()) : "H"},
        {"{dossier}", Narrow(fp.parent_path().filename().wstring())}, {"{ext}", ext}, {"{date}", date}};
    std::string r = B.pattern.empty() ? "{nom}" : B.pattern;
    for (auto& [k, v] : tok)
        for (size_t at; (at = r.find(k)) != std::string::npos;) r.replace(at, strlen(k), v);
    for (auto& ch : r) if (strchr("\\/:*?\"<>|", ch)) ch = '_';
    return r.empty() ? stem : r;
}

std::string App::BatchFolder(size_t i, const Document* d) const {
    auto& B = batch;
    const std::string file = i < B.files.size() ? B.files[i] : "";
    if (B.sourceKind == 1) return !B.dest.empty() ? B.dest : !d || d->path.empty() ? DirOf(settingsPath) : DirOf(d->path);
    if (B.dest.empty()) return DirOf(file);
    std::string folder = B.dest;
    if (B.keepTree && B.subfolders) {
        auto rel = Utf8Path(file).parent_path().lexically_relative(Utf8Path(B.source));
        std::string r = Narrow(rel.generic_wstring());
        if (!r.empty() && r != "." && r.rfind("..", 0) != 0) folder += "/" + r;
    }
    return folder;
}

void App::StartBatch() {
    auto& B = batch;
    B.files.clear();
    B.views.clear();
    B.errors.clear();
    B.log.clear();
    B.done = B.skipped = 0;
    if (B.sourceKind == 1) {
        for (auto& dv : docs) {
            B.views.push_back(dv.get());
            B.files.push_back(dv->doc->path.empty() ? dv->doc->Name() : dv->doc->path);
        }
    } else {
        B.files = BatchScan();
    }
    if (B.files.empty()) { Status(B.sourceKind == 1 ? "Traitement par lots : aucun document ouvert" : "Traitement par lots : aucune image dans " + B.source); return; }
    for (auto& k : B.chain)
        if (!FindActionKey(k)) { Status("Traitement par lots : action introuvable « " + k.substr(2) + " »"); return; }
    if (B.chain.empty() && B.format == 0) { Status("Traitement par lots : choisissez une action ou un format de sortie"); return; }
    B.running = true;
    B.cancel = false;
    B.index = 0;
    B.stage = 0;
    Status("Traitement par lots : " + std::to_string(B.files.size()) + " image(s)");
}

void App::UpdateBatch() {
    auto& B = batch;
    if (!B.running) return;
    auto next = [&] {
        // Close the document opened for this file (never prompts) and move on.
        if (B.opened)
            for (int i = 0; i < (int)docs.size(); ++i)
                if (docs[i].get() == B.view) { docs[i]->doc->dirty = false; CloseDocument(i, true); break; }
        B.view = nullptr;
        B.opened = false;
        ++B.index;
        B.stage = 0;
    };
    if (B.cancel) {
        if (actionRun.active) return;  // let the running step finish
        if (B.view) next();
        B.running = false;
        B.cancel = false;
        Status("Traitement par lots annulé");
        return;
    }
    if (B.index >= B.files.size()) {
        B.running = false;
        Status("Traitement par lots terminé : " + std::to_string(B.done) + "/" + std::to_string(B.files.size()) +
               (B.skipped ? ", " + std::to_string(B.skipped) + " ignorée(s)" : "") +
               (B.errors.empty() ? "" : " (" + std::to_string(B.errors.size()) + " erreur(s), voir le journal)"));
        for (auto& e : B.errors) Log("Lot : %s", e.c_str());
        return;
    }
    const std::string& file = B.files[B.index];
    auto fail = [&](const std::string& msg) {
        B.errors.push_back(file + " : " + msg);
        B.log.push_back({file, "", msg, 2});
    };
    if (B.stage == 0) {
        if (B.sourceKind == 1) {
            DocView* v = B.index < B.views.size() ? B.views[B.index] : nullptr;
            int at = -1;
            for (int i = 0; i < (int)docs.size(); ++i) if (docs[i].get() == v) at = i;
            if (at < 0) { fail("document fermé"); ++B.index; return; }
            activeDoc = at;  // exports read the active document
            B.view = v;
            B.opened = false;
        } else {
            size_t before = docs.size();
            OpenImage(file, false);
            if (docs.size() == before) { fail("ouverture impossible"); ++B.index; return; }
            B.view = Active();
            B.opened = true;
        }
        // The chain of actions, as one list of steps.
        std::vector<ActionStep> steps;
        std::string names;
        for (auto& k : B.chain)
            if (const Action* a = FindActionKey(k)) {
                for (auto& s : a->steps) if (s.enabled) steps.push_back(s);
                names += (names.empty() ? "" : " + ") + a->name;
            }
        actionLastExport.clear();
        if (steps.empty()) { B.stage = 2; return; }
        B.stage = 1;
        StartActionSteps(*B.view, names, steps, 0, INT_MAX, "", [this](bool ok) {
            if (!ok) {
                batch.errors.push_back(batch.files[batch.index] + " : " + statusMsg);
                batch.log.push_back({batch.files[batch.index], "", statusMsg, 2});
            }
            batch.stage = ok ? 2 : 4;
        });
        // The actions' own Exporter steps write into the batch folder, under the batch name.
        actionRun.exportFolder = B.dest.empty() && B.sourceKind == 0 ? "" : BatchFolder(B.index, B.view->doc.get());
        actionRun.exportStem = BatchName(B.index, B.view->doc.get());
        return;
    }
    if (B.stage == 1) return;  // action running
    if (B.stage == 2) {
        Document& d = *B.view->doc;
        if (B.format == 0) {
            B.log.push_back({file, actionLastExport, actionLastExport.empty() ? "traité (aucun fichier écrit)" : "traité", 0});
            B.stage = 3;
            return;
        }
        std::string ext = kBatchExt[std::clamp(B.format, 0, 8)];
        if (B.format == 7) {
            ext = Lower(Narrow(Utf8Path(file).extension().wstring()));
            if (!ext.empty()) ext.erase(0, 1);
            if (ext == "jpeg") ext = "jpg";
            if (ext == "tiff") ext = "tif";
            if (ext != "png" && ext != "jpg" && ext != "tga" && ext != "bmp" && ext != "psd" && ext != "sxl" && ext != "tif") ext = "png";
        }
        const std::string folder = BatchFolder(B.index, &d), name = BatchName(B.index, &d);
        std::string out = folder + "/" + name + "." + ext;
        std::error_code ec;
        auto same = [&](const std::string& a) { return std::filesystem::equivalent(Utf8Path(a), Utf8Path(file), ec); };
        if (std::filesystem::exists(Utf8Path(out), ec)) {
            if (B.conflict == 1) {
                ++B.skipped;
                B.log.push_back({file, out, "ignorée : le fichier existe déjà", 1});
                B.stage = 3;
                return;
            }
            // Numbered copy; the source file itself is never replaced.
            if (B.conflict == 2 || same(out))
                for (int k = 2; std::filesystem::exists(Utf8Path(out), ec); ++k) out = folder + "/" + name + " (" + std::to_string(k) + ")." + ext;
        }
        WriteImageAs(*B.view, out, ext, B.quality);
        B.log.push_back({file, out, "ok", 0});
        B.stage = 3;
        return;
    }
    if (B.stage == 3) {
        if (!exports.empty() || !saves.empty() || psdExport) return;
        if (!B.log.empty() && B.log.back().state == 0) ++B.done;
        next();
        return;
    }
    if (B.stage == 4) next();  // failed: skip
}

// ------------------------------------------------------------------ panel

void App::DrawActionsPanel() {
    if (!ImGui::Begin("Actions", PanelFlag("Actions"))) { ImGui::End(); return; }
    DocView* v = Active();
    actionSel = std::clamp(actionSel, 0, std::max(0, (int)actions.size() - 1));
    // Toolbar: record / stop, play, new, editor.
    bool rec = actionRecording;
    if (rec) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 50, 50, 255));
    if (ImGui::Button(rec ? "Arrêter" : "Enregistrer")) {
        if (rec) StopRecording();
        else if (!actions.empty()) {
            actionRecording = true;
            actionRecordInto = actionSel;
            Status("Enregistrement dans « " + actions[actionSel].name + " » : les commandes des menus sont ajoutées");
        }
    }
    if (rec) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enregistre les commandes : taille de l'image / de la zone de travail, rotation, recadrage,\nfiltres (paramètres compris), calques, sélection, remplir / effacer, fusionner, aplatir.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!v || actions.empty() || actionRun.active);
    if (ImGui::Button("Exécuter") && v) StartAction(*v, actionSel);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("+")) {
        actions.push_back({"Action " + std::to_string(actions.size() + 1), {}});
        actionSel = (int)actions.size() - 1;
        actionRename = true;
        SaveActions();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nouvelle action");
    ImGui::SameLine();
    if (ImGui::Button("Éditeur...")) {
        panelOpen["Éditeur d'actions"].open = true;
        actionEditorFocus = true;
        builtinSel = -1;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ouvre l'éditeur d'actions : chaque étape expliquée, modifiable,\nactions toutes faites (conversions, tailles...), import / export.");
    if (actionRun.active)
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1, 1), "Exécution : %s (%zu/%zu)", actionRun.name.c_str(), actionRun.next + 1, actionRun.steps.size());
    if (actionRecording) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Enregistrement...");
    ImGui::Separator();
    ImGui::BeginChild("##actions", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.2f));
    for (int i = 0; i < (int)actions.size(); ++i) {
        Action& a = actions[i];
        ImGui::PushID(i);
        bool sel = i == actionSel;
        ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (sel ? ImGuiTreeNodeFlags_Selected : 0);
        bool open;
        if (sel && actionRename) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s", a.name.c_str());
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) || ImGui::IsItemDeactivated()) {
                a.name = buf;
                actionRename = false;
                SaveActions();
            }
            open = false;
        } else {
            open = ImGui::TreeNodeEx("##a", fl, "%s  (%zu)", a.name.c_str(), a.steps.size());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { actionSel = i; builtinSel = -1; }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { actionSel = i; actionRename = true; }
        }
        if (open) {
            for (int k = 0; k < (int)a.steps.size(); ++k) {
                ActionStep& s = a.steps[k];
                ImGui::PushID(k);
                if (ImGui::Checkbox("##on", &s.enabled)) SaveActions();
                ImGui::SameLine();
                const bool running = actionRun.active && actionRun.key == "u:" + a.name && actionRun.next < actionRun.srcIndex.size() && actionRun.srcIndex[actionRun.next] == k;
                if (ImGui::Selectable(ActionStepSummary(s).c_str(), running)) {
                    // A click opens the step in the editor.
                    actionSel = i;
                    builtinSel = -1;
                    actionStepSel = k;
                    panelOpen["Éditeur d'actions"].open = true;
                    actionEditorFocus = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
                    ImGui::TextUnformatted(ActionStepDescription(s).c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (ImGui::Button("Traitement par lots...", ImVec2(-1, 0))) {
        batch.open = true;
        if (batch.chain.empty() && !actions.empty()) batch.chain.push_back("u:" + actions[actionSel].name);
    }
    ImGui::End();
}

}  // namespace sx
