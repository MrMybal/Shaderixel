// Visual effect editor (Unreal-material-editor style) for graph-based effects.
#include "ui/App.h"
#include "ui/Theme.h"
#include "ui/CodeEdit.h"
#include "ui/Filter.h"
#include "doc/Brushes.h"
#include "doc/HlslImport.h"

#include <filesystem>
#include "doc/EffectGraph.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <fstream>
#include <limits>
#include <map>
#include <set>

namespace sx {

namespace {
constexpr float kW = 168.0f;
constexpr float kHead = 24.0f;
constexpr float kRowH = 20.0f;
constexpr float kCommentTitle = 24.0f;
constexpr float kRerouteW = 30.0f;

ImU32 CategoryColor(const char* cat) {
    std::string c = cat;
    if (c == "Entrées") return IM_COL32(36, 150, 168, 255);
    if (c == "Constantes") return IM_COL32(90, 110, 70, 255);
    if (c == "Paramètres") return IM_COL32(60, 150, 90, 255);
    if (c == "Maths") return IM_COL32(70, 84, 120, 255);
    if (c == "Vecteurs") return IM_COL32(110, 80, 150, 255);
    if (c == "Couleur") return IM_COL32(170, 90, 60, 255);
    if (c == "Procédural") return IM_COL32(170, 130, 50, 255);
    if (c == "Personnalisé") return IM_COL32(200, 92, 150, 255);
    if (c == "Fusion") return IM_COL32(190, 70, 110, 255);
    if (c == "Pinceau") return IM_COL32(60, 130, 200, 255);
    if (c == "Organisation") return IM_COL32(110, 112, 125, 255);
    return IM_COL32(52, 160, 110, 255);  // output
}

// Short description of a node (add menu tooltips, search).
const char* OpDescription(const std::string& op) {
    static const std::map<std::string, const char*> d = {
        {"input", "Couleur de l'image d'entrée au pixel courant."},
        {"sample", "Lit l'image d'entrée à d'autres coordonnées (déformations, flous, décalages)."},
        {"uv", "Coordonnées du pixel de 0 à 1 (x à droite, y vers le bas)."},
        {"texture", "Une image d'entrée vue comme texture : lue à n'importe quelle coordonnée dans un nœud HLSL."},
        {"canvassize", "Largeur, hauteur du canevas et taille d'un pixel (1 / largeur, 1 / hauteur)."},
        {"pixel", "Position du pixel en pixels."},
        {"size", "Taille de l'image en pixels."},
        {"const", "Un nombre fixe."},
        {"const2", "Deux nombres fixes (vecteur 2D)."},
        {"color", "Une couleur fixe (RGBA)."},
        {"param", "Un nombre réglable dans Propriétés (curseur)."},
        {"paramswitch", "Une case à cocher dans Propriétés : choisit l'une ou l'autre branche."},
        {"colorramp", "Rampe de couleurs (dégradé à arrêts) : une valeur de 0 à 1 devient une couleur."},
        {"normalheight", "Carte de normales calculée depuis la luminosité de l'image (relief)."},
        {"offsetuv", "Décale des coordonnées UV."},
        {"twirl", "Fait tourner les UV autour d'un centre, plus fort au milieu."},
        {"paramcolor", "Une couleur réglable dans Propriétés."},
        {"param2", "Une position ou un vecteur 2D réglable dans Propriétés."},
        {"add", "A + B"}, {"sub", "A - B"}, {"mul", "A × B"}, {"div", "A / B"},
        {"min", "Le plus petit de A et B."}, {"max", "Le plus grand de A et B."},
        {"pow", "A à la puissance B."}, {"lerp", "Mélange de A vers B selon T (0 : A, 1 : B)."},
        {"clamp", "Limite la valeur entre Min et Max."}, {"smoothstep", "Transition douce de 0 à 1 entre deux bornes."},
        {"step", "0 sous le seuil, 1 au-dessus."}, {"oneminus", "1 - x : inverse une valeur de 0 à 1."},
        {"abs", "Valeur absolue."}, {"negate", "Change le signe."}, {"saturate", "Limite entre 0 et 1."},
        {"frac", "Partie après la virgule (répétitions, motifs)."}, {"floor", "Arrondi vers le bas."}, {"ceil", "Arrondi vers le haut."},
        {"sqrt", "Racine carrée."}, {"sin", "Sinus (ondulations)."}, {"cos", "Cosinus (ondulations)."},
        {"exp2", "2 à la puissance x."}, {"log2", "Logarithme en base 2."}, {"if", "Choisit une valeur selon que A est plus grand que B ou non."},
        {"dot", "Produit scalaire de deux vecteurs."}, {"length", "Longueur d'un vecteur."}, {"distance", "Distance entre deux points."},
        {"normalize", "Vecteur de longueur 1 dans la même direction."}, {"split", "Sépare les composantes (x, y, z, w)."},
        {"append", "Colle deux valeurs en un vecteur plus grand."}, {"make4", "Construit un RGBA à partir de 4 valeurs."},
        {"make3", "Construit un RGB à partir de 3 valeurs."}, {"make2", "Construit un vecteur 2D à partir de 2 valeurs."},
        {"mask", "Garde certaines composantes (R, G, B, A)."},
        {"luma", "Luminosité perçue d'une couleur."}, {"desaturate", "Retire de la couleur (vers le gris)."},
        {"rgb2hsv", "Couleur RGB vers teinte, saturation, valeur."}, {"hsv2rgb", "Teinte, saturation, valeur vers RGB."},
        {"tosrgb", "Linéaire vers sRGB (affichage)."}, {"tolinear", "sRGB vers linéaire (calculs)."},
        {"noise", "Bruit procédural (nuages, grain)."}, {"checker", "Damier."}, {"rotateuv", "Tourne des coordonnées autour d'un centre."},
        {"opacity", "Opacité du calque (fusions)."}, {"blendover", "Compose le résultat de la fusion sur le dessous."},
        {"brush", "Données de la touche du pinceau : position, pression, graine, temps..."}, {"brushtex", "Une texture du pinceau."},
        {"roundtip", "Pointe ronde avec dureté."}, {"random", "Nombre aléatoire propre à chaque touche."},
        {"custom", "Votre code HLSL, avec ses entrées et sorties déclarées."},
        {"reroute", "Point de passage pour ranger un lien (double-clic sur un lien)."},
        {"sampleoffset", "L'image d'entrée décalée de quelques pixels (ombres portées, décalages)."},
        {"blur", "L'image d'entrée floutée (rayon en pixels)."},
        {"edges", "Contours de l'image d'entrée (filtre de Sobel sur la luminance)."},
        {"pi", "3,14159..."},
        {"tan", "Tangente."}, {"atan2", "Angle du vecteur (X, Y), de -Pi à Pi."},
        {"fmod", "Reste de A / B, toujours positif (répétitions)."}, {"round", "Arrondi à l'entier le plus proche."},
        {"sign", "-1, 0 ou 1 selon le signe."}, {"remap", "Ramène une valeur d'un intervalle vers un autre."},
        {"posterize", "Réduit une valeur à quelques niveaux (aplats)."}, {"cross", "Produit vectoriel de deux vecteurs 3D."},
        {"contrast", "Écarte les couleurs du pivot (1 : inchangé)."}, {"hueshift", "Tourne la teinte (1 : un tour complet)."},
        {"invert", "Négatif des couleurs (l'alpha est gardé)."},
        {"voronoi", "Cellules de Voronoï : distance au centre de la cellule et valeur aléatoire de la cellule."},
        {"fbm", "Bruit fractal : plusieurs octaves de bruit (nuages, terrains)."},
        {"radial", "1 au centre, 0 au rayon."}, {"lineargrad", "Dégradé de 0 à 1 dans une direction (angle en radians)."},
        {"polar", "Coordonnées polaires : distance au centre et angle (0 à 1)."},
        {"tiling", "Répète les coordonnées (motifs en mosaïque)."},
        {"circle", "Disque aux bords adoucis."}, {"rect", "Rectangle aux bords adoucis."},
    };
    auto it = d.find(op);
    return it == d.end() ? "" : it->second;
}

// Sub-group in the add menu (Maths is long).
// Library of effect functions: %LOCALAPPDATA%\\Shaderixel\\effect_functions\\<name>.json ({"name", "graph"}).
std::string FunctionLibraryDir() {
    const char* la = getenv("LOCALAPPDATA");
    return la ? std::string(la) + "\\Shaderixel\\effect_functions" : std::string();
}

std::vector<std::pair<std::string, std::string>> LibraryFunctions() {
    // Read again at most once a second (the add menu draws every frame).
    static std::vector<std::pair<std::string, std::string>> cache;
    static double last = -10;
    if (ImGui::GetTime() - last < 1.0) return cache;
    last = ImGui::GetTime();
    cache.clear();
    const std::string dir = FunctionLibraryDir();
    std::error_code ec;
    if (dir.empty() || !std::filesystem::exists(Utf8Path(dir), ec)) return cache;
    for (auto& e : std::filesystem::directory_iterator(Utf8Path(dir), ec)) {
        if (e.path().extension() != ".json") continue;
        std::string text;
        Json j;
        if (!ReadTextFile(Narrow(e.path().wstring()), text) || !Json::Parse(text, j)) continue;
        cache.push_back({j["name"].S(Narrow(e.path().stem().wstring())), j["graph"].Dump(0)});
    }
    std::sort(cache.begin(), cache.end());
    return cache;
}

bool SaveLibraryFunction(const std::string& name, const std::string& graphJson, std::string* error) {
    const std::string dir = FunctionLibraryDir();
    Json g;
    if (dir.empty() || !Json::Parse(graphJson, g)) { if (error) *error = "graphe illisible"; return false; }
    std::string file = name.empty() ? "Fonction" : name;
    for (auto& c : file) if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(dir), ec);
    Json j = Json::Object();
    j["name"] = name;
    j["graph"] = g;
    const std::string path = dir + "\\" + file + ".json", tmp = path + ".tmp";
    {
        std::ofstream f(Utf8Path(tmp), std::ios::binary);
        f << j.Dump(1);
        f.close();
        if (f.fail()) { if (error) *error = "écriture impossible : " + path; return false; }
    }
    std::filesystem::rename(Utf8Path(tmp), Utf8Path(path), ec);
    if (ec) { if (error) *error = "écriture impossible : " + path; return false; }
    return true;
}

std::string OpSubGroup(const std::string& op) {
    static const std::map<std::string, const char*> g = {
        {"add", "Arithmétique"}, {"sub", "Arithmétique"}, {"mul", "Arithmétique"}, {"div", "Arithmétique"}, {"negate", "Arithmétique"},
        {"abs", "Arithmétique"}, {"oneminus", "Arithmétique"},
        {"min", "Comparaison"}, {"max", "Comparaison"}, {"if", "Comparaison"}, {"step", "Comparaison"},
        {"clamp", "Limites et arrondis"}, {"saturate", "Limites et arrondis"}, {"frac", "Limites et arrondis"}, {"floor", "Limites et arrondis"},
        {"ceil", "Limites et arrondis"},
        {"lerp", "Interpolation"}, {"smoothstep", "Interpolation"},
        {"pow", "Puissances"}, {"sqrt", "Puissances"}, {"exp2", "Puissances"}, {"log2", "Puissances"},
        {"sin", "Trigonométrie"}, {"cos", "Trigonométrie"}, {"tan", "Trigonométrie"}, {"atan2", "Trigonométrie"},
        {"fmod", "Limites et arrondis"}, {"round", "Limites et arrondis"}, {"sign", "Arithmétique"},
        {"remap", "Interpolation"}, {"posterize", "Limites et arrondis"},
        {"voronoi", "Bruits"}, {"fbm", "Bruits"}, {"noise", "Bruits"},
        {"radial", "Formes et dégradés"}, {"lineargrad", "Formes et dégradés"}, {"circle", "Formes et dégradés"}, {"rect", "Formes et dégradés"},
        {"checker", "Formes et dégradés"},
        {"polar", "Coordonnées"}, {"tiling", "Coordonnées"}, {"rotateuv", "Coordonnées"}, {"offsetuv", "Coordonnées"}, {"twirl", "Coordonnées"},
    };
    auto it = g.find(op);
    return it == g.end() ? "" : it->second;
}

int Rows(const EGOpDef& d) { return std::max<int>((int)std::max(d.inputs.size(), d.outputs.size()), 1); }

// Result previews in the nodes (effect graphs): the computing nodes, not the plain values.
constexpr float kPvH = 100.0f;
bool gNodePreviews = false;
bool HasNodePreview(const EGNode& n) {
    static const char* none[] = {"reroute", "const", "const2", "color", "param", "paramcolor", "param2", "paramswitch", "texture", "output", "canvassize", "size",
                                 "opacity", "blendover", "brush", "brushtex", "roundtip", "random"};
    if (!gNodePreviews) return false;
    for (const char* o : none) if (n.op == o) return false;
    return true;
}

float NodeH(const EGNode& n, const EGOpDef& d);
float NodeW(const EGNode& n);

// The graph with its output showing one node: RGBA as is; scalars, 2D and RGB values through the RGB input
// (opaque), a 2D value completed with 0 (blue).
// A function's graph has no output node: one is added (previews, output types).
EffectGraph WithOutput(const EffectGraph& g) {
    EffectGraph g2 = g;
    if (!g2.OutputNode()) {
        EGNode o;
        o.id = g2.nextId++;
        o.op = "output";
        o.inputs.resize(3);
        g2.nodes.push_back(o);
    }
    return g2;
}

std::string PreviewGraphFor(const EffectGraph& g, Id node, int outType) {
    EffectGraph g2 = WithOutput(g);
    EGNode* o = g2.Find(g2.OutputNode());
    if (!o) return g2.Serialize();
    for (auto& l : o->inputs) l = {};
    if (o->inputs.size() < 3) o->inputs.resize(3);
    if (outType == 4 || outType == 0) {
        o->inputs[0] = {node, 0};
        return g2.Serialize();
    }
    // Opaque: the alpha pin gets 1.
    EGNode one;
    one.id = g2.nextId++;
    one.op = "const";
    one.value = {1, 0, 0, 0};
    const Id oneId = one.id;
    g2.nodes.push_back(one);
    o = g2.Find(g2.OutputNode());
    o->inputs[2] = {oneId, 0};
    if (outType == 2) {
        EGNode zero;
        zero.id = g2.nextId++;
        zero.op = "const";
        zero.value = {0, 0, 0, 0};
        EGNode app;
        app.id = g2.nextId++;
        app.op = "append";
        app.inputs = {{node, 0}, {zero.id, 0}};
        const Id appId = app.id;
        g2.nodes.push_back(zero);
        g2.nodes.push_back(app);
        o = g2.Find(g2.OutputNode());
        o->inputs[1] = {appId, 0};
    } else {
        o->inputs[1] = {node, 0};
    }
    return g2.Serialize();
}

// Nodes pushed down where they overlap (each keeps its column): previews make the nodes taller than the
// layout they were placed with. True when a node moved.
bool SpreadNodes(EffectGraph& g) {
    std::vector<EGNode*> ns;
    for (auto& n : g.nodes) if (FindOp(n.op)) ns.push_back(&n);
    std::sort(ns.begin(), ns.end(), [](const EGNode* a, const EGNode* b) { return a->pos.y < b->pos.y; });
    bool moved = false;
    for (size_t i = 0; i < ns.size(); ++i) {
        EGNode& n = *ns[i];
        const float h = NodeH(n, EffectiveOp(n));
        for (bool again = true; again;) {
            again = false;
            for (size_t j = 0; j < i; ++j) {
                const EGNode& o = *ns[j];
                const float oh = NodeH(o, EffectiveOp(o));
                if (std::abs(n.pos.x - o.pos.x) < std::max(NodeW(n), NodeW(o)) + 8 && n.pos.y < o.pos.y + oh + 12 && o.pos.y < n.pos.y + h + 12) {
                    n.pos.y = o.pos.y + oh + 14;
                    moved = again = true;
                }
            }
        }
    }
    return moved;
}

float NodeW(const EGNode& n) { return n.op == "reroute" ? kRerouteW : kW; }

float NodeH(const EGNode& n, const EGOpDef& d) {
    if (n.op == "reroute") return 20.0f;
    float extra = (n.op == "const" || n.op == "const2" || n.op == "color" || n.op == "param" || n.op == "paramcolor" || n.op == "param2" || n.op == "paramswitch" ||
                   n.op == "input" || n.op == "sample" || n.op == "texture" || n.op == "mask" || n.op == "custom" || n.op == "brushtex" ||
                   n.op == "sampleoffset" || n.op == "blur" || n.op == "edges")
                      ? kRowH
                      : 0.0f;
    return kHead + Rows(d) * kRowH + extra + 6 + (HasNodePreview(n) ? kPvH + 8 : 0.0f);
}

std::string ValueText(const EGNode& n) {
    char b[96];
    if (n.op == "const") snprintf(b, sizeof(b), "%.3g", n.value.x);
    else if (n.op == "const2") snprintf(b, sizeof(b), "%.3g, %.3g", n.value.x, n.value.y);
    else if (n.op == "param" || n.op == "paramcolor" || n.op == "param2") snprintf(b, sizeof(b), "« %s »", (n.text.empty() ? n.name : n.text).c_str());
    else if (n.op == "paramswitch") snprintf(b, sizeof(b), "« %s » %s", n.name.c_str(), n.value.x > 0.5f ? "(coché)" : "(décoché)");
    else if (n.op == "texture" && !n.name.empty()) snprintf(b, sizeof(b), "Entrée %d (%s)", n.index + 1, n.name.c_str());
    else if (n.op == "brushtex") snprintf(b, sizeof(b), "Texture %d%s%s", n.index + 1, n.text.empty() ? "" : " : ", n.text.c_str());
    else if (n.op == "input" || n.op == "sample" || n.op == "texture" || n.op == "sampleoffset" || n.op == "blur" || n.op == "edges")
        snprintf(b, sizeof(b), "Entrée %d", n.index + 1);
    else if (n.op == "mask") {
        std::string s;
        const char* c = "RGBA";
        int bits = n.index ? n.index : 7;
        for (int i = 0; i < 4; ++i) if (bits & (1 << i)) s += c[i];
        snprintf(b, sizeof(b), "%s", s.c_str());
    } else if (n.op == "custom") snprintf(b, sizeof(b), "code HLSL");
    else b[0] = 0;
    return b;
}
}  // namespace

EffectInstance* App::EditedEffect(DocView& v, std::string* where) {
    Document& d = *v.doc;
    if (d.Mode() == DocMode::Nodes) {
        Node* n = d.S().graph.Find(d.S().activeNode);
        if (n && n->kind == NodeKind::Effect) {
            if (where) *where = n->name;
            return &n->fx;
        }
        return nullptr;
    }
    Layer* l = d.ActiveLayer();
    if (!l) return nullptr;
    if ((l->kind == LayerKind::Fill || l->kind == LayerKind::Adjustment)) {
        if (where) *where = l->name;
        return &l->effect;
    }
    if (efEd.filterIndex >= 0 && efEd.filterIndex < (int)l->filters.size()) {
        if (where) *where = l->name + " (filtre " + std::to_string(efEd.filterIndex + 1) + ")";
        return &l->filters[efEd.filterIndex];
    }
    // No filter chosen in Propriétés: the first one made with a graph or code (in the document, then from the library).
    for (auto& f : l->filters)
        if (!f.graph.empty() || !f.customCode.empty()) {
            if (where) *where = l->name + " (filtre)";
            return &f;
        }
    for (auto& f : l->filters)
        if (auto def = f.Def(); def && !def->graph.empty()) {
            if (where) *where = l->name + " (filtre)";
            return &f;
        }
    return nullptr;
}

void App::DrawEffectEditor() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Éditeur d'effets", PanelFlag("Éditeur d'effets"), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (open) PopOutCorner("Éditeur d'effets");
    DocView* v = Active();
    if (!open || !v) { ImGui::End(); return; }
    Document& d = *v->doc;
    std::string where;
    auto& ed = efEd;
    Layer* layer = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
    static Id lastLayer = 0;  // the filter chosen from Propriétés belongs to that layer
    if ((layer ? layer->id : 0) != lastLayer) { lastLayer = layer ? layer->id : 0; ed.filterIndex = -1; }
    BrushSettings& brushTarget = tool == Tool::Eraser ? eraser : brush;
    static EffectInstance brushProxy;  // the brush graph seen as an effect instance
    EffectInstance* fx = nullptr;
    if (ed.target == 1 && layer) {
        fx = &layer->blendFx;
        where = layer->name + " (fusion)";
    } else if (ed.target == 2) {
        if (brushProxy.graph != brushTarget.tipGraph) brushProxy.graph = brushTarget.tipGraph;
        brushProxy.params = brushTarget.tipParams;
        fx = &brushProxy;
        where = tool == Tool::Eraser ? "pinceau de la gomme" : "pinceau";
    } else {
        fx = EditedEffect(*v, &where);
    }
    const bool brushMode = ed.target == 2;
    // A library effect made with a graph: its graph is shown directly, the first change makes it a copy in the document.
    EffectDefPtr libDef = fx && fx->graph.empty() && fx->customCode.empty() && !fx->effectId.empty() ? fx->Def() : nullptr;
    const bool libGraph = libDef && !libDef->graph.empty() && !ed.libCode;

    // Library page (templates, originals) or the edit itself.
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 8 * dpi, ImGui::GetCursorPosY() + 6 * dpi));
    if (ed.browse) {
        DrawEffectLibraryPage();
        ImGui::End();
        return;
    }
    DrawEditSessionBar();
    if (ed.browse) { ImGui::End(); return; }
    ImGui::SameLine(0, 20 * dpi);
    // What is edited: effect, blend of the active layer, current brush.
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Éditer :");
    ImGui::SameLine();
    ImGui::RadioButton("Effet", &ed.target, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Fusion du calque", &ed.target, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Pinceau", &ed.target, 2);

    // Code and library items (graphs continue below with their canvas).
    if (ed.target == 2 && brushTarget.tipGraph.empty()) {
        ImGui::BeginChild("##brushcode", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        if (TipDef(brushTarget)) DrawBrushPad(*v, brushTarget);
        DrawBrushCodeEditor(*v, brushTarget);
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    if (fx && fx->graph.empty() && !libGraph && (!fx->customCode.empty() || !fx->effectId.empty())) {
        ImGui::TextDisabled("  Édition : %s", where.c_str());
        if (libDef && !libDef->graph.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Voir le graphe")) ed.libCode = false;
        }
        ImGui::BeginChild("##fxcode", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        if (!fx->customCode.empty()) DrawEffectCodeEditor(*v, *fx);
        else DrawLibraryItemView(*v, *fx, ed.target == 1);
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    if (!fx || (fx->graph.empty() && !libGraph)) {
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 16 * dpi, ImGui::GetCursorPosY() + 12 * dpi));
        ImGui::BeginGroup();
        if (ed.target == 1) {
            if (!layer) ImGui::TextWrapped("Sélectionnez un calque (mode calques) : sa fusion avec les calques du dessous se construit ici en graphe.");
            else {
                ImGui::TextWrapped("Fusion de « %s » : A = calques du dessous, B = ce calque, masque, opacité du calque.", layer->name.c_str());
                if (ImGui::Button("Créer une fusion en graphe")) {
                    Edit e(d, v->history, "Fusion en graphe");
                    layer->blendFx = {};
                    layer->blendFx.graph = EffectGraph::DefaultBlend().Serialize();
                    if (auto def = layer->blendFx.Def()) layer->blendFx.params = def->DefaultParams();
                    layer->passThrough = false;
                    d.SyncGraph();
                    e.Commit();
                }
                ImGui::SameLine();
                if (ImGui::Button("Créer une fusion en code")) {
                    Edit e(d, v->history, "Fusion personnalisée");
                    layer->blendFx = {};
                    layer->blendFx.customCode = kCustomBlendTemplate;
                    if (auto def = layer->blendFx.Def()) layer->blendFx.params = def->DefaultParams();
                    layer->passThrough = false;
                    d.SyncGraph();
                    e.Commit();
                }
            }
            ImGui::EndGroup();
            ImGui::End();
            return;
        }
        if (ed.target == 2) {
            ImGui::TextWrapped("Le pinceau actif n'est pas un graphe. Un graphe de pinceau dessine la pointe (nœud « Touche du pinceau » : position, pression, graine, temps...) ; le placement des touches se règle à gauche.");
            if (ImGui::Button("Créer un pinceau en graphe")) {
                brushTarget.tipId.clear();
                brushTarget.tipCode.clear();
                brushTarget.tipGraph = EffectGraph::DefaultBrush().Serialize();
                if (auto td = TipDef(brushTarget); td && td->def) brushTarget.tipParams = td->def->DefaultParams();
            }
            ImGui::EndGroup();
            ImGui::End();
            return;
        }
        if (!fx) {
            // Nothing to edit here: the library (templates, originals, new).
            ImGui::EndGroup();
            DrawEffectLibraryPage();
            ImGui::End();
            return;
        }
        ImGui::TextWrapped("Cet effet n'est pas un graphe.");
        ImGui::Spacing();
        if (fx && ImGui::Button("Convertir cet effet en graphe vide")) {
            DocState before = d.S();
            auto old = fx->Def();
            fx->graph = EffectGraph::Default().Serialize();
            fx->customCode.clear();
            fx->effectId.clear();
            fx->SyncParams(old);
            d.SyncGraph();
            v->history.PushState("Créer un graphe d'effet", before, d.S());
        }
        if (ImGui::Button("Nouvel effet depuis du HLSL...")) hlslImportRequest = true;
        ui::HelpTooltip("Colle ou ouvre du HLSL : entrées et sorties créées depuis les annotations //@in //@out ou la signature de la fonction.");
        DrawHlslImportPopup(nullptr, {}, nullptr);
        if (ImGui::Button("Nouveau calque de réglage « Effet visuel »")) {
            Edit e(d, v->history, "Effet visuel");
            if (d.Mode() == DocMode::Layers) {
                Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
                l.name = "Effet visuel";
                l.effect.graph = EffectGraph::Default().Serialize();
                d.SyncGraph();
            } else {
                Node& n = d.AddEffectNode("", "", nodeEd.contextPos);
                n.fx.graph = EffectGraph::Default().Serialize();
                n.name = "Effet visuel";
                n.inputs.assign(NodeInputCount(n), 0);
            }
            e.Commit();
        }
        ImGui::EndGroup();
        ImGui::End();
        return;
    }

    // Parse (cached on the JSON text).
    const std::string& graphJson = libGraph ? libDef->graph : fx->graph;
    if (ed.cachedJson != graphJson) {
        // Another graph (not our own edit, which keeps cachedJson up to date): framed, selection cleared.
        if (!EffectGraph::Deserialize(graphJson, ed.graph)) ed.graph = EffectGraph::Default();
        ed.cachedJson = graphJson;
        ed.frameAll = true;
        ed.multi.clear();
        ed.funcPath.clear();
        ed.funcLevels.clear();
    }
    // Functions: back to the parent graph / into a function node (requested last frame).
    if (ed.exitFunc && !ed.funcPath.empty()) {
        EffectGraph parent = ed.funcLevels.back();
        const Id fnId = ed.funcPath.back();
        if (EGNode* fn = parent.Find(fnId)) {
            fn->text = ed.graph.Serialize();
            SyncFunctionInterface(*fn, ed.graph);
        }
        ed.graph = std::move(parent);
        ed.funcLevels.pop_back();
        ed.funcPath.pop_back();
        ed.selected = fnId;
        ed.multi = {fnId};
        ed.frameAll = true;
    }
    ed.exitFunc = false;
    if (ed.enterFunc) {
        if (EGNode* fn = ed.graph.Find(ed.enterFunc); fn && fn->op == "function") {
            EffectGraph inner;
            if (EffectGraph::Deserialize(fn->text, inner)) {
                ed.funcLevels.push_back(ed.graph);
                ed.funcPath.push_back(fn->id);
                ed.graph = std::move(inner);
                ed.selected = 0;
                ed.multi.clear();
                ed.frameAll = true;
            }
        }
        ed.enterFunc = 0;
    }
    EffectGraph& g = ed.graph;
    // The effect's whole graph: the edited level written back into its function nodes, up to the top.
    auto composeRoot = [&]() {
        std::string child = g.Serialize();
        for (int i = (int)ed.funcLevels.size() - 1; i >= 0; --i) {
            EffectGraph& parent = ed.funcLevels[i];
            if (EGNode* fn = parent.Find(ed.funcPath[i])) {
                fn->text = child;
                EffectGraph inner;
                if (EffectGraph::Deserialize(child, inner)) SyncFunctionInterface(*fn, inner);
            }
            child = parent.Serialize();
        }
        return child;
    };
    gNodePreviews = ed.nodePreviews && !brushMode && ed.graph.kind == EGKind::Effect;
    bool changed = false;     // structural change -> push undo now
    bool live = false;        // continuous change (drag/slider) -> write without undo
    std::string changeLabel = "Modifier le graphe";
    ImGuiIO& io = ImGui::GetIO();

    // ---------------------------------------------------------- details panel
    const EGNode* selNode = g.Find(ed.selected);
    float panelW = (selNode && selNode->op == "custom" ? 400.0f : 270.0f) * dpi;  // custom nodes: pins + code need room
    ImGui::BeginChild("##efdetails", ImVec2(panelW, 0), ImGuiChildFlags_Borders, 0);
    ImGui::TextDisabled("Édition : %s", where.c_str());
    if (!ed.funcPath.empty()) {
        // Inside a function: where, and the way back.
        std::string crumb = g.name.empty() ? "Graphe" : "Graphe";
        for (size_t i = 0; i < ed.funcPath.size(); ++i)
            if (const EGNode* fn = ed.funcLevels[i].Find(ed.funcPath[i])) crumb += "  >  " + (fn->name.empty() ? std::string("Fonction") : fn->name);
        ImGui::TextColored(ImVec4(0.75f, 0.6f, 1, 1), "%s", crumb.c_str());
        if (ImGui::Button("\xE2\x97\x80 Remonter au graphe parent")) ed.exitFunc = true;
        ui::HelpTooltip("Les entrées / sorties de la fonction sont ses nœuds « Entrée de fonction » et « Sortie de fonction ».");
    }
    if (EGNode* fsel = g.Find(ed.selected); fsel && (fsel->op == "function" || fsel->op == "funcin" || fsel->op == "funcout")) {
        ImGui::SeparatorText(fsel->op == "function" ? "Fonction" : fsel->op == "funcin" ? "Entrée de fonction" : "Sortie de fonction");
        if (ImGui::InputText("Nom##fn", &fsel->name)) live = true;
        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
        if (fsel->op != "function") {
            if (ImGui::InputInt("Ordre##fn", &fsel->index)) { fsel->index = std::max(0, fsel->index); changed = true; }
            ui::HelpTooltip("Place de cette broche sur le nœud de la fonction.");
        } else {
            if (ImGui::Button("Ouvrir (double-clic)")) ed.enterFunc = fsel->id;
            ImGui::SameLine();
            if (ImGui::Button("Dégrouper")) {
                const Id fid = fsel->id;
                if (ExpandFunction(g, fid)) {
                    ed.selected = 0;
                    ed.multi.clear();
                    changed = true;
                    changeLabel = "Dégrouper la fonction";
                }
                fsel = nullptr;
            }
            if (fsel && ImGui::Button("Enregistrer dans la bibliothèque")) {
                std::string err;
                if (SaveLibraryFunction(fsel->name, fsel->text, &err)) Status("Fonction enregistrée : " + fsel->name + " (menu d'ajout > Fonctions de la bibliothèque)");
                else Status("Fonction : " + err);
            }
        }
    }
    if (ed.multi.size() >= 2) {
        if (ImGui::Button("Créer une fonction (Ctrl+G)")) ed.demoCollapse = true;
        ui::HelpTooltip("Les nœuds sélectionnés deviennent un seul nœud réutilisable (comme les Material Functions d'Unreal) :\n"
                        "les liens qui entrent deviennent ses entrées, ceux qui sortent ses sorties. Double-clic pour l'ouvrir.");
    }
    if (libGraph) {
        ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), "Effet de la bibliothèque (%s)", libDef->category.c_str());
        ImGui::TextWrapped("Protégé : la première modification en crée une copie dans ce document.");
        if (ImGui::SmallButton("Voir le code")) ed.libCode = true;
    }
    ImGui::SeparatorText("Effet");
    ImGui::PushItemWidth(-90 * dpi);
    if (ed.funcPath.empty()) {  // (inside a function: the effect's name is on the top graph)
        if (ImGui::InputText("Nom", &g.name)) live = true;
        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
        if (ImGui::InputText("Catégorie", &g.category)) live = true;
        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    }
    auto trackLive = [&](bool edited) {
        if (edited) live = true;
        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    };
    if (g.kind == EGKind::Effect) {
        if (ImGui::SliderInt("Entrées", &g.inputs, 0, 4)) changed = true;
        if (ImGui::Checkbox("Prémultiplié", &g.premultiplied)) changed = true;
    } else if (g.kind == EGKind::Blend) {
        ImGui::TextDisabled("Entrée 1 = A (dessous), 2 = B (calque), 3 = masque.");
    } else {
        ImGui::SeparatorText("Placement des touches");
        trackLive(ImGui::SliderFloat("Espacement", &g.spacing, 0.01f, 2.0f, "%.2f"));
        trackLive(ImGui::SliderFloat("Aérographe", &g.airbrush, 0, 120, g.airbrush > 0 ? "%.0f /s" : "non"));
        ui::HelpTooltip("Touches par seconde tant que le bouton reste appuyé (0 : seulement en bougeant).");
        trackLive(ImGui::SliderFloat("Suivre le trait", &g.follow, 0, 1));
        trackLive(ImGui::SliderFloat("Dispersion", &g.scatter, 0, 4));
        trackLive(ImGui::SliderFloat("Taille aléatoire", &g.sizeJitter, 0, 1));
        trackLive(ImGui::SliderFloat("Angle aléatoire", &g.angleJitter, 0, 1));
        trackLive(ImGui::SliderFloat("Opacité aléatoire", &g.opacityJitter, 0, 1));
        ImGui::SeparatorText("Réaction au stylet");
        trackLive(ImGui::SliderFloat("Inclinaison -> angle", &g.tiltAngle, 0, 1));
        ui::HelpTooltip("La pointe s'oriente dans la direction où le stylet penche.");
        trackLive(ImGui::SliderFloat("Inclinaison -> taille", &g.tiltSize, 0, 2));
        ui::HelpTooltip("Stylet couché : touche plus large (comme un crayon sur la tranche).");
        trackLive(ImGui::SliderFloat("Rotation -> angle", &g.twistAngle, 0, 1));
        ui::HelpTooltip("Rotation du stylet sur lui-même (stylets type Art Pen).");
        trackLive(ImGui::SliderFloat("Vitesse -> taille", &g.speedSize, -1, 1));
        ui::HelpTooltip("Négatif : plus fin quand on va vite ; positif : plus épais.");
        ImGui::TextDisabled("Pression : Options > Tablette (taille, flux, courbe).");
        DrawBrushPad(*v, brushTarget);
    }
    EGNode* sel = g.Find(ed.selected);
    if (sel) {
        const EGOpDef* od = FindOp(sel->op);
        ImGui::SeparatorText(od ? od->label : sel->op.c_str());
        const std::string& op = sel->op;
        auto track = [&](bool edited) {
            if (edited) live = true;
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
        };
        if (op == "const") track(ImGui::DragFloat("Valeur", &sel->value.x, 0.01f));
        else if (op == "const2") track(ImGui::DragFloat2("Valeur", &sel->value.x, 0.01f));
        else if (op == "color") track(ImGui::ColorEdit4("Couleur", &sel->value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR));
        else if (op == "param" || op == "paramcolor" || op == "param2") {
            track(ImGui::InputText("Nom", &sel->name));
            if (op == "param2") {
                track(ImGui::DragFloat2("Défaut", &sel->value.x, 0.01f));
                track(ImGui::DragFloat("Min", &sel->min, 0.01f));
                track(ImGui::DragFloat("Max", &sel->max, 0.01f));
            } else if (op == "param") {
                track(ImGui::DragFloat("Défaut", &sel->value.x, 0.01f));
                track(ImGui::DragFloat("Min", &sel->min, 0.01f));
                track(ImGui::DragFloat("Max", &sel->max, 0.01f));
            } else {
                track(ImGui::ColorEdit4("Défaut", &sel->value.x, ImGuiColorEditFlags_Float));
            }
            ImGui::TextDisabled("Réglable dans Propriétés.");
        } else if (op == "paramswitch") {
            track(ImGui::InputText("Nom", &sel->name));
            bool on = sel->value.x > 0.5f;
            if (ImGui::Checkbox("Coché par défaut", &on)) { sel->value.x = on ? 1.0f : 0.0f; changed = true; }
            ImGui::TextDisabled("Case à cocher dans Propriétés.");
        } else if (op == "colorramp") {
            // Gradient bar: stops dragged along it, click to add one, right click to remove, the chosen stop's color below.
            auto stops = ParseRamp(sel->text);
            static int rampSel = 0;
            static Id rampNode = 0;
            if (rampNode != sel->id) { rampNode = sel->id; rampSel = 0; }
            rampSel = std::clamp(rampSel, 0, (int)stops.size() - 1);
            const float bw = ImGui::GetContentRegionAvail().x, bh = 22 * dpi;
            const ImVec2 b0 = ImGui::GetCursorScreenPos(), b1(b0.x + bw, b0.y + bh);
            ImDrawList* rdl = ImGui::GetWindowDrawList();
            auto col = [](const float4& c) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, 1)); };
            for (size_t i = 0; i + 1 < stops.size(); ++i) {
                const float x0 = b0.x + stops[i].pos * bw, x1 = b0.x + stops[i + 1].pos * bw;
                rdl->AddRectFilledMultiColor(ImVec2(x0, b0.y), ImVec2(std::max(x1, x0 + 1), b1.y), col(stops[i].color), col(stops[i + 1].color), col(stops[i + 1].color), col(stops[i].color));
            }
            if (stops.front().pos > 0) rdl->AddRectFilled(b0, ImVec2(b0.x + stops.front().pos * bw, b1.y), col(stops.front().color));
            if (stops.back().pos < 1) rdl->AddRectFilled(ImVec2(b0.x + stops.back().pos * bw, b0.y), b1, col(stops.back().color));
            rdl->AddRect(b0, b1, ui::col::Border);
            ImGui::InvisibleButton("##rampbar", ImVec2(bw, bh + 12 * dpi));
            const ImVec2 mp = ImGui::GetIO().MousePos;
            const float mt = std::clamp((mp.x - b0.x) / std::max(bw, 1.0f), 0.0f, 1.0f);
            int hoverStop = -1;
            for (int i = 0; i < (int)stops.size(); ++i) {
                const float x = b0.x + stops[i].pos * bw;
                const ImVec2 tri[3] = {ImVec2(x, b1.y + 1), ImVec2(x - 6 * dpi, b1.y + 11 * dpi), ImVec2(x + 6 * dpi, b1.y + 11 * dpi)};
                rdl->AddTriangleFilled(tri[0], tri[1], tri[2], col(stops[i].color));
                rdl->AddTriangle(tri[0], tri[1], tri[2], i == rampSel ? ui::col::Accent : IM_COL32(200, 200, 200, 200), i == rampSel ? 2.0f : 1.0f);
                if (std::abs(mp.x - x) < 7 * dpi && mp.y > b0.y && mp.y < b1.y + 13 * dpi) hoverStop = i;
            }
            static bool rampDrag = false;
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                if (hoverStop >= 0) {
                    rampSel = hoverStop;
                    rampDrag = true;
                } else {
                    // A new stop with the color the bar has there.
                    RampStop ns;
                    ns.pos = mt;
                    ns.color = stops.back().color;
                    for (size_t i = 0; i + 1 < stops.size(); ++i)
                        if (mt >= stops[i].pos && mt <= stops[i + 1].pos) {
                            const float k = (mt - stops[i].pos) / std::max(stops[i + 1].pos - stops[i].pos, 1e-5f);
                            const float4 a = stops[i].color, b = stops[i + 1].color;
                            ns.color = {a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k, a.w + (b.w - a.w) * k};
                        }
                    stops.push_back(ns);
                    std::stable_sort(stops.begin(), stops.end(), [](const RampStop& a, const RampStop& b) { return a.pos < b.pos; });
                    for (int i = 0; i < (int)stops.size(); ++i) if (stops[i].pos == ns.pos) rampSel = i;
                    sel->text = FormatRamp(stops);
                    changed = true;
                }
            }
            if (rampDrag && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                stops[rampSel].pos = mt;
                const RampStop moved = stops[rampSel];
                std::stable_sort(stops.begin(), stops.end(), [](const RampStop& a, const RampStop& b) { return a.pos < b.pos; });
                for (int i = 0; i < (int)stops.size(); ++i) if (stops[i].pos == moved.pos && stops[i].color.x == moved.color.x) rampSel = i;
                sel->text = FormatRamp(stops);
                live = true;
            } else if (rampDrag) {
                rampDrag = false;
                changed = true;
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && hoverStop >= 0 && stops.size() > 2) {
                stops.erase(stops.begin() + hoverStop);
                rampSel = std::min(rampSel, (int)stops.size() - 1);
                sel->text = FormatRamp(stops);
                changed = true;
            }
            ui::HelpTooltip("Clic sur la barre : ajouter un arrêt. Glisser un repère : le déplacer. Clic droit : le supprimer.");
            RampStop& cur = stops[rampSel];
            bool edited = false;
            edited |= ImGui::ColorEdit4("Couleur de l'arrêt", &cur.color.x);
            const bool c1 = ImGui::IsItemDeactivatedAfterEdit();
            edited |= ImGui::SliderFloat("Position", &cur.pos, 0, 1, "%.3f");
            const bool c2 = ImGui::IsItemDeactivatedAfterEdit();
            if (edited) {
                sel->text = FormatRamp(stops);
                live = true;
            }
            if (c1 || c2) changed = true;
            if (ImGui::SmallButton("Inverser la rampe")) {
                for (auto& st : stops) st.pos = 1.0f - st.pos;
                sel->text = FormatRamp(stops);
                changed = true;
            }
        } else if (op == "brushtex") {
            int idx = sel->index + 1;
            if (ImGui::SliderInt("Texture", &idx, 1, 4)) { sel->index = idx - 1; changed = true; }
            track(ImGui::InputText("Libellé", &sel->name));
            track(ImGui::InputText("Image par défaut", &sel->text));
            ui::HelpTooltip("Chemin relatif au dossier des pinceaux (ex. textures/grain.png).\nL'utilisateur choisit une autre image dans les réglages du pinceau.");
            if (ImGui::SmallButton("Bibliothèque")) ImGui::OpenPopup("##btexlib");
            if (ImGui::BeginPopup("##btexlib")) {
                std::error_code ec;
                for (auto& e : std::filesystem::directory_iterator(Utf8Path(Brushes().Dir() + "/textures"), ec)) {
                    std::string rel = "textures/" + Narrow(e.path().filename().wstring());
                    if (ImGui::Selectable(rel.c_str(), rel == sel->text)) { sel->text = rel; changed = true; }
                }
                ImGui::EndPopup();
            }
        } else if (op == "input" || op == "sample" || op == "texture" || op == "sampleoffset" || op == "blur" || op == "edges") {
            int idx = sel->index + 1;
            if (ImGui::SliderInt("Entrée", &idx, 1, std::max(g.kind == EGKind::Blend ? 3 : g.inputs, 1))) { sel->index = idx - 1; changed = true; }
        } else if (op == "mask") {
            int bits = sel->index ? sel->index : 7;
            const char* names[] = {"R", "G", "B", "A"};
            for (int i = 0; i < 4; ++i) {
                bool on = bits & (1 << i);
                if (i) ImGui::SameLine();
                if (ImGui::Checkbox(names[i], &on)) { bits = on ? bits | (1 << i) : bits & ~(1 << i); changed = true; }
            }
            sel->index = bits ? bits : 1;
        } else if (op == "custom") {
            track(ImGui::InputText("Titre", &sel->name));
            if (!sel->HasPins()) {
                ImGui::TextDisabled("Entrées A B C D (float4), retourner un float4.");
                if (ImGui::Button("Déclarer les broches")) {
                    for (const char* nm : {"A", "B", "C", "D"}) {
                        EGPinDecl d;
                        d.name = nm;
                        d.type = kPinFloat4;
                        sel->pins.push_back(d);
                    }
                    EGPinDecl o;
                    o.name = "Resultat";
                    o.type = kPinFloat4;
                    sel->outs = {o};
                    changed = true;
                }
                ui::HelpTooltip("Entrées et sorties nommées et typées (float à float4, texture lue à n'importe quel UV).");
            } else {
                // Declared pins: name (HLSL identifier) + type; textures are read with Texture2DSample(T, S, uv).
                // Pin types (EGPinType order): float, float2, float3, float4, Texture, int, bool, couleur.
                static const char* kPinTypes = "float\0float2\0float3\0float4\0Texture\0int\0bool\0couleur\0";
                auto pinList = [&](std::vector<EGPinDecl>& pins, const char* title, bool inputs) {
                    ImGui::SeparatorText(title);
                    int removeAt = -1, moveUp = -1;
                    for (size_t i = 0; i < pins.size(); ++i) {
                        EGPinDecl& pin = pins[i];
                        ImGui::PushID((int)i + (inputs ? 0 : 1000));
                        std::string head = pin.name + "  (" + PinTypeName(pin.type) + ")" + (pin.category.empty() ? "" : "  [" + pin.category + "]");
                        bool open = ImGui::TreeNodeEx("##pin", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", head.c_str());
                        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight() * 2 - 8);
                        if (i > 0 && ImGui::SmallButton("^")) moveUp = (int)i;
                        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
                        if (ImGui::SmallButton("x")) removeAt = (int)i;
                        if (open) {
                            track(ImGui::InputText("Nom", &pin.name, ImGuiInputTextFlags_CharsNoBlank));
                            ui::HelpTooltip("Identifiant utilisé dans le code.");
                            int t = pin.type - 1;
                            bool texOk = inputs;
                            if (ImGui::Combo("Type", &t, kPinTypes)) {
                                int nt = std::clamp(t + 1, 1, (int)kPinTypeCount);
                                if (nt == kPinTexture && !texOk) nt = kPinFloat4;
                                if (nt == kPinColor && pin.def == float4{}) pin.def = {1, 1, 1, 1};
                                if (nt == kPinInt && pin.max == 1) pin.max = 10;
                                pin.type = nt;
                                changed = true;
                            }
                            track(ImGui::InputText("Catégorie", &pin.category));
                            ui::HelpTooltip(inputs ? "Groupe sous lequel le réglage apparaît dans Propriétés." : "Groupe de la sortie.");
                            if (inputs && pin.type != kPinTexture) {
                                track(ImGui::InputText("Libellé", &pin.label));
                                ui::HelpTooltip("Nom affiché dans Propriétés (sinon le nom).");
                                // Default + range: the value used when nothing is connected (then a parameter of the effect).
                                switch (pin.type) {
                                    case kPinFloat: track(ImGui::DragFloat("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinInt: {
                                        int iv = (int)std::lround(pin.def.x);
                                        if (ImGui::DragInt("Défaut", &iv)) { pin.def.x = (float)iv; live = true; }
                                        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
                                        break;
                                    }
                                    case kPinBool: {
                                        bool bv = pin.def.x > 0.5f;
                                        if (ImGui::Checkbox("Défaut", &bv)) { pin.def.x = bv ? 1.0f : 0.0f; changed = true; }
                                        break;
                                    }
                                    case kPinFloat2: track(ImGui::DragFloat2("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinFloat3: track(ImGui::DragFloat3("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinFloat4: track(ImGui::DragFloat4("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinColor: track(ImGui::ColorEdit4("Défaut", &pin.def.x, ImGuiColorEditFlags_Float)); break;
                                }
                                if (pin.type != kPinBool && pin.type != kPinColor) {
                                    float r[2] = {pin.min, pin.max};
                                    if (ImGui::DragFloat2("Min / max", r, 0.01f)) { pin.min = r[0]; pin.max = r[1]; live = true; }
                                    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
                                }
                                ImGui::TextDisabled("Non reliée : réglable dans Propriétés.");
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    if (removeAt >= 0) {
                        pins.erase(pins.begin() + removeAt);
                        if (inputs && removeAt < (int)sel->inputs.size()) sel->inputs.erase(sel->inputs.begin() + removeAt);
                        changed = true;
                    }
                    if (moveUp > 0) {
                        std::swap(pins[moveUp], pins[moveUp - 1]);
                        if (inputs && moveUp < (int)sel->inputs.size()) std::swap(sel->inputs[moveUp], sel->inputs[moveUp - 1]);
                        changed = true;
                    }
                    if (ImGui::SmallButton(inputs ? "+ Entrée" : "+ Sortie")) {
                        EGPinDecl d;
                        d.name = (inputs ? "In" : "Out") + std::to_string(pins.size() + 1);
                        d.type = inputs ? kPinFloat : kPinFloat4;
                        pins.push_back(d);
                        changed = true;
                    }
                };
                pinList(sel->pins, "Entrées", true);
                pinList(sel->outs, "Sorties (une seule : return ; plusieurs : paramètres out)", false);
                sel->inputs.resize(sel->pins.size());
                ImGui::TextDisabled("Déclarations (fonctions, constantes)");
                track(ui::CodeEditor("##decl", &sel->decl, ImVec2(-1, 90 * dpi), false, nullptr, ed.declMarks.count(sel->id) ? &ed.declMarks[sel->id] : nullptr));
            }
            // Names known in the code: pins, then the effect context.
            std::vector<std::string> names;
            for (auto& p : sel->pins) names.push_back(p.name);
            for (auto& p : sel->outs) names.push_back(p.name);
            if (!sel->HasPins()) for (const char* k : {"A", "B", "C", "D"}) names.push_back(k);  // legacy float4 inputs
            for (const char* k : {"UV", "ctx", "CyCanvasSize", "CyInput", "CyInputSampler"}) names.push_back(k);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Code");
            ImGui::SameLine();
            if (ImGui::SmallButton("Mettre en forme")) {
                std::string f = ui::FormatHlsl(sel->text);
                if (f != sel->text) { sel->text = f; changed = true; changeLabel = "Mettre en forme le code"; }
            }
            ui::HelpTooltip("Une instruction par ligne, blocs indentés (code compacté sur une ligne).");
            ImGui::SameLine();
            if (ImGui::SmallButton("Agrandir")) ed.bigCode = sel->id;
            const std::vector<ui::CodeMark>* marks = ed.codeMarks.count(sel->id) ? &ed.codeMarks[sel->id] : nullptr;
            if (marks) ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%d erreur(s) : survolez la ligne en rouge", (int)marks->size());
            track(ui::CodeEditor("##code", &sel->text, ImVec2(-1, 260 * dpi), false, &names, marks));
            // Large editor in its own window (same node, same undo).
            if (ed.bigCode == sel->id) {
                bool openBig = true;
                ImGui::SetNextWindowSize(ImVec2(980 * dpi, 680 * dpi), ImGuiCond_FirstUseEver);
                if (ImGui::Begin(("Code du nœud : " + sel->name + "###efbigcode").c_str(), &openBig)) {
                    if (ImGui::Button("Mettre en forme")) {
                        std::string f = ui::FormatHlsl(sel->text);
                        if (f != sel->text) { sel->text = f; changed = true; changeLabel = "Mettre en forme le code"; }
                    }
                    if (!sel->decl.empty() || sel->HasPins()) {
                        ImGui::TextDisabled("Déclarations (fonctions, constantes)");
                        track(ui::CodeEditor("##bigdecl", &sel->decl, ImVec2(-1, 120 * dpi), false, &names, ed.declMarks.count(sel->id) ? &ed.declMarks[sel->id] : nullptr));
                    }
                    ImGui::TextDisabled("Code");
                    track(ui::CodeEditor("##bigcode", &sel->text, ImVec2(-1, -1), false, &names, marks));
                }
                ImGui::End();
                if (!openBig) ed.bigCode = 0;
            }
        }
        // Unconnected inputs: their value on this node (constants on the pins, like Unreal).
        if (op != "custom" && op != "output") {
            const EGOpDef eff = EffectiveOp(*sel);
            bool any = false;
            for (size_t i = 0; i < eff.inputs.size(); ++i) {
                if (eff.inputs[i].defValue < 0) continue;  // implicit sources (UV, opacity...)
                if (!any) {
                    ImGui::SeparatorText("Entrées non reliées");
                    any = true;
                }
                const bool linked = i < sel->inputs.size() && sel->inputs[i].node;
                ImGui::PushID((int)i + 500);
                float val = sel->PinValue(i, eff.inputs[i].defValue);
                ImGui::BeginDisabled(linked);
                if (ImGui::DragFloat(eff.inputs[i].name[0] ? eff.inputs[i].name : "Entrée", &val, 0.01f)) {
                    if (sel->pinValues.size() <= i) sel->pinValues.resize(i + 1, std::numeric_limits<float>::quiet_NaN());
                    sel->pinValues[i] = val;
                    live = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) { changed = true; changeLabel = "Valeur d'entrée"; }
                ImGui::EndDisabled();
                if (linked) ImGui::SetItemTooltip("Reliée : la valeur vient du lien.");
                if (sel->PinValueSet(i)) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Défaut")) {
                        sel->pinValues[i] = std::numeric_limits<float>::quiet_NaN();
                        changed = true;
                        changeLabel = "Valeur d'entrée";
                    }
                }
                ImGui::PopID();
            }
            if (any) ImGui::TextDisabled("Double-clic sur une entrée du nœud : la modifier sur place.");
        }
        if (op != "output" && ImGui::Button("Supprimer le nœud")) {
            Id rid = sel->id;
            g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const EGNode& n) { return n.id == rid; }), g.nodes.end());
            for (auto& n : g.nodes) for (auto& l : n.inputs) if (l.node == rid) l = {};
            ed.selected = 0;
            sel = nullptr;
            changed = true;
            changeLabel = "Supprimer un nœud";
        }
    }
    ImGui::PopItemWidth();

    // Compile status
    ImGui::SeparatorText("Compilation");
    // Inside a function: the whole effect is compiled (the function alone has no output).
    EffectGraph rootGraph;
    const bool inFunc = !ed.funcPath.empty() && EffectGraph::Deserialize(ed.cachedJson, rootGraph);
    EGCompileResult cr = CompileEffectGraph(inFunc ? rootGraph : g);
    // Output types of the edited level (inside a function: its own nodes, compiled with an output).
    const EGCompileResult crLevel = inFunc ? CompileEffectGraph(WithOutput(g)) : EGCompileResult{};
    const EGCompileResult& crTypes = inFunc ? crLevel : cr;
    std::string shaderErr;
    Id renderNode = 0;
    for (auto& n : d.S().graph.nodes)
        if (n.kind == NodeKind::Effect && (&n.fx == fx || (!fx->graph.empty() && n.fx.graph == fx->graph))) renderNode = n.id;
    bool failed = renderNode && v->renderer.NodeFailed(renderNode, &shaderErr);
    if (brushMode)
        if (auto td = TipDef(brushTarget)) failed = !v->renderer.BrushPipeline(*td, &shaderErr) && !shaderErr.empty();
    // Shader errors: the line of the generated code found back in the HLSL node that holds it
    // (kept for the next frame: the code is drawn above).
    const std::string errKey = failed && !brushMode ? shaderErr + "#" + cr.code : std::string();
    if (errKey != ed.marksKey) {
        ed.marksKey = errKey;
        ed.codeMarks.clear();
        ed.declMarks.clear();
        if (!errKey.empty())
            if (auto cdef = ParseEffect("", cr.code))
                for (auto& [line, msg] : ui::ParseShaderErrors(shaderErr)) {
                    const std::string text = ui::LineOf(cdef->body, line);
                    for (auto& n : g.nodes) {
                        if (n.op != "custom") continue;
                        if (int l = ui::FindCodeLine(n.text, text); l >= 0) { ed.codeMarks[n.id].push_back({l, msg}); break; }
                        if (int l = ui::FindCodeLine(n.decl, text); l >= 0) { ed.declMarks[n.id].push_back({l, msg}); break; }
                    }
                }
    }
    if (!cr.error.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", cr.error.c_str());
    else if (failed) {
        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
        ImGui::TextWrapped("%s", shaderErr.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
    } else ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.55f, 1), "OK");
    if (!brushMode) {
        ImGui::Checkbox("Colonne d'aperçu", &efPv.show);
        ImGui::SameLine();
        if (ImGui::Checkbox("Aperçus sur les nœuds", &ed.nodePreviews) && ed.nodePreviews && g.kind == EGKind::Effect) {
            gNodePreviews = true;
            SpreadNodes(g);
        }
    }
    if (ImGui::Button("Écarter les nœuds")) {
        if (SpreadNodes(g)) { changed = true; changeLabel = "Écarter les nœuds"; }
    }
    ui::HelpTooltip("Pousse vers le bas les nœuds qui se chevauchent (chacun garde sa colonne).");
    if (ImGui::Button("Importer du HLSL...")) hlslImportRequest = true;
    ui::HelpTooltip("Colle ou ouvre du HLSL : un nœud est créé avec ses entrées et sorties\n(annotations //@in //@out, ou signature d'une fonction).");
    DrawHlslImportPopup(&g, ed.ctxPos.x == 0 && ed.ctxPos.y == 0 ? float2{120, 120} : ed.ctxPos, &changed);
    if (ImGui::Button("Voir le HLSL généré")) ImGui::OpenPopup("##hlsl");
    if (ImGui::BeginPopup("##hlsl")) {
        std::string code = cr.code;
        ui::CodeEditor("##gen", &code, ImVec2(820 * dpi, 560 * dpi), true);
        ImGui::EndPopup();
    }
    if (brushMode) {
        if (ImGui::Button("Convertir en HLSL éditable")) {
            brushTarget.tipCode = cr.code;
            brushTarget.tipGraph.clear();
            ImGui::EndChild();
            ImGui::End();
            return;
        }
    } else if (ImGui::Button("Convertir en HLSL éditable")) {
        DocState before = d.S();
        auto old = fx->Def();
        fx->customCode = cr.code;
        fx->graph.clear();
        fx->SyncParams(old);
        d.SyncGraph();
        v->history.PushState("Convertir en HLSL", before, d.S());
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    if (g.kind != EGKind::Effect) {
        // Library: blends -> shaders/effects, brushes -> shaders/brushes (graph kept for editing).
        if (ImGui::Button("Enregistrer dans la bibliothèque...")) ImGui::OpenPopup("##savegraphlib");
        std::string savedId;
        const LibKind lk = g.kind == EGKind::Brush ? LibKind::Brush : LibKind::Blend;
        // The graph rides along (//@graph) so the library item opens again as a graph.
        if (LibrarySavePopup("##savegraphlib", lk, g.name, g.category, "//@graph " + g.ToJson().Dump(0) + "\n" + cr.code, &savedId) && lk == LibKind::Brush) {
            std::vector<float4> keep = brushTarget.tipParams;
            SelectTip(brushTarget, savedId);
            if (keep.size() == brushTarget.tipParams.size()) brushTarget.tipParams = keep;
        }
    }
    if (g.kind == EGKind::Effect && ImGui::Button("Exporter vers un matériau Unreal...")) {
        std::string path = platform::SaveFileDialog(hwnd, L"Script Python Unreal (*.py)\0*.py\0", L"py");
        if (!path.empty()) {
            std::string matName = "M_SX_";
            for (char c : g.name) matName += (isalnum((unsigned char)c) ? c : '_');
            EGUnrealExport ex = ExportEffectGraphToUnreal(g, matName, "/Game/Shaderixel");
            if (!ex.error.empty()) Status("Export impossible : " + ex.error);
            else {
                std::ofstream f(Utf8Path(path), std::ios::binary);
                f << ex.python;
                std::ofstream h(Utf8Path(path + ".hlsl"), std::ios::binary);
                h << ex.customCode;
                Status("Script Unreal exporté : " + path);
            }
        }
    }
    ui::HelpTooltip("Génère un script Python à exécuter dans l'éditeur Unreal :\ncrée un matériau (nœud Custom + paramètres + textures).");
    ImGui::EndChild();
    ImGui::SameLine(0, 0);

    // ---------------------------------------------------------- graph canvas
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    // Preview column on the right (effects and blends; brushes have their pad).
    const bool showPv = efPv.show && !brushMode;
    const float pvW = showPv ? std::clamp(size.x * 0.28f, 240 * dpi, 400 * dpi) : 0.0f;
    size.x -= pvW;
    if (size.x < 20 || size.y < 20) { ImGui::End(); return; }
    ImGui::InvisibleButton("##efcanvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    auto toS = [&](float2 p) { return ImVec2(origin.x + (p.x + ed.pan.x) * ed.zoom, origin.y + (p.y + ed.pan.y) * ed.zoom); };
    auto toG = [&](ImVec2 s) { return float2{(s.x - origin.x) / ed.zoom - ed.pan.x, (s.y - origin.y) / ed.zoom - ed.pan.y}; };

    if (hovered && io.MouseWheel != 0) {
        float2 b = toG(io.MousePos);
        ed.zoom = std::clamp(ed.zoom * std::pow(1.15f, io.MouseWheel), 0.25f, 2.5f);
        float2 a = toG(io.MousePos);
        ed.pan = ed.pan + (a - b);
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4) ||
                                  (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0) && (io.KeyAlt || spaceHeld))))
        ed.pan = ed.pan + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};

    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(17, 18, 22, 255));
    float step = 24.0f * ed.zoom;
    if (step > 6) {
        float ox = std::fmod(ed.pan.x * ed.zoom, step), oy = std::fmod(ed.pan.y * ed.zoom, step);
        for (float x = ox; x < size.x; x += step) dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y), ui::col::Grid);
        for (float y = oy; y < size.y; y += step) dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y), ui::col::Grid);
    }

    auto inPin = [&](const EGNode& n, int i) { return toS(n.op == "reroute" ? n.pos + float2{0, 10} : n.pos + float2{0, kHead + kRowH * (i + 0.5f)}); };
    auto outPin = [&](const EGNode& n, int k) {
        return toS(n.op == "reroute" ? n.pos + float2{kRerouteW, 10} : n.pos + float2{kW, kHead + kRowH * (k + 0.5f)});
    };
    float pr = 5.5f * ed.zoom;
    ImVec2 m = io.MousePos;
    auto hit = [&](ImVec2 a, float r) { float dx = a.x - m.x, dy = a.y - m.y; return dx * dx + dy * dy <= r * r; };
    auto openAdd = [&](Id fromNode, int fromOut, Id toNode, int toIn) {
        ed.ctxPos = toG(m);
        ed.search[0] = 0;
        ed.ctxFrom = fromNode;
        ed.ctxFromOut = fromOut;
        ed.ctxTo = toNode;
        ed.ctxToIn = toIn;
        ImGui::OpenPopup("##efadd");
    };

    Id hNode = 0, hOutNode = 0, hInNode = 0;
    int hOut = -1, hIn = -1;
    if (hovered) {
        for (int k = (int)g.nodes.size() - 1; k >= 0 && !hNode && !hOutNode && !hInNode; --k) {
            const EGNode& n = g.nodes[k];
            if (!FindOp(n.op)) continue;
            const EGOpDef eff = EffectiveOp(n);
            const EGOpDef* od = &eff;
            const float pinR = n.op == "reroute" ? 7.0f * ed.zoom : pr * 2.2f;  // the middle of a reroute drags it
            for (int o = 0; o < (int)od->outputs.size(); ++o) if (hit(outPin(n, o), pinR)) { hOutNode = n.id; hOut = o; }
            for (int i = 0; i < (int)od->inputs.size() && !hOutNode; ++i) if (hit(inPin(n, i), pinR)) { hInNode = n.id; hIn = i; }
            ImVec2 a = toS(n.pos), b = toS(n.pos + float2{NodeW(n), NodeH(n, *od)});
            if (!hOutNode && !hInNode && m.x >= a.x && m.x <= b.x && m.y >= a.y && m.y <= b.y) hNode = n.id;
        }
    }
    // Comments: the title bar drags (with the nodes inside), the corner resizes; the body lets clicks through.
    Id hComment = 0;
    int hCommentPart = 0;  // 1 title, 2 resize corner
    if (hovered && !hNode && !hInNode && !hOutNode)
        for (int k = (int)g.comments.size() - 1; k >= 0 && !hComment; --k) {
            const EGComment& c = g.comments[k];
            const ImVec2 a = toS(c.pos), b = toS(c.pos + c.size);
            if (m.x >= b.x - 14 && m.x <= b.x + 2 && m.y >= b.y - 14 && m.y <= b.y + 2) hCommentPart = 2;
            else if (m.x >= a.x && m.x <= b.x && m.y >= a.y && m.y <= a.y + kCommentTitle * ed.zoom) hCommentPart = 1;
            if (hCommentPart) hComment = c.id;
        }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !spaceHeld) {
        if (hOutNode) {
            ed.dragLink = true;
            ed.linkFrom = hOutNode;
            ed.linkFromOut = hOut;
            ed.linkTo = 0;
        } else if (hInNode) {
            EGNode* n = g.Find(hInNode);
            if (n && hIn < (int)n->inputs.size() && n->inputs[hIn].node) {
                ed.dragLink = true;
                ed.linkFrom = n->inputs[hIn].node;
                ed.linkFromOut = n->inputs[hIn].output;
                n->inputs[hIn] = {};
                changed = true;
                changeLabel = "Délier";
            } else {
                ed.dragLink = true;
                ed.linkFrom = 0;
                ed.linkTo = hInNode;
                ed.linkToIn = hIn;
            }
        } else if (hNode) {
            // Ctrl / Maj: add to the selection (Ctrl on a selected node: removes it).
            if (io.KeyCtrl || io.KeyShift) {
                if (io.KeyCtrl && ed.multi.count(hNode)) {
                    ed.multi.erase(hNode);
                    if (ed.selected == hNode) ed.selected = ed.multi.empty() ? 0 : *ed.multi.begin();
                } else {
                    ed.multi.insert(hNode);
                    ed.selected = hNode;
                }
            } else {
                if (!ed.multi.count(hNode)) ed.multi = {hNode};
                ed.selected = hNode;
            }
            ed.dragNode = hNode;
            ed.moved = false;
        } else if (hComment) {
            ed.selComment = hComment;
            ed.selected = 0;
            ed.multi.clear();
            ed.moved = false;
            if (hCommentPart == 2) {
                ed.sizeComment = hComment;
            } else {
                // The nodes lying inside move with it.
                ed.dragComment = hComment;
                ed.commentNodes.clear();
                for (auto& c : g.comments)
                    if (c.id == hComment)
                        for (auto& n : g.nodes) {
                            const float2 e = n.pos + float2{NodeW(n), NodeH(n, EffectiveOp(n))};
                            if (n.pos.x >= c.pos.x && n.pos.y >= c.pos.y && e.x <= c.pos.x + c.size.x && e.y <= c.pos.y + c.size.y) ed.commentNodes.push_back(n.id);
                        }
            }
        } else {
            // Empty spot: box selection (Ctrl / Maj: added to the selection).
            ed.selComment = 0;
            if (!io.KeyCtrl && !io.KeyShift) {
                ed.selected = 0;
                ed.multi.clear();
            }
            ed.boxing = true;
            ed.boxStart = m;
        }
    }
    if ((ed.dragComment || ed.sizeComment) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
        const float2 dmove{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
        for (auto& c : g.comments) {
            if (c.id == ed.dragComment) {
                c.pos = c.pos + dmove;
                for (Id id : ed.commentNodes) if (EGNode* n = g.Find(id)) n->pos = n->pos + dmove;
            } else if (c.id == ed.sizeComment) {
                c.size = {std::max(80.0f, c.size.x + dmove.x), std::max(60.0f, c.size.y + dmove.y)};
            }
        }
        ed.moved = true;
        live = true;
    }
    if (ed.dragNode && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
        if (EGNode* n = g.Find(ed.dragNode)) {
            const float2 dmove{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
            n->pos = n->pos + dmove;
            if (ed.multi.count(n->id))  // the whole selection moves
                for (auto& o : g.nodes)
                    if (o.id != n->id && ed.multi.count(o.id)) o.pos = o.pos + dmove;
            ed.moved = true;
            live = true;
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (ed.dragLink) {
            Id from = 0, to = 0;
            int fo = 0, ti = 0;
            if (ed.linkFrom && hInNode) { from = ed.linkFrom; fo = ed.linkFromOut; to = hInNode; ti = hIn; }
            else if (ed.linkTo && hOutNode) { from = hOutNode; fo = hOut; to = ed.linkTo; ti = ed.linkToIn; }
            if (from && to && from != to && !g.WouldCycle(from, to)) {
                if (EGNode* n = g.Find(to)) {
                    if ((int)n->inputs.size() <= ti) n->inputs.resize(ti + 1);
                    n->inputs[ti] = {from, fo};
                    changed = true;
                    changeLabel = "Relier";
                }
            } else if (!from && !to && hovered && !hNode && !hInNode && !hOutNode) {
                // Dropped on the empty canvas: the add menu, for this link.
                openAdd(ed.linkFrom, ed.linkFromOut, ed.linkFrom ? 0 : ed.linkTo, ed.linkToIn);
            }
            ed.dragLink = false;
        }
        if (ed.dragNode) {
            if (ed.moved) { changed = true; changeLabel = ed.multi.size() > 1 ? "Déplacer des nœuds" : "Déplacer un nœud"; }
            ed.dragNode = 0;
        }
        if (ed.dragComment || ed.sizeComment) {
            if (ed.moved) { changed = true; changeLabel = ed.sizeComment ? "Redimensionner un commentaire" : "Déplacer un commentaire"; }
            ed.dragComment = ed.sizeComment = 0;
        }
        if (ed.boxing) {
            ed.boxing = false;
            const ImVec2 a(std::min(ed.boxStart.x, m.x), std::min(ed.boxStart.y, m.y)), b(std::max(ed.boxStart.x, m.x), std::max(ed.boxStart.y, m.y));
            if (b.x - a.x > 3 || b.y - a.y > 3)
                for (auto& n : g.nodes) {
                    if (!FindOp(n.op)) continue;
                    const EGOpDef eff = EffectiveOp(n);
                    const ImVec2 na = toS(n.pos), nb = toS(n.pos + float2{NodeW(n), NodeH(n, eff)});
                    if (nb.x >= a.x && na.x <= b.x && nb.y >= a.y && na.y <= b.y) {
                        ed.multi.insert(n.id);
                        if (!ed.selected) ed.selected = n.id;
                    }
                }
        }
    }

    // Typed pins and links: scalar gray, vector 2 green, RGB yellow, RGBA pink (Unreal colors).
    auto typeColor = [](int t, int alpha = 255) {
        switch (t) {
            case 1: return IM_COL32(170, 172, 180, alpha);
            case 2: return IM_COL32(110, 210, 120, alpha);
            case 3: return IM_COL32(235, 200, 80, alpha);
            case 4: return IM_COL32(230, 120, 210, alpha);
            case kEGTexture: return IM_COL32(90, 170, 240, alpha);  // texture (custom node pins)
            default: return IM_COL32(150, 150, 170, alpha);
        }
    };
    static const char* kTypeNames[] = {"", "x", "xy", "RGB", "RGBA"};
    // Comment frames, behind the links and the nodes.
    for (auto& c : g.comments) {
        const ImVec2 a = toS(c.pos), b = toS(c.pos + c.size);
        auto col = [&](float alpha) { return ImGui::GetColorU32(ImVec4(c.color.x, c.color.y, c.color.z, alpha)); };
        const float rr = 6 * ed.zoom, th = kCommentTitle * ed.zoom;
        dl->AddRectFilled(a, b, col(0.14f), rr);
        dl->AddRectFilled(a, ImVec2(b.x, a.y + th), col(0.7f), rr, ImDrawFlags_RoundCornersTop);
        const bool csel = c.id == ed.selComment;
        dl->AddRect(a, b, csel ? ui::col::Selection : col(0.8f), rr, 0, csel ? 2.5f : 1.0f);
        dl->AddTriangleFilled(ImVec2(b.x - 12, b.y - 2), ImVec2(b.x - 2, b.y - 2), ImVec2(b.x - 2, b.y - 12), col(0.9f));
        if (ed.zoom > 0.3f) {
            const float cfs = ImGui::GetFontSize() * std::max(0.8f, ed.zoom * 1.05f);
            dl->PushClipRect(a, ImVec2(b.x - 4, a.y + th), true);
            dl->AddText(ImGui::GetFont(), cfs, ImVec2(a.x + 8 * ed.zoom, a.y + (th - cfs) * 0.5f), IM_COL32_WHITE, c.text.c_str());
            dl->PopClipRect();
        }
    }

    // Links
    auto link = [&](ImVec2 a, ImVec2 b, ImU32 col) {
        float dx = std::max(std::abs(b.x - a.x) * 0.5f, 30.0f * ed.zoom);
        dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, col, 2.0f * ed.zoom);
    };
    for (auto& n : g.nodes)
        for (int i = 0; i < (int)n.inputs.size(); ++i) {
            const EGNode* s = n.inputs[i].node ? g.Find(n.inputs[i].node) : nullptr;
            if (s) link(outPin(*s, n.inputs[i].output), inPin(n, i), (s->id == ed.selected || n.id == ed.selected) ? ui::col::Selection : typeColor(crTypes.OutType(s->id, n.inputs[i].output), 200));
        }
    if (ed.dragLink) {
        if (ed.linkFrom) { if (const EGNode* s = g.Find(ed.linkFrom)) link(outPin(*s, ed.linkFromOut), m, ui::col::Accent); }
        else if (const EGNode* t = g.Find(ed.linkTo)) link(m, inPin(*t, ed.linkToIn), ui::col::Accent);
    }

    // Result of a node on the simulated inputs: the graph with the output wired to it (rebuilt after a pause in the edits,
    // two per frame at most, compiled in the background).
    int pvBuilds = 2;
    const bool pvSettled = ImGui::GetTime() - ed.lastEdit > 0.3;
    auto nodePreview = [&](const EGNode& n) -> gpu::Texture* {
        auto& ng = efPv.nodeGraphs[n.id];
        const std::string key = ed.cachedJson + "#" + std::to_string(n.id);
        if (ng.first != key && pvSettled && pvBuilds > 0) {
            --pvBuilds;
            ng = {key, PreviewGraphFor(g, n.id, crTypes.OutType(n.id, 0))};

        }
        if (ng.second.empty()) return nullptr;
        EffectInstance inst = *fx;
        inst.graph = ng.second;
        inst.effectId.clear();
        inst.customCode.clear();
        inst.SyncParams(fx->Def());
        return RenderEffectPreview(*v, inst, g.inputs, 0x100000000ull + n.id);
    };

    // Nodes
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * ed.zoom;
    bool text = ed.zoom > 0.45f;
    std::set<Id> errNodes(cr.errorNodes.begin(), cr.errorNodes.end());
    for (auto& [id, mk] : ed.codeMarks) errNodes.insert(id);
    for (auto& [id, mk] : ed.declMarks) errNodes.insert(id);
    for (auto& n : g.nodes) {
        if (!FindOp(n.op)) continue;
        const EGOpDef eff = EffectiveOp(n);
        const EGOpDef* od = &eff;
        float h = NodeH(n, *od);
        ImVec2 a = toS(n.pos), b = toS(n.pos + float2{NodeW(n), h});
        if (n.op == "reroute") {
            // A dot on the link.
            const int t = crTypes.OutType(n.id, 0);
            const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            dl->AddLine(inPin(n, 0), outPin(n, 0), typeColor(t, 220), 2.0f * ed.zoom);
            dl->AddCircleFilled(c, 6.5f * ed.zoom, typeColor(t));
            if (n.id == ed.selected || ed.multi.count(n.id)) dl->AddCircle(c, 10.0f * ed.zoom, ui::col::Selection, 0, 2.0f);
            continue;
        }
        float r = 5 * ed.zoom;
        dl->AddRectFilled(ImVec2(a.x + 3, a.y + 4), ImVec2(b.x + 3, b.y + 4), IM_COL32(0, 0, 0, 70), r);
        dl->AddRectFilled(a, b, ui::col::PanelLight, r);
        dl->AddRectFilled(a, ImVec2(b.x, a.y + kHead * ed.zoom), CategoryColor(od->category), r, ImDrawFlags_RoundCornersTop);
        bool selected = n.id == ed.selected || ed.multi.count(n.id);
        if (selected) dl->AddRect(ImVec2(a.x - 3, a.y - 3), ImVec2(b.x + 3, b.y + 3), IM_COL32(255, 140, 0, 90), r + 3, 0, 4.0f);
        dl->AddRect(a, b, errNodes.count(n.id) ? ui::col::Error : selected ? ui::col::Selection : ui::col::Border, r, 0, selected ? 3.0f : 1.0f);
        if (text) {
            std::string title = (n.op == "param" || n.op == "paramcolor" || n.op == "param2" || n.op == "custom") && !n.name.empty()
                                    ? (n.op == "custom" ? "HLSL : " + n.name : !n.text.empty() ? n.text : n.name)
                                    : std::string(od->label);
            dl->PushClipRect(a, ImVec2(b.x - 4, a.y + kHead * ed.zoom), true);
            dl->AddText(font, fs, ImVec2(a.x + 7 * ed.zoom, a.y + (kHead * ed.zoom - fs) * 0.5f), IM_COL32_WHITE, title.c_str());
            dl->PopClipRect();
        }
        for (int i = 0; i < (int)od->inputs.size(); ++i) {
            ImVec2 p = inPin(n, i);
            bool conn = i < (int)n.inputs.size() && n.inputs[i].node;
            int t = conn ? crTypes.OutType(n.inputs[i].node, n.inputs[i].output) : 0;
            int declared = n.op == "custom" && i < (int)n.pins.size() ? (n.pins[i].type == kEGTexture ? kEGTexture : PinValueSize(n.pins[i].type)) : 0;
            if (conn) dl->AddCircleFilled(p, pr, typeColor(t));
            else dl->AddCircle(p, pr, declared ? typeColor(declared) : IM_COL32(120, 122, 140, 255), 0, 1.5f * ed.zoom);
            if (text) {
                std::string lbl = od->inputs[i].name;
                const bool setHere = !conn && od->inputs[i].defValue >= 0 && n.PinValueSet(i);
                if (!conn && od->inputs[i].defValue >= 0) {  // negative: implicit source (UV, opacity...)
                    char b2[32];
                    snprintf(b2, sizeof(b2), " (%.3g)", n.PinValue(i, od->inputs[i].defValue));
                    lbl += b2;
                }
                dl->AddText(font, fs * 0.88f, ImVec2(p.x + 9 * ed.zoom, p.y - fs * 0.45f), setHere ? IM_COL32(150, 200, 255, 255) : ui::col::TextDim, lbl.c_str());
            }
        }
        for (int o = 0; o < (int)od->outputs.size(); ++o) {
            ImVec2 p = outPin(n, o);
            int t = crTypes.OutType(n.id, o);
            dl->AddCircleFilled(p, pr, typeColor(t));
            // Label: the output name, else its type.
            const char* lbl = od->outputs[o][0] ? od->outputs[o] : (t >= 1 && t <= 4 ? kTypeNames[t] : t == kEGTexture ? "Texture" : "");
            if (text && lbl[0]) {
                ImVec2 ts = font->CalcTextSizeA(fs * 0.88f, FLT_MAX, 0, lbl);
                dl->AddText(font, fs * 0.88f, ImVec2(p.x - 9 * ed.zoom - ts.x, p.y - fs * 0.45f), typeColor(t, 230), lbl);
            }
        }
        std::string vt = ValueText(n);
        if (text && !vt.empty()) {
            ImVec2 tp(a.x + 8 * ed.zoom, a.y + (kHead + Rows(*od) * kRowH + 2) * ed.zoom);
            if (n.op == "color" || n.op == "paramcolor") {
                ImU32 c = ImGui::ColorConvertFloat4ToU32(ImVec4(n.value.x, n.value.y, n.value.z, 1));
                dl->AddRectFilled(tp, ImVec2(tp.x + 40 * ed.zoom, tp.y + fs), c, 3);
                tp.x += 46 * ed.zoom;
            }
            dl->AddText(font, fs * 0.9f, tp, ui::col::Text, vt.c_str());
        }
        if (HasNodePreview(n) && crTypes.OutType(n.id, 0) != kEGTexture) {
            const float pw = kPvH * ed.zoom;
            const ImVec2 pa(a.x + (NodeW(n) * ed.zoom - pw) * 0.5f, b.y - pw - 6 * ed.zoom), pb(pa.x + pw, pa.y + pw);
            dl->AddRectFilled(pa, pb, IM_COL32(40, 40, 46, 255));
            if (gpu::Texture* t = nodePreview(n)) {
                if (gpu::Texture* shown = v->renderer.Thumbnail(*ctx, 0x7E5F100000000000ull + n.id, t, efPv.slots[0x100000000ull + n.id].hash ^ (u64)pw,
                                                                (u32)std::max(1.0f, pw), (u32)std::max(1.0f, pw)))
                    dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(shown), pa, pb);
            }
            dl->AddRect(pa, pb, ui::col::Border);
        }
    }

    auto addAt = [&](const EGOpDef& od, float2 at) {
        EGNode n;
        n.id = g.nextId++;
        n.op = od.op;
        n.pos = at;
        n.inputs.resize(od.inputs.size());
        if (n.op == "param") { n.name = "Valeur"; n.value = {0.5f, 0, 0, 0}; }
        if (n.op == "paramswitch") { n.name = "Activer"; n.value = {1, 0, 0, 0}; }
        if (n.op == "colorramp") n.text = "0 0.02 0.02 0.1 1;0.5 0.9 0.35 0.1 1;1 1 0.95 0.7 1";
        if (n.op == "param2") { n.name = "Position"; n.value = {0.5f, 0.5f, 0, 0}; }
        if (n.op == "paramcolor") { n.name = "Couleur"; n.value = {1, 0.5f, 0.2f, 1}; }
        if (n.op == "color") n.value = {1, 1, 1, 1};
        if (n.op == "const") n.value = {1, 0, 0, 0};
        if (n.op == "custom") n.text = "    return A * B;";
        if (n.op == "mask") n.index = 7;
        g.nodes.push_back(n);
        ed.selected = n.id;
        changed = true;
        changeLabel = std::string("Ajouter ") + od.label;
    };
    // Node palette: drop on the canvas, or click (center of the view).
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) paletteFocus = 1;
    if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType(kPalettePayload) && ((const char*)pl->Data)[0] == '1' &&
        ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + size.x, origin.y + size.y))) {
        if (ImGui::BeginDragDropTargetCustom(ImRect(origin, ImVec2(origin.x + size.x, origin.y + size.y)), ImGui::GetID("##efpaldrop"))) {
            if (const ImGuiPayload* got = ImGui::AcceptDragDropPayload(kPalettePayload)) {
                paletteAdd.pending = true;
                paletteAdd.target = 1;
                paletteAdd.key = (const char*)got->Data + 1;
                paletteAdd.hasPos = true;
                paletteAdd.screenPos = io.MousePos;
            }
            ImGui::EndDragDropTarget();
        }
    }
    if (paletteAdd.pending && paletteAdd.target == 1) {
        paletteAdd.pending = false;
        for (auto& od : EffectGraphOps())
            if (paletteAdd.key == od.op && OpAllowed(od, g.kind)) {
                addAt(od, paletteAdd.hasPos ? toG(paletteAdd.screenPos) : toG(ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f)));
                break;
            }
    }
    if (hovered && !hNode && !hInNode && !hOutNode && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (hComment && hCommentPart == 1) {
            for (auto& c : g.comments)
                if (c.id == hComment) {
                    ed.renameComment = c.id;
                    ed.commentText = c.text;
                    ImGui::OpenPopup("##cmtname");
                }
        } else {
            // On a link: a reroute node inserted there.
            Id target = 0;
            int targetIn = 0;
            for (auto& n : g.nodes) {
                for (int i = 0; i < (int)n.inputs.size() && !target; ++i) {
                    const EGNode* s = n.inputs[i].node ? g.Find(n.inputs[i].node) : nullptr;
                    if (!s) continue;
                    const ImVec2 la = outPin(*s, n.inputs[i].output), lb = inPin(n, i);
                    const float dx = std::max(std::abs(lb.x - la.x) * 0.5f, 30.0f * ed.zoom);
                    for (int k = 0; k <= 32; ++k) {
                        const ImVec2 q = ImBezierCubicCalc(la, ImVec2(la.x + dx, la.y), ImVec2(lb.x - dx, lb.y), lb, k / 32.0f);
                        if ((q.x - m.x) * (q.x - m.x) + (q.y - m.y) * (q.y - m.y) < 49.0f) { target = n.id; targetIn = i; break; }
                    }
                }
                if (target) break;
            }
            if (EGNode* t = target ? g.Find(target) : nullptr) {
                EGNode r;
                r.id = g.nextId++;
                r.op = "reroute";
                r.pos = toG(m) - float2{kRerouteW * 0.5f, 10};
                r.inputs = {t->inputs[targetIn]};
                t->inputs[targetIn] = {r.id, 0};
                g.nodes.push_back(r);
                ed.selected = r.id;
                ed.multi = {r.id};
                ed.boxing = false;
                changed = true;
                changeLabel = "Reroutage";
            }
        }
    }
    if (ImGui::BeginPopup("##cmtname")) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(260 * dpi);
        if (ImGui::InputText("##t", &ed.commentText, ImGuiInputTextFlags_EnterReturnsTrue)) {
            for (auto& c : g.comments)
                if (c.id == ed.renameComment) c.text = ed.commentText;
            changed = true;
            changeLabel = "Renommer un commentaire";
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##cmtctx")) {
        for (auto it = g.comments.begin(); it != g.comments.end(); ++it) {
            if (it->id != ed.selComment) continue;
            if (ImGui::MenuItem("Renommer")) {
                ed.renameComment = it->id;
                ed.commentText = it->text;
                ImGui::CloseCurrentPopup();
                ed.demoAdd = -1;  // opens the rename popup below (outside this popup)
            }
            float col[3] = {it->color.x, it->color.y, it->color.z};
            if (ImGui::ColorEdit3("Couleur", col, ImGuiColorEditFlags_NoInputs)) {
                it->color = {col[0], col[1], col[2], 1};
                live = true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) { changed = true; changeLabel = "Couleur d'un commentaire"; }
            static const float4 presets[] = {{0.30f, 0.42f, 0.70f, 1}, {0.25f, 0.55f, 0.35f, 1}, {0.70f, 0.45f, 0.20f, 1},
                                             {0.65f, 0.25f, 0.30f, 1}, {0.50f, 0.32f, 0.65f, 1}, {0.40f, 0.40f, 0.45f, 1}};
            for (int k = 0; k < 6; ++k) {
                if (k) ImGui::SameLine();
                ImGui::PushID(k);
                if (ImGui::ColorButton("##p", ImVec4(presets[k].x, presets[k].y, presets[k].z, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(18 * dpi, 18 * dpi))) {
                    it->color = presets[k];
                    changed = true;
                    changeLabel = "Couleur d'un commentaire";
                }
                ImGui::PopID();
            }
            if (ImGui::MenuItem("Supprimer le commentaire")) {
                g.comments.erase(it);
                ed.selComment = 0;
                changed = true;
                changeLabel = "Supprimer un commentaire";
            }
            break;
        }
        ImGui::EndPopup();
    }
    if (ed.demoAdd == -1) {
        ed.demoAdd = 0;
        ImGui::OpenPopup("##cmtname");
    }

    // Double click on a function node: opened.
    if (hovered && hNode && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        if (const EGNode* fnode = g.Find(hNode); fnode && fnode->op == "function") ed.enterFunc = fnode->id;
    // Double click on the label of an unconnected input: its value, edited in place.
    if (hovered && hNode && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        if (const EGNode* dn = g.Find(hNode); dn && dn->op != "custom" && FindOp(dn->op)) {
            const EGOpDef de = EffectiveOp(*dn);
            for (int i = 0; i < (int)de.inputs.size(); ++i) {
                const ImVec2 pp = inPin(*dn, i);
                const bool linked = i < (int)dn->inputs.size() && dn->inputs[i].node;
                if (!linked && de.inputs[i].defValue >= 0 && m.x > pp.x && m.x < pp.x + kW * 0.6f * ed.zoom && std::abs(m.y - pp.y) < kRowH * 0.5f * ed.zoom) {
                    ed.pinEditNode = dn->id;
                    ed.pinEditIn = i;
                    ed.dragNode = 0;
                    ImGui::OpenPopup("##pinval");
                }
            }
        }
    if (ImGui::BeginPopup("##pinval")) {
        EGNode* pn = g.Find(ed.pinEditNode);
        const EGOpDef* pod = pn ? FindOp(pn->op) : nullptr;
        if (!pn || !pod || ed.pinEditIn >= (int)EffectiveOp(*pn).inputs.size()) {
            ImGui::CloseCurrentPopup();
        } else {
            const EGOpDef pe = EffectiveOp(*pn);
            const size_t i = (size_t)ed.pinEditIn;
            ImGui::TextDisabled("%s", pe.inputs[i].name[0] ? pe.inputs[i].name : "Entrée");
            float val = pn->PinValue(i, pe.inputs[i].defValue);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(140 * dpi);
            if (ImGui::DragFloat("##v", &val, 0.01f)) {
                if (pn->pinValues.size() <= i) pn->pinValues.resize(i + 1, std::numeric_limits<float>::quiet_NaN());
                pn->pinValues[i] = val;
                live = true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) { changed = true; changeLabel = "Valeur d'entrée"; }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Add-node menu (right click, or a link dropped on the empty canvas). Categories as a tree like Unreal,
    // a flat list while searching (Enter: the first one). From a link: only the nodes that can take it,
    // linked on creation.
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 16.0f) {
        if (hComment) {
            ed.selComment = hComment;
            ImGui::OpenPopup("##cmtctx");
        } else {
            openAdd(0, 0, 0, 0);
        }
    }
    if (ed.demoAdd) {
        Id src = 0;
        for (auto& n : g.nodes) if (n.op == "uv" && !src) src = n.id;
        m = ImVec2(origin.x + size.x * 0.45f, origin.y + size.y * 0.3f);
        openAdd(ed.demoAdd == 2 ? src : 0, 0, 0, 0);
        if (ed.demoAdd == 3) snprintf(ed.search, sizeof(ed.search), "%s", "inter");
        ed.demoAdd = 0;
    }
    if (ImGui::IsPopupOpen("##efadd") && (ed.ctxFrom || ed.ctxTo)) {
        const ImVec2 to = toS(ed.ctxPos);
        if (ed.ctxFrom) { if (const EGNode* s = g.Find(ed.ctxFrom)) link(outPin(*s, ed.ctxFromOut), to, ui::col::Accent); }
        else if (const EGNode* t = g.Find(ed.ctxTo)) link(to, inPin(*t, ed.ctxToIn), ui::col::Accent);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(320 * dpi, 0), ImVec2(460 * dpi, 620 * dpi));
    ImGui::SetNextWindowPos(toS(ed.ctxPos), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##efadd")) {
        const bool fromOut = ed.ctxFrom != 0, toIn = !fromOut && ed.ctxTo != 0;
        int linkType = 0;  // components of the dragged output (5 = texture)
        if (fromOut || toIn) {
            const EGNode* cn = g.Find(fromOut ? ed.ctxFrom : ed.ctxTo);
            std::string pinName, nodeName;
            if (cn && FindOp(cn->op)) {
                const EGOpDef ce = EffectiveOp(*cn);
                nodeName = cn->op == "custom" && !cn->name.empty() ? cn->name : ce.label;
                if (fromOut && ed.ctxFromOut < (int)ce.outputs.size()) pinName = ce.outputs[ed.ctxFromOut];
                if (toIn && ed.ctxToIn < (int)ce.inputs.size()) pinName = ce.inputs[ed.ctxToIn].name;
            }
            if (fromOut) linkType = crTypes.OutType(ed.ctxFrom, ed.ctxFromOut);
            if (pinName.empty() && fromOut) pinName = linkType >= 1 && linkType <= 4 ? kTypeNames[linkType] : linkType == kEGTexture ? "Texture" : "";
            ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), fromOut ? "Depuis la sortie %s de « %s »" : "Vers l'entrée %s de « %s »", pinName.c_str(), nodeName.c_str());
            ImGui::Checkbox("Contextuel", &ed.ctxSensitive);
            ui::HelpTooltip("Seulement les nœuds qui peuvent être reliés à ce lien ; le nouveau nœud est relié.");
        }
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool enter = ImGui::InputTextWithHint("##s", "Rechercher un nœud...", ed.search, sizeof(ed.search), ImGuiInputTextFlags_EnterReturnsTrue);
        const std::string q = ed.search;
        auto fits = [&](const EGOpDef& od) {
            const std::string op = od.op;
            if (op == "output" || !OpAllowed(od, g.kind)) return false;
            const std::string cat = od.category;
            if (cat == "Fonction (appel)") return false;              // inserted from the function library
            if (cat == "Fonction" && ed.funcPath.empty()) return false;  // inputs / outputs: inside a function
            if (!ed.ctxSensitive) return true;
            if (fromOut) {
                if (od.inputs.empty() && op != "custom") return false;
                // A texture goes into a custom node (texture pin); the other nodes take values.
                if (linkType == kEGTexture) return op == "custom";
            }
            if (toIn && od.outputs.empty()) return false;
            return true;
        };
        auto matches = [&](const EGOpDef& od) {
            return q.empty() || ui::FilterMatch(q, od.label) || ui::FilterMatch(q, od.op) || ui::FilterMatch(q, od.category) ||
                   ui::FilterMatch(q, OpDescription(od.op));
        };
        const EGOpDef* chosen = nullptr;
        auto item = [&](const EGOpDef& od, bool highlight) {
            ImGui::PushID(od.op);
            if (ImGui::Selectable(od.label, highlight)) chosen = &od;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(od.label);
                if (const char* desc = OpDescription(od.op); desc[0]) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24);
                    ImGui::TextDisabled("%s", desc);
                    ImGui::PopTextWrapPos();
                }
                std::string ins, outs;
                for (auto& p : od.inputs) ins += (ins.empty() ? "" : ", ") + std::string(p.name);
                for (auto* o : od.outputs) outs += (outs.empty() ? "" : ", ") + std::string(o[0] ? o : "valeur");
                if (!ins.empty()) ImGui::Text("Entrées : %s", ins.c_str());
                if (!outs.empty()) ImGui::Text("Sorties : %s", outs.c_str());
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        };
        if (!fromOut && !toIn && q.empty() && ImGui::Selectable("Ajouter un commentaire (C)")) {
            EGComment c;
            c.id = g.nextId++;
            c.pos = ed.ctxPos;
            g.comments.push_back(c);
            ed.selComment = c.id;
            changed = true;
            changeLabel = "Ajouter un commentaire";
            ImGui::CloseCurrentPopup();
        }
        ImGui::BeginChild("##addlist", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
        if (!q.empty()) {
            // Flat list: the first one is taken by Enter.
            bool first = true;
            for (auto& od : EffectGraphOps()) {
                if (!fits(od) || !matches(od)) continue;
                if (first && enter) chosen = &od;
                item(od, first);
                ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(od.category).x);
                ImGui::TextDisabled("%s", od.category);
                first = false;
            }
            if (first) ImGui::TextDisabled("Aucun nœud ne correspond.");
        } else {
            // Tree: category, then sub-group (Maths), then the nodes. Opened by default from a link (short lists).
            std::vector<std::string> cats;
            for (auto& od : EffectGraphOps())
                if (fits(od) && std::find(cats.begin(), cats.end(), od.category) == cats.end()) cats.push_back(od.category);
            for (const std::string& cat : cats) {
                ImVec4 cc = ImGui::ColorConvertU32ToFloat4(CategoryColor(cat.c_str()));
                ImGui::PushStyleColor(ImGuiCol_Text, ImLerp(cc, ImVec4(1, 1, 1, 1), 0.35f));
                bool open = ImGui::TreeNodeEx(cat.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ((fromOut || toIn) ? ImGuiTreeNodeFlags_DefaultOpen : 0));
                ImGui::PopStyleColor();
                if (!open) continue;
                std::vector<std::string> subs;
                for (auto& od : EffectGraphOps())
                    if (fits(od) && od.category == cat && std::find(subs.begin(), subs.end(), OpSubGroup(od.op)) == subs.end()) subs.push_back(OpSubGroup(od.op));
                for (const std::string& sub : subs) {
                    bool subOpen = true;
                    if (!sub.empty()) subOpen = ImGui::TreeNodeEx(sub.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen);
                    if (!subOpen) continue;
                    for (auto& od : EffectGraphOps())
                        if (fits(od) && od.category == cat && OpSubGroup(od.op) == sub) {
                            ImGui::Indent(ImGui::GetStyle().IndentSpacing * 0.4f);
                            item(od, false);
                            ImGui::Unindent(ImGui::GetStyle().IndentSpacing * 0.4f);
                        }
                    if (!sub.empty()) ImGui::TreePop();
                }
                ImGui::TreePop();
            }
        }
        // Saved functions (reusable in any effect).
        {
            const auto lib = LibraryFunctions();
            bool any = false;
            for (auto& [fname, fjson] : lib) any |= q.empty() || ui::FilterMatch(q, fname);
            if (any && ImGui::TreeNodeEx("Fonctions de la bibliothèque", ImGuiTreeNodeFlags_SpanAvailWidth | (q.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen))) {
                for (auto& [fname, fjson] : lib) {
                    if (!q.empty() && !ui::FilterMatch(q, fname)) continue;
                    if (ImGui::Selectable((fname + "##libfn").c_str())) {
                        EffectGraph inner;
                        if (EffectGraph::Deserialize(fjson, inner)) {
                            EGNode fn;
                            fn.id = g.nextId++;
                            fn.op = "function";
                            fn.name = fname;
                            fn.pos = ed.ctxPos;
                            fn.text = fjson;
                            SyncFunctionInterface(fn, inner);
                            g.nodes.push_back(fn);
                            ed.selected = fn.id;
                            ed.multi = {fn.id};
                            changed = true;
                            changeLabel = "Ajouter une fonction";
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();
        if (chosen) {
            addAt(*chosen, ed.ctxPos);
            const Id nid = g.nodes.back().id;
            // Linked to the dragged pin.
            if (fromOut && g.Find(ed.ctxFrom)) {
                EGNode& nn = g.nodes.back();
                const EGOpDef ne = EffectiveOp(nn);
                if (!ne.inputs.empty()) {
                    if (nn.inputs.size() < ne.inputs.size()) nn.inputs.resize(ne.inputs.size());
                    nn.inputs[0] = {ed.ctxFrom, ed.ctxFromOut};
                }
            } else if (toIn) {
                if (EGNode* t = g.Find(ed.ctxTo)) {
                    if ((int)t->inputs.size() <= ed.ctxToIn) t->inputs.resize(ed.ctxToIn + 1);
                    t->inputs[ed.ctxToIn] = {nid, 0};
                }
            }
            ed.ctxFrom = ed.ctxTo = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        ed.ctxFrom = ed.ctxTo = 0;
    }
    // Box being drawn.
    if (ed.boxing) {
        const ImVec2 a(std::min(ed.boxStart.x, m.x), std::min(ed.boxStart.y, m.y)), b(std::max(ed.boxStart.x, m.x), std::max(ed.boxStart.y, m.y));
        dl->AddRectFilled(a, b, IM_COL32(120, 140, 255, 30));
        dl->AddRect(a, b, IM_COL32(120, 140, 255, 180));
    }
    if (ed.selected && !ed.multi.count(ed.selected)) ed.multi = {ed.selected};  // selected elsewhere (library, demos)
    for (auto it = ed.multi.begin(); it != ed.multi.end();) it = g.Find(*it) ? std::next(it) : ed.multi.erase(it);
    const bool keys = focused && !io.WantTextInput && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    const bool nodesSelected = !ed.multi.empty();
    if (keys && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && !ed.multi.empty()) {
        std::set<Id> gone;
        for (Id id : ed.multi)
            if (const EGNode* n = g.Find(id); n && n->op != "output") gone.insert(id);
        if (!gone.empty()) {
            g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const EGNode& x) { return gone.count(x.id) != 0; }), g.nodes.end());
            for (auto& x : g.nodes) for (auto& l : x.inputs) if (gone.count(l.node)) l = {};
            ed.multi.clear();
            ed.selected = 0;
            changed = true;
            changeLabel = gone.size() > 1 ? "Supprimer des nœuds" : "Supprimer un nœud";
        }
    }
    if (keys && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        EGComment c;
        c.id = g.nextId++;
        float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
        for (auto& n : g.nodes)
            if (ed.multi.count(n.id)) {
                mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
                mx = {std::max(mx.x, n.pos.x + NodeW(n)), std::max(mx.y, n.pos.y + NodeH(n, EffectiveOp(n)))};
            }
        if (mn.x < mx.x) {
            c.pos = mn - float2{30, 30 + kCommentTitle};
            c.size = (mx - mn) + float2{60, 60 + kCommentTitle};
        } else {
            c.pos = hovered ? toG(m) : toG(ImVec2(origin.x + size.x * 0.4f, origin.y + size.y * 0.4f));
        }
        g.comments.push_back(c);
        ed.selComment = c.id;
        changed = true;
        changeLabel = "Ajouter un commentaire";
    }
    if (keys && ed.selComment && !nodesSelected && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        g.comments.erase(std::remove_if(g.comments.begin(), g.comments.end(), [&](const EGComment& c) { return c.id == ed.selComment; }), g.comments.end());
        ed.selComment = 0;
        changed = true;
        changeLabel = "Supprimer un commentaire";
    }
    if ((keys && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G) && !ed.multi.empty()) || ed.demoCollapse) {
        ed.demoCollapse = false;
        if (const Id fid = CollapseToFunction(g, ed.multi, "Fonction")) {
            ed.selected = fid;
            ed.multi = {fid};
            changed = true;
            changeLabel = "Créer une fonction";
        }
    }
    if (keys && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        ed.selComment = 0;
        for (auto& n : g.nodes) ed.multi.insert(n.id);
        if (!ed.selected && !g.nodes.empty()) ed.selected = g.nodes.front().id;
    }
    // Copy: the selected nodes (not the output) and the links between them, as text in the clipboard.
    auto copyText = [&]() -> std::string {
        EffectGraph clip;
        clip.kind = g.kind;
        for (auto& n : g.nodes)
            if (ed.multi.count(n.id) && n.op != "output") {
                EGNode c = n;
                for (auto& l : c.inputs) if (!ed.multi.count(l.node)) l = {};
                clip.nodes.push_back(c);
            }
        return clip.nodes.empty() ? std::string() : "SXEG:" + clip.Serialize();
    };
    // Paste: new ids, links between the pasted nodes kept, parameters renamed when the name is taken.
    auto paste = [&](const std::string& text, bool atMouse) {
        EffectGraph clip;
        if (text.rfind("SXEG:", 0) != 0 || !EffectGraph::Deserialize(text.substr(5), clip)) return;
        float2 mn{1e9f, 1e9f};
        int count = 0;
        for (auto& n : clip.nodes) {
            if (n.op == "output" || !FindOp(n.op) || !OpAllowed(*FindOp(n.op), g.kind)) continue;
            mn.x = std::min(mn.x, n.pos.x);
            mn.y = std::min(mn.y, n.pos.y);
            ++count;
        }
        if (!count) return;
        const float2 off = atMouse ? toG(m) - mn : float2{40, 40};
        std::map<Id, Id> remap;
        for (auto& n : clip.nodes)
            if (n.op != "output" && FindOp(n.op) && OpAllowed(*FindOp(n.op), g.kind)) remap[n.id] = g.nextId++;
        auto nameTaken = [&](const std::string& name) {
            for (auto& o : g.nodes) if ((o.op == "param" || o.op == "paramcolor" || o.op == "param2") && o.name == name) return true;
            return false;
        };
        ed.multi.clear();
        for (auto& n : clip.nodes) {
            if (n.op == "output" || !FindOp(n.op) || !OpAllowed(*FindOp(n.op), g.kind)) continue;
            EGNode c = n;
            c.id = remap[n.id];
            c.pos = n.pos + off;
            for (auto& l : c.inputs) l = remap.count(l.node) ? EGLink{remap[l.node], l.output} : EGLink{};
            if (c.op == "param" || c.op == "paramcolor" || c.op == "param2") {
                std::string base = c.name;
                for (int k = 2; nameTaken(c.name); ++k) c.name = base + " " + std::to_string(k);
            }
            g.nodes.push_back(c);
            ed.multi.insert(c.id);
        }
        ed.selected = ed.multi.empty() ? 0 : *ed.multi.begin();
        changed = true;
        changeLabel = "Coller des nœuds";
    };
    if (keys && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !ed.multi.empty()) {
        std::string t = copyText();
        if (!t.empty()) ImGui::SetClipboardText(t.c_str());
    }
    if (keys && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X) && !ed.multi.empty()) {
        std::string t = copyText();
        if (!t.empty()) {
            ImGui::SetClipboardText(t.c_str());
            std::set<Id> gone;
            for (Id id : ed.multi) if (const EGNode* n = g.Find(id); n && n->op != "output") gone.insert(id);
            g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const EGNode& x) { return gone.count(x.id) != 0; }), g.nodes.end());
            for (auto& x : g.nodes) for (auto& l : x.inputs) if (gone.count(l.node)) l = {};
            ed.multi.clear();
            ed.selected = 0;
            changed = true;
            changeLabel = "Couper des nœuds";
        }
    }
    if (keys && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
        if (const char* t = ImGui::GetClipboardText()) paste(t, hovered);
    if (keys && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && !ed.multi.empty()) paste(copyText(), false);
    // Frame: the selection, or the whole graph (F; also when a graph is opened).
    auto frame = [&](bool selectionOnly) {
        float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
        int count = 0;
        for (auto& n : g.nodes) {
            if (!FindOp(n.op) || (selectionOnly && !ed.multi.count(n.id))) continue;
            const EGOpDef eff = EffectiveOp(n);
            mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
            mx = {std::max(mx.x, n.pos.x + NodeW(n)), std::max(mx.y, n.pos.y + NodeH(n, eff))};
            ++count;
        }
        if (!count) return;
        const float margin = 60;
        ed.zoom = std::clamp(std::min(size.x / (mx.x - mn.x + margin * 2), (size.y - 30) / (mx.y - mn.y + margin * 2)), 0.25f, 1.25f);
        ed.pan = {size.x / ed.zoom * 0.5f - (mn.x + mx.x) * 0.5f, (size.y + 30) / ed.zoom * 0.5f - (mn.y + mx.y) * 0.5f};
    };
    if (keys && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) frame(!ed.multi.empty());
    if (ed.frameAll) {
        ed.frameAll = false;
        if (gNodePreviews) SpreadNodes(g);  // view only: saved with the next change
        frame(false);
    }
    dl->AddText(ImVec2(origin.x + 10, origin.y + 8), ui::col::TextDim,
                "Clic droit : ajouter  |  sortie vers entrée : relier  |  cadre : sélection  |  Ctrl+C / X / V / D  |  F : cadrer  |  C : commentaire  |  double-clic sur un lien : reroutage  |  Suppr");
    dl->PopClipRect();

    // ---------------------------------------------------------- preview
    if (showPv) {
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("##efpreview", ImVec2(pvW, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Aperçu");
        ImGui::SameLine();
        int what = efPv.node ? 1 : 0;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##pvwhat", &what, "Sortie de l'effet\0Nœud sélectionné\0")) efPv.node = what == 1;
        // The selected node: the same graph with the output wired to it.
        EffectInstance pvFx = *fx;
        const EGNode* psel = g.Find(ed.selected);
        const bool nodeMode = efPv.node && g.kind == EGKind::Effect && psel && psel->op != "output";
        if (nodeMode) {
            const std::string key = ed.cachedJson + "#" + std::to_string(psel->id);
            if (efPv.nodeKey != key) {
                efPv.nodeGraph = PreviewGraphFor(g, psel->id, crTypes.OutType(psel->id, 0));
                efPv.nodeKey = key;
            }
            pvFx.graph = efPv.nodeGraph;
            pvFx.effectId.clear();
            pvFx.customCode.clear();
            pvFx.SyncParams(fx->Def());
        }
        const int nIn = g.kind == EGKind::Blend ? 3 : g.inputs;
        bool waiting = false;
        gpu::Texture* out = RenderEffectPreview(*v, pvFx, nIn, nodeMode ? 1 : 0, &waiting);
        DrawPreviewImage(*v, out, 0x7E5F000000000000ull + (nodeMode ? 1 : 0), efPv.slots[nodeMode ? 1 : 0].hash, ImGui::GetContentRegionAvail().x,
                         ImGui::GetContentRegionAvail().y * 0.55f);
        if (efPv.node && !nodeMode) ImGui::TextDisabled("Sélectionnez un nœud (pas la sortie) : son résultat s'affiche ici.");
        else if (waiting) ImGui::TextDisabled("Compilation...");
        else if (nodeMode) ImGui::TextDisabled("Résultat du nœud « %s »", psel->op == "custom" && !psel->name.empty() ? psel->name.c_str() : FindOp(psel->op) ? FindOp(psel->op)->label : psel->op.c_str());
        ImGui::SeparatorText("Entrées simulées");
        DrawPreviewSources(*v, nIn, g.kind == EGKind::Blend);
        if (nIn > 0) {
            const float tw = std::min(96.0f * dpi, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) / std::min(nIn, 4));
            for (int i = 0; i < std::min(nIn, 4); ++i) {
                if (i) ImGui::SameLine();
                u64 sh = 0;
                gpu::Texture* src = PreviewSource(*v, i, sh);
                DrawPreviewImage(*v, src, 0x7E5F000000000020ull + i, sh, tw);
            }
        }
        ImGui::SeparatorText("Réglages");
        if (fx != &brushProxy && EffectParamsUI(*fx, "efpv")) {
            if (d.Mode() == DocMode::Layers) d.SyncGraph();
            d.dirty = true;
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // Write back: brush graph -> brush settings (no document change).
    if (brushMode && (changed || live)) {
        auto old = fx->Def();
        fx->graph = composeRoot();
        ed.cachedJson = fx->graph;
        fx->SyncParams(old);
        brushTarget.tipGraph = fx->graph;
        brushTarget.tipParams = fx->params;
        if (g.spacing > 0) brushTarget.spacing = g.spacing;
        return;
    }
    if (changed || live) ed.lastEdit = ImGui::GetTime();
    // Write back to the effect instance.
    if (changed || live) {
        auto old = fx->Def();
        fx->graph = composeRoot();
        if (libGraph) {  // the library effect becomes a copy kept in the document
            fx->effectId.clear();
            fx->customCode.clear();
        }
        ed.cachedJson = fx->graph;
        fx->SyncParams(old);
        // Parameter defaults edited in the graph also update the instance value.
        // (found again: nodes may have been added or removed since the start of the frame)
        if (const EGNode* selNow = g.Find(ed.selected); selNow && (selNow->op == "param" || selNow->op == "paramcolor")) {
            auto nd = fx->Def();
            for (size_t i = 0; nd && i < nd->params.size() && i < fx->params.size(); ++i)
                if (nd->params[i].label == selNow->name || nd->params[i].id.find(selNow->name) != std::string::npos) fx->params[i] = nd->params[i].def;
        }
        if (d.Mode() == DocMode::Layers) d.SyncGraph();
        else if (Node* n = d.S().graph.Find(d.S().activeNode)) n->inputs.resize(NodeInputCount(*n), 0);
        d.dirty = true;
        if (!ed.pendingBefore) ed.pendingBefore = frameBefore;  // state before the first change of this gesture
        if (changed) {
            v->history.PushState(changeLabel, *ed.pendingBefore, d.S());
            ed.pendingBefore.reset();
        }
    }
}


// ------------------------------------------------------------------ code / library views of the editor

void App::DrawLibraryItemView(DocView& v, EffectInstance& fx, bool blend) {
    // A library effect / blend: read-only here, edited as a copy kept in the document.
    Document& d = *v.doc;
    auto def = fx.Def();
    if (!def) { ImGui::TextDisabled("Effet introuvable : %s", fx.effectId.c_str()); return; }
    const LibKind kind = blend ? LibKind::Blend : LibKind::Effect;
    ImGui::Text("%s", def->name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s  -  bibliothèque", def->category.c_str());
    bool user = IsUserLibraryItem(kind, fx.effectId), locked = LibraryItemLocked(kind, fx.effectId);
    if (!user) ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), "Élément de base protégé : modifier crée une copie dans ce document.");
    else {
        ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), "Élément de votre bibliothèque : modifier crée une copie dans ce document.");
        bool lk = locked;
        if (ImGui::Checkbox("Verrouillé", &lk)) SetLibraryLock(kind, fx.effectId, lk);
        ui::HelpTooltip("Verrouillé : l'élément ne peut plus être remplacé par un enregistrement (dans aucun projet).");
    }
    if (ImGui::Button("Modifier une copie (code)")) ConvertToCustomCode(v, fx);
    if (!def->graph.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Modifier une copie (graphe)")) {
            DocState before = d.S();
            auto old = fx.Def();
            fx.graph = def->graph;
            fx.effectId.clear();
            fx.customCode.clear();
            fx.SyncParams(old);
            d.SyncGraph();
            v.history.PushState("Modifier une copie (graphe)", before, d.S());
            efEd.cachedJson.clear();
        }
    }
    std::string text;
    ReadTextFile(gpu::Shaders().ShaderDir() + "/effects/" + fx.effectId + ".slang", text);
    // The saved graph (one long JSON line) is not shown; compacted code is laid out.
    std::string shown;
    for (size_t start = 0; start < text.size();) {
        size_t e = text.find('\n', start);
        if (e == std::string::npos) e = text.size();
        std::string_view line(text.data() + start, e - start);
        if (line.rfind("//@graph", 0) == 0) shown += "//@graph ... (graphe enregistré : bouton « Voir le graphe »)\n";
        else shown.append(line).append("\n");
        start = e + 1;
    }
    static std::string formattedKey, formatted;
    if (formattedKey != shown) {
        formattedKey = shown;
        formatted = ui::FormatHlsl(shown, 0);
    }
    ui::CodeEditor("##libcode", &formatted, ImVec2(-1, -1), true);
}

void App::DrawEffectCodeEditor(DocView& v, EffectInstance& fx) {
    Document& d = *v.doc;
    ImGui::TextDisabled("Copie modifiable dans ce document : la bibliothèque n'est pas touchée.");
    bool committed = CustomCodeUI(v, fx, 0, "efed");
    if (committed && d.Mode() == DocMode::Nodes)
        if (Node* n = d.S().graph.Find(d.S().activeNode); n && &n->fx == &fx) {
            if (auto nd = n->fx.Def()) n->name = nd->name;
            n->inputs.resize(NodeInputCount(*n), 0);
        }
}

void App::DrawBrushCodeEditor(DocView& v, BrushSettings& b) {
    auto td = TipDef(b);
    auto newButtons = [&] {
        if (ImGui::Button("Nouveau pinceau (code)")) {
            b.tipId.clear();
            b.tipGraph.clear();
            b.tipCode = kBrushTemplate;
            b.tipParams = ParseBrush("edit", b.tipCode)->def->DefaultParams();
            brushEd.bufferKey.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Nouveau pinceau (graphe)")) {
            b.tipId.clear();
            b.tipCode.clear();
            b.tipGraph = EffectGraph::DefaultBrush().Serialize();
            if (auto nd = TipDef(b); nd && nd->def) b.tipParams = nd->def->DefaultParams();
            efEd.cachedJson.clear();
        }
    };
    if (!td) {
        ImGui::TextWrapped("Pointe ronde standard (non programmable). Créez un pinceau : la pointe (Tip) dessine chaque touche, "
                           "Dynamics place les touches (suivre le trait, dispersion, aléa, temps).");
        newButtons();
        return;
    }
    std::string name = td->def ? td->def->name : "Pinceau";
    std::string category = td->def ? td->def->category : "Mes pinceaux";
    if (b.tipCode.empty()) {
        // Library brush: read-only, edited as a copy in the brush settings.
        bool user = IsUserLibraryItem(LibKind::Brush, b.tipId), locked = LibraryItemLocked(LibKind::Brush, b.tipId);
        ImGui::Text("%s", name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s  -  bibliothèque", category.c_str());
        ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), user ? "Pinceau de votre bibliothèque : modifier crée une copie." : "Pinceau de base protégé : modifier crée une copie.");
        if (user) {
            bool lk = locked;
            if (ImGui::Checkbox("Verrouillé", &lk)) SetLibraryLock(LibKind::Brush, b.tipId, lk);
        }
        if (ImGui::Button("Modifier une copie (code)")) {
            b.tipCode = td->text;
            brushEd.bufferKey.clear();
        }
        ImGui::SameLine();
        newButtons();
        std::string text = td->text;
        ui::CodeEditor("##brushlib", &text, ImVec2(-1, -1), true);
        return;
    }
    // Edited copy.
    if (brushEd.bufferKey != "edit") {
        brushEd.bufferKey = "edit";
        brushEd.buffer = b.tipCode;
    }
    ImGui::Text("%s", name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("copie modifiable (réglages du pinceau) : la bibliothèque n'est pas touchée");
    ui::CodeEditor("##brushcode", &brushEd.buffer, ImVec2(-1, std::max(160.0f * dpi, ImGui::GetContentRegionAvail().y - 80 * dpi)));
    bool apply = ImGui::Button("Appliquer (Ctrl+Entrée)") ||
                 (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl);
    if (apply && brushEd.buffer != b.tipCode) {
        auto old = td->def;
        b.tipCode = brushEd.buffer;
        auto nd = TipDef(b);
        // Keep the values of parameters that still exist.
        std::vector<float4> params = nd->def->DefaultParams();
        if (old)
            for (size_t i = 0; i < nd->def->params.size(); ++i)
                for (size_t k = 0; k < old->params.size() && k < b.tipParams.size(); ++k)
                    if (old->params[k].id == nd->def->params[i].id) params[i] = b.tipParams[k];
        b.tipParams = params;
        if (nd->spacing > 0) b.spacing = nd->spacing;
    }
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer dans la bibliothèque...")) ImGui::OpenPopup("##savebrushlib");
    std::string savedId;
    if (LibrarySavePopup("##savebrushlib", LibKind::Brush, name, category == "Exemples" ? "Mes pinceaux" : category, brushEd.buffer, &savedId)) {
        std::vector<float4> keep = b.tipParams;
        SelectTip(b, savedId);
        if (keep.size() == b.tipParams.size()) b.tipParams = keep;
    }
    if (!b.tipId.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Revenir à l'original")) {
            b.tipCode.clear();
            brushEd.bufferKey.clear();
            if (auto lib = Brushes().Find(b.tipId); lib && lib->def) b.tipParams = lib->def->DefaultParams();
        }
    }
    std::string err;
    if (auto nt = TipDef(b)) {
        if (!v.renderer.BrushPipeline(*nt, &err) && !err.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
            ImGui::TextWrapped("%s", err.c_str());
            ImGui::PopStyleColor();
        } else if (nt->def && !nt->def->error.empty()) {
            ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", nt->def->error.c_str());
        } else if (brushEd.buffer != b.tipCode) {
            ImGui::TextDisabled("Modifications non appliquées");
        }
    }
}


// ------------------------------------------------------------------ HLSL import

bool App::CreateEffectFromHlsl(DocView& v, const std::string& source, std::string* error) {
    Document& d = *v.doc;
    EffectGraph g;
    if (!GraphFromHlsl(source, g, error)) return false;
    Edit e(d, v.history, "Effet HLSL importé");
    if (d.Mode() == DocMode::Layers) {
        if (g.kind == EGKind::Blend) {
            Layer* l = d.ActiveLayer();
            if (!l) { if (error) *error = "sélectionnez le calque qui recevra la fusion"; return false; }
            l->blendFx = {};
            l->blendFx.graph = g.Serialize();
            if (auto def = l->blendFx.Def()) l->blendFx.params = def->DefaultParams();
            l->passThrough = false;
            efEd.target = 1;
        } else {
            Layer& l = d.AddEffectLayer(g.inputs == 0 ? LayerKind::Fill : LayerKind::Adjustment, "");
            l.name = g.name;
            l.effect.graph = g.Serialize();
            if (auto def = l.effect.Def()) l.effect.params = def->DefaultParams();
            efEd.target = 0;
            efEd.filterIndex = -1;
        }
        d.SyncGraph();
    } else {
        Node& n = d.AddEffectNode("", "", nodeEd.contextPos);
        n.fx.graph = g.Serialize();
        n.name = g.name;
        if (auto def = n.fx.Def()) n.fx.params = def->DefaultParams();
        n.inputs.assign(NodeInputCount(n), 0);
        d.S().activeNode = n.id;
        efEd.target = 0;
    }
    e.Commit();
    efEd.cachedJson.clear();
    Status("Effet HLSL importé : " + g.name);
    return true;
}

void App::DrawHlslImportPopup(EffectGraph* g, float2 pos, bool* changed) {
    // g: add the node to this graph; null: a new effect (layer / node) in the document.
    if (hlslImportRequest) {
        ImGui::OpenPopup("##hlslimport");
        hlslImportRequest = false;
        hlslImportError.clear();
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopup("##hlslimport")) return;
    ImGui::TextUnformatted(g ? "Importer du HLSL dans ce graphe" : "Nouvel effet depuis du HLSL");
    ImGui::TextDisabled("Annotations : //@name, //@category, //@in nom type défaut min max \"Libellé\" \"Catégorie\", //@out nom type, //@main fonction");
    ImGui::TextDisabled("Types : float float2 float3 float4 int bool color texture ; textures lues par Texture2DSample(T, S, uv) ou T.Sample(S, uv).");
    if (ImGui::Button("Ouvrir un fichier...")) {
        std::string path = platform::OpenFileDialog(hwnd, L"HLSL (*.hlsl, *.fx, *.slang, *.txt)\0*.hlsl;*.fx;*.slang;*.txt\0");
        if (!path.empty()) ReadTextFile(path, hlslImportBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Exemple")) {
        hlslImportBuffer =
            "//@name Vignette douce\n//@category Mes effets\n"
            "//@in Amount float 0.6 0 1 \"Intensité\" \"Réglages\"\n"
            "//@in Radius float 0.75 0.1 1.5 \"Rayon\" \"Réglages\"\n"
            "//@in Tint color 0 0 0 1 \"Couleur\" \"Couleur\"\n"
            "//@out Result float4\n\n"
            "float4 Vignette(Texture2D Image, SamplerState S, float2 UV, float Amount, float Radius, float4 Tint)\n"
            "{\n"
            "    float4 c = Image.Sample(S, UV);\n"
            "    float d = length(UV - 0.5) * 1.41421;\n"
            "    float k = smoothstep(Radius, Radius - 0.5, d);\n"
            "    return float4(lerp(Tint.rgb, c.rgb, lerp(1.0, k, Amount)), c.a);\n"
            "}\n";
    }
    ImGui::PushFont(ui::GetFonts().mono, 0.0f);
    ui::CodeEditor("##hlslsrc", &hlslImportBuffer, ImVec2(760 * dpi, 420 * dpi));
    ImGui::PopFont();
    if (ImGui::Button(g ? "Ajouter le nœud" : "Créer l'effet")) {
        hlslImportError.clear();
        if (g) {
            HlslImport imp = ParseHlslNode(hlslImportBuffer);
            if (!imp.error.empty()) hlslImportError = imp.error;
            else {
                efEd.selected = AddHlslNode(*g, imp, pos);
                if (changed) *changed = true;
                ImGui::CloseCurrentPopup();
            }
        } else if (DocView* v = Active()) {
            if (CreateEffectFromHlsl(*v, hlslImportBuffer, &hlslImportError)) ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Fermer")) ImGui::CloseCurrentPopup();
    if (!hlslImportError.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", hlslImportError.c_str());
    }
    ImGui::EndPopup();
}

// ------------------------------------------------------------------ brush test pad

void App::DrawBrushPad(DocView& v, BrushSettings& b) {
    // Paint with the brush being edited (mouse or pen: pressure, tilt, rotation, speed).
    auto td = TipDef(b);
    const float h = 150 * dpi;
    const float w = std::max(120.0f, ImGui::GetContentRegionAvail().x);
    const u32 pw = (u32)w, ph = (u32)h;
    if (!pad.ready || !v.renderer.PadTexture() || v.renderer.PadTexture()->Width() != pw || v.renderer.PadTexture()->Height() != ph) {
        v.renderer.PadClear(*ctx, pw, ph);
        pad.ready = true;
    }
    ImVec2 o = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Light paper behind (dark brushes stay visible), the pad on top.
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(236, 234, 228, 255), 4);
    if (gpu::Texture* t = v.renderer.PadTexture()) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), o, ImVec2(o.x + w, o.y + h));
    ImGui::InvisibleButton("##brushpad", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    dl->AddRect(o, ImVec2(o.x + w, o.y + h), hovered ? ui::col::Accent : ui::col::Border, 4);
    if (!pad.stroking) dl->AddText(ImVec2(o.x + 8, o.y + 6), IM_COL32(90, 90, 100, 255), "Zone d'essai : dessinez ici (souris ou stylet)");
    if (hovered) {
        float r = std::max(b.size * 0.5f, 1.0f);
        ImVec2 m = ImGui::GetIO().MousePos;
        dl->AddCircle(m, r, IM_COL32(40, 40, 50, 200), 0, 1.0f);
    }
    if (!td) return;
    const bool erase = tool == Tool::Eraser;
    EffectInstance tip;
    tip.params = b.tipParams;
    BrushSettings bound = b;
    BindTipTextures(bound, *td);
    float4 color = erase ? float4{1, 1, 1, 1} : LinearColor(fg);
    const float tipSpacing = td->spacing > 0 ? td->spacing : b.spacing;
    auto emit = [&](const std::vector<PointerSample>& samples) {
        std::vector<Dab> dabs;
        std::vector<float4> extra;
        for (auto& s : samples) {
            float2 p{s.x - o.x, s.y - o.y};
            float pr = s.pen ? PenPressure(s.pressure) : 1.0f;
            float2 d = p - pad.last;
            float len = Length(d);
            float spacing = std::max(1.0f, b.size * std::max(tipSpacing, 0.01f) * (b.pressureSize ? std::max(pr, 0.1f) : 1.0f));
            float angle = len > 1e-3f ? std::atan2(d.y, d.x) : 0.0f;
            float t = spacing - pad.remainder;
            while (t <= len) {
                float2 q = pad.last + d * (t / std::max(len, 1e-5f));
                dabs.push_back({q.x, q.y, b.size * 0.5f * (b.pressureSize ? std::max(pr, 0.05f) : 1.0f), b.flow * (b.pressureOpacity ? pr : 1.0f)});
                extra.push_back({pr, angle, (float)(ImGui::GetTime() - pad.start), pad.dist + t});
                extra.push_back(PenExtra(s));
                t += spacing;
            }
            pad.remainder = len > 0 ? len - (t - spacing) : pad.remainder;
            pad.dist += len;
            if (len > 0) pad.last = p;
        }
        if (!dabs.empty() && v.renderer.PadStroke(*ctx, *td, tip, bound, color, dabs, extra, erase, pad.dabs)) pad.dabs += (u32)dabs.size();
    };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        v.renderer.PadBeginStroke(*ctx);
        pad.stroking = true;
        ImVec2 m = ImGui::GetIO().MousePos;
        pad.last = {m.x - o.x, m.y - o.y};
        pad.remainder = 0;
        pad.dist = 0;
        pad.dabs = 0;
        pad.start = ImGui::GetTime();
        PointerSample first = platform::PenActive() ? platform::LastPenSample() : PointerSample{m.x, m.y, 1.0f, false, false};
        first.x = m.x;
        first.y = m.y;
        pad.remainder = b.size * std::max(tipSpacing, 0.01f);  // first dab at the click
        emit({first});
    }
    if (pad.stroking) {
        std::vector<PointerSample> samples = frameSamples;
        if (samples.empty()) {
            PointerSample s = platform::PenActive() ? platform::LastPenSample() : PointerSample{0, 0, 1.0f, false, false};
            s.x = ImGui::GetIO().MousePos.x;
            s.y = ImGui::GetIO().MousePos.y;
            samples.push_back(s);
        }
        emit(samples);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) pad.stroking = false;
    }
    if (ImGui::SmallButton("Effacer la zone d'essai")) v.renderer.PadClear(*ctx, pw, ph);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", platform::PenActive() ? platform::TabletStatus().c_str() : "Souris : pression 1, sans inclinaison");
}

}  // namespace sx
