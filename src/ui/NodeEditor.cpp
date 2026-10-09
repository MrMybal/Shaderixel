// Node graph editor (custom ImDrawList widget): pan/zoom, links, live thumbnails.
#include "ui/App.h"
#include <unordered_set>
#include "ui/Theme.h"
#include "doc/EffectGraph.h"
#include "doc/GraphTypes.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sx {

void GraphFrozenNodes(const Graph& g, std::unordered_set<Id>& frozen);  // GraphTools.cpp

namespace {
constexpr float kNodeW = 184.0f;
constexpr float kHeader = 26.0f;
constexpr float kRow = 22.0f;
constexpr float kPin = 6.0f;
constexpr u64 kThumbKeyBit = 1ull << 62;

ImU32 HeaderColor(const Node& n) {
    switch (n.kind) {
        case NodeKind::Output: return IM_COL32(52, 160, 110, 255);
        case NodeKind::Surface: return IM_COL32(36, 150, 168, 255);
        case NodeKind::Blend: return IM_COL32(64, 104, 200, 255);
        case NodeKind::Split: return IM_COL32(70, 150, 90, 255);
        case NodeKind::Tool: return IM_COL32(170, 124, 26, 255);
        case NodeKind::Value: return IM_COL32(70, 120, 60, 255);
        case NodeKind::Math: return IM_COL32(60, 110, 90, 255);
        case NodeKind::VarGet: case NodeKind::VarSet: return IM_COL32(120, 90, 50, 255);
        case NodeKind::Event: return IM_COL32(170, 40, 40, 255);
        case NodeKind::Action: return IM_COL32(60, 70, 110, 255);
        case NodeKind::Function: return n.funcKind == "tool" ? IM_COL32(170, 124, 26, 255) : IM_COL32(150, 80, 180, 255);
        case NodeKind::SubInput: return IM_COL32(40, 130, 150, 255);
        case NodeKind::SubOutput: return IM_COL32(52, 150, 100, 255);
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            if (!n.fx.customCode.empty()) return IM_COL32(200, 92, 150, 255);
            if (d && d->inputs.empty()) return IM_COL32(210, 130, 50, 255);
            return IM_COL32(112, 84, 220, 255);
        }
    }
    return IM_COL32(90, 90, 90, 255);
}

// Kind of a node, shown as a tag in its header: Image (pixels), Draw (drawn by a shader: fills,
// generators, text, shapes), Blend, Effect (input -> output), Tool (graph tools: Transform, more to
// come), SubGraph (a graph inside:
// group, smart object holding a document), plus the channel split and the output.
struct NodeTag { const char* text; ImU32 color; };
// Tools placed in the layer graph (operate on an input without being an image effect).
bool IsGraphTool(const std::string& id) { return id == "transform_2d"; }
NodeTag NodeTypeTag(Document& d, const Node& n) {
    const NodeTag image = {"IMAGE", IM_COL32(36, 150, 168, 255)}, draw = {"DRAW", IM_COL32(210, 130, 50, 255)},
                  blend = {"BLEND", IM_COL32(64, 104, 200, 255)}, effect = {"EFFECT", IM_COL32(112, 84, 220, 255)},
                  tool = {"TOOL", IM_COL32(220, 170, 40, 255)}, sub = {"SUBGRAPH", IM_COL32(170, 90, 200, 255)};
    Layer* l = n.layer ? d.FindLayer(n.layer) : nullptr;
    switch (n.kind) {
        case NodeKind::Output: return {"SORTIE", IM_COL32(52, 160, 110, 255)};
        case NodeKind::Split: return {"CANAUX", IM_COL32(70, 150, 90, 255)};
        case NodeKind::Tool: return tool;
        case NodeKind::Function: return n.funcKind == "tool" ? tool : sub;
        case NodeKind::SubInput: return {"IN", IM_COL32(60, 190, 210, 255)};
        case NodeKind::Value: return {"VALEUR", IM_COL32(160, 230, 70, 255)};
        case NodeKind::Math: return {"MATH", IM_COL32(120, 220, 170, 255)};
        case NodeKind::VarGet: case NodeKind::VarSet: return {"VAR", IM_COL32(230, 180, 90, 255)};
        case NodeKind::Event: return {"EVENT", IM_COL32(255, 110, 110, 255)};
        case NodeKind::Action: return {"EXEC", IM_COL32(170, 190, 255, 255)};
        case NodeKind::SubOutput: return {"OUT", IM_COL32(70, 200, 130, 255)};
        case NodeKind::Blend: return l && l->kind == LayerKind::Group ? sub : blend;
        case NodeKind::Surface:
            if (l && (l->kind == LayerKind::Text || l->kind == LayerKind::Shape)) return draw;
            if (l && l->kind == LayerKind::Smart) {
                SmartObject* so = d.GetSmart(l->smart);
                std::string ext = so ? Narrow(Utf8Path(so->linkedPath).extension().wstring()) : "";
                if (so && (so->child || so->childOpen || ext == ".sxl" || ext == ".sx3d")) return sub;
            }
            return image;
        case NodeKind::Effect: {
            if (IsGraphTool(n.fx.effectId)) return tool;
            auto def = n.fx.Def();
            if ((def && def->inputs.empty()) || (l && l->kind == LayerKind::Fill)) return draw;
            return effect;
        }
    }
    return effect;
}

// Pins and links are colored by type (Unreal-style): RGBA image pink, single channel green,
// values (float, int, bool, vec2, color, text, name) and the execution flow (white).
ImU32 TypeColor(PinType t, int alpha = 255) {
    switch (t) {
        case PinType::Channel: return IM_COL32(110, 210, 120, alpha);
        case PinType::Float: return IM_COL32(160, 230, 70, alpha);
        case PinType::Int: return IM_COL32(40, 205, 175, alpha);
        case PinType::Bool: return IM_COL32(225, 60, 60, alpha);
        case PinType::Vec2: return IM_COL32(240, 200, 60, alpha);
        case PinType::Color: return IM_COL32(80, 145, 255, alpha);
        case PinType::String: return IM_COL32(250, 150, 60, alpha);
        case PinType::Name: return IM_COL32(185, 145, 255, alpha);
        case PinType::Exec: return IM_COL32(240, 240, 240, alpha);
        default: return IM_COL32(230, 120, 210, alpha);
    }
}
// Pin shape: circle (single value / image), square (array), arrow (execution).
void DrawPin(ImDrawList* dl, ImVec2 p, float r, const PinDesc& pin, bool filled, float zoom) {
    const ImU32 c = TypeColor(pin.type);
    if (pin.type == PinType::Exec) {
        ImVec2 a(p.x - r, p.y - r), b(p.x + r * 0.2f, p.y - r), t(p.x + r * 1.1f, p.y), d(p.x + r * 0.2f, p.y + r), e(p.x - r, p.y + r);
        ImVec2 pts[] = {a, b, t, d, e};
        if (filled) dl->AddConvexPolyFilled(pts, 5, c);
        else dl->AddPolyline(pts, 5, c, ImDrawFlags_Closed, 1.8f * zoom);
        return;
    }
    if (pin.array) {
        if (filled) dl->AddRectFilled(ImVec2(p.x - r, p.y - r), ImVec2(p.x + r, p.y + r), c, 1.5f);
        else dl->AddRect(ImVec2(p.x - r, p.y - r), ImVec2(p.x + r, p.y + r), c, 1.5f, 0, 2.0f * zoom);
        return;
    }
    if (filled) {
        dl->AddCircleFilled(p, r, c);
        dl->AddCircle(p, r, IM_COL32(20, 20, 24, 255), 0, 1.5f);
    } else {
        dl->AddCircleFilled(p, r, IM_COL32(40, 42, 50, 255));
        dl->AddCircle(p, r, c, 0, 2.0f * zoom);
    }
}
// Labels of the Split outputs keep their channel color.
ImU32 LabelColor(const Node& n, int k) {
    if (n.kind != NodeKind::Split) return TypeColor(PinType::RGBA, 230);
    static const ImU32 c[] = {IM_COL32(235, 90, 90, 255), IM_COL32(100, 215, 110, 255), IM_COL32(90, 150, 255, 255), IM_COL32(210, 210, 215, 255)};
    return c[std::clamp(k, 0, 3)];
}

struct Layout {
    float h, w = kNodeW;
    float thumbW, thumbH;
    int inputs, outputs, rows;
    bool image = true;  // shows an image thumbnail (else a line of text: value, operation...)
    std::vector<float> valX, valW;  // value field of each input (node-relative x, width; x < 0: none)
};

// Width of the value field shown next to an unlinked value input (Unreal-style).
float ValueFieldWidth(const PinDesc& p) {
    if (p.array) return ImGui::CalcTextSize("[000]").x * 0.85f + 8;
    switch (p.type) {
        case PinType::Bool: return 15;
        case PinType::Color: return 34;
        case PinType::Float: case PinType::Int: return 58;
        default: return 104;
    }
}

Layout NodeLayout(const Node& n, float aspect) {
    Layout l;
    l.inputs = NodeInputCount(n);
    l.outputs = NodeOutputCount(n);
    l.rows = std::max({l.inputs, l.outputs, 1});
    // Width: inputs (label + value field) and outputs (label) never overlap.
    std::vector<PinDesc> ins, outs;
    NodeSignature(n, &ins, &outs);
    float inW = 0, outW = 0;
    l.valX.assign(ins.size(), -1.0f);
    l.valW.assign(ins.size(), 0.0f);
    for (size_t i = 0; i < ins.size(); ++i) {
        const float x = 10 + ImGui::CalcTextSize(ins[i].name.c_str()).x * 0.9f + 6;
        float wv = 0;
        const bool linked = i < n.inputs.size() && n.inputs[i];
        if (!linked && PinIsValue(ins[i].type)) {
            wv = ValueFieldWidth(ins[i]);
            l.valX[i] = x;
            l.valW[i] = wv;
        }
        inW = std::max(inW, x + wv);
    }
    for (auto& o : outs) outW = std::max(outW, ImGui::CalcTextSize(o.name.c_str()).x * 0.85f + 14);
    l.w = std::max(kNodeW, std::ceil(inW + 14 + outW));
    l.thumbW = l.w - 16;
    l.thumbH = std::clamp(l.thumbW / std::max(aspect, 0.1f), 40.0f, std::min(l.thumbW, 200.0f));
    l.image = n.kind == NodeKind::Output || (n.kind == NodeKind::SubOutput && PinIsImage(n.value.type));
    if (n.kind == NodeKind::Function && l.outputs == 0) l.image = true;
    for (int k = 0; k < l.outputs && !l.image; ++k) l.image = PinIsImage(NodeOutputType(n, k));
    if (!l.image) l.thumbH = 22;
    l.h = kHeader + l.rows * kRow + l.thumbH + 12;
    return l;
}

// Node menu entries (tree by category).
struct NodeEntry {
    std::string category, label, keywords;
    std::function<Id(Document&, float2)> make;
};

u64 EntriesGeneration() { return Effects().Generation() * 1000003ull + FunctionLibraryGen(); }

std::vector<NodeEntry> NodeEntries() {
    std::vector<NodeEntry> e;
    e.push_back({"Fonctions", "Nouveau sous-graphe (fonction)", "subgraph sous graphe fonction function groupe",
                 [](Document& d, float2 p) { return d.AddFunctionNode("subgraph", p).id; }});
    e.push_back({"Fonctions", "Nouvel outil personnalisé (fonction)", "tool outil custom fonction function apply",
                 [](Document& d, float2 p) { return d.AddFunctionNode("tool", p).id; }});
    // Values, operations, variables, execution flow
    const PinType kValTypes[] = {PinType::Float, PinType::Int, PinType::Bool, PinType::Vec2, PinType::Color, PinType::String, PinType::Name};
    for (PinType t : kValTypes) {
        e.push_back({"Valeurs", PinTypeLabel(t), std::string("value constante ") + PinTypeLabel(t),
                     [t](Document& d, float2 p) { return AddTypedNode(d, NodeKind::Value, "", t, false, p).id; }});
        e.push_back({"Valeurs : tableaux", std::string("Tableau ") + PinTypeLabel(t), std::string("array tableau liste ") + PinTypeLabel(t),
                     [t](Document& d, float2 p) { return AddTypedNode(d, NodeKind::Value, "", t, true, p).id; }});
    }
    for (auto& op : MathOps()) {
        std::string id = op.id;
        e.push_back({op.category, op.label, op.keywords, [id](Document& d, float2 p) { return AddTypedNode(d, NodeKind::Math, id, PinType::Float, false, p).id; }});
    }
    e.push_back({"Variables", "Lire une variable", "get variable lire", [](Document& d, float2 p) { return AddTypedNode(d, NodeKind::VarGet, "", PinType::Float, false, p).id; }});
    e.push_back({"Variables", "Définir une variable", "set variable definir exec",
                 [](Document& d, float2 p) { return AddTypedNode(d, NodeKind::VarSet, "", PinType::Float, false, p).id; }});
    e.push_back({"Exécution", "Événement (bouton d'exécution)", "event bouton exec run start",
                 [](Document& d, float2 p) { return AddTypedNode(d, NodeKind::Event, "Exécuter", PinType::Exec, false, p).id; }});
    for (auto& a : ExecActions()) {
        std::string id = a.id;
        e.push_back({"Exécution", a.label, a.keywords, [id](Document& d, float2 p) { return AddTypedNode(d, NodeKind::Action, id, PinType::Exec, false, p).id; }});
    }
    e.push_back({"Fonctions", "Entrée de fonction", "input entree broche pin parametre", [](Document& d, float2 p) { return d.AddNode(NodeKind::SubInput, p).id; }});
    e.push_back({"Fonctions", "Sortie de fonction", "output sortie broche pin", [](Document& d, float2 p) { return d.AddNode(NodeKind::SubOutput, p).id; }});
    for (auto& f : FunctionLibrary()) {
        std::string path = f.path;
        e.push_back({f.kind == "tool" ? "Bibliothèque : outils" : "Bibliothèque : sous-graphes", f.name, f.desc + " fonction bibliotheque library",
                     [path](Document& d, float2 p) { return g_insertLibraryFunction ? g_insertLibraryFunction(d, p, path) : (Id)0; }});
    }
    e.push_back({"Canaux", "Séparer RGBA (R, V, B, A)", "split channels canal", [](Document& d, float2 p) { return d.AddNode(NodeKind::Split, p).id; }});
    e.push_back({"Calques", "Calque pixel", "surface peinture", [](Document& d, float2 p) { return d.AddNode(NodeKind::Surface, p).id; }});
    e.push_back({"Fusion", "Fusion (mode de fusion)", "blend mélange", [](Document& d, float2 p) { return d.AddNode(NodeKind::Blend, p).id; }});
    e.push_back({"Fusion", "Fusion personnalisée (A, B, masque)", "blend custom hlsl mélange",
                 [](Document& d, float2 p) { return d.AddEffectNode("", kCustomBlendTemplate, p).id; }});
    e.push_back({"Outils", "Bake (figer l'entrée en pixels)", "bake figer pixels cache freeze passlive",
                 [](Document& d, float2 p) { return d.AddToolNode("bake", p).id; }});
    e.push_back({"Outils", "Upscale (agrandir puis baker)", "upscale agrandir resolution esrgan ia",
                 [](Document& d, float2 p) { return d.AddToolNode("upscale", p).id; }});
    e.push_back({"Outils", "ComfyUI (workflow sur l'entrée)", "comfyui ia ai stable diffusion workflow img2img",
                 [](Document& d, float2 p) { return d.AddToolNode("comfyui", p).id; }});
    e.push_back({"Outils", "Transformation", "transform position echelle rotation", [](Document& d, float2 p) {
                     Node& nn = d.AddEffectNode("transform_2d", "", p);
                     nn.name = "Transformation";
                     return nn.id;
                 }});
    e.push_back({"Personnalisé", "HLSL personnalisé", "code custom shader", [](Document& d, float2 p) { return d.AddEffectNode("", kCustomEffectTemplate, p).id; }});
    e.push_back({"Personnalisé", "Effet visuel (graphe)", "graph unreal material", [](Document& d, float2 p) {
                     Node& nn = d.AddEffectNode("", "", p);
                     nn.fx.graph = EffectGraph::Default().Serialize();
                     nn.name = "Effet visuel";
                     nn.fx.params.clear();
                     nn.inputs.assign(NodeInputCount(nn), 0);
                     return nn.id;
                 }});
    for (auto& def : Effects().All()) {
        std::string id = def->id;
        if (EffectIsVariant(*def)) continue;  // variants stay in the Effets panel (Variantes)
        e.push_back({def->blend ? std::string("Fusions") : EffectFamily(*def), def->name, id + " " + def->category, [id](Document& d, float2 p) { return d.AddEffectNode(id, "", p).id; }});
    }
    return e;
}

std::string Lower(std::string s) {
    for (auto& ch : s) ch = (char)tolower((unsigned char)ch);
    return s;
}
}  // namespace

void App::DrawNodePalette() {
    if (!ImGui::Begin("Palette de nœuds", PanelFlag("Palette de nœuds"))) { ImGui::End(); return; }
    ImGui::RadioButton("Graphe des calques", &paletteFocus, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Graphe d'effet", &paletteFocus, 1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##palsearch", "Rechercher un nœud...", &paletteSearch);
    std::string q = Lower(paletteSearch);
    ImGui::TextDisabled("Glisser sur le graphe, ou clic : au centre de la vue.");
    ImGui::BeginChild("##pallist");
    auto item = [&](const std::string& label, const std::string& key, const char* tip) {
        if (ImGui::Selectable(label.c_str())) {
            paletteAdd.pending = true;
            paletteAdd.target = paletteFocus;
            paletteAdd.key = key;
            paletteAdd.hasPos = false;
        }
        if (tip && *tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            std::string data = std::string(paletteFocus ? "1" : "0") + key;
            ImGui::SetDragDropPayload(kPalettePayload, data.c_str(), data.size() + 1);
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
        }
    };
    if (paletteFocus == 0) {
        static std::vector<NodeEntry> entries;
        static u64 gen = ~0ull;
        if (gen != EntriesGeneration()) { entries = NodeEntries(); gen = EntriesGeneration(); }
        std::vector<std::string> cats;
        for (auto& e : entries) if (std::find(cats.begin(), cats.end(), e.category) == cats.end()) cats.push_back(e.category);
        for (auto& c : cats) {
            bool any = false;
            for (auto& e : entries) any |= e.category == c && (q.empty() || Lower(e.label + " " + e.category + " " + e.keywords).find(q) != std::string::npos);
            if (!any) continue;
            if (!q.empty()) ImGui::SetNextItemOpen(true);
            if (!ImGui::TreeNodeEx(c.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) continue;
            for (auto& e : entries) {
                if (e.category != c || (!q.empty() && Lower(e.label + " " + e.category + " " + e.keywords).find(q) == std::string::npos)) continue;
                ImGui::PushID(e.keywords.c_str());
                item(e.label, e.label + "\x1f" + e.category, nullptr);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    } else {
        // Effect graph operations allowed for the graph being edited.
        EGKind kind = efEd.target == 2 ? EGKind::Brush : efEd.target == 1 ? EGKind::Blend : EGKind::Effect;
        std::string lastCat;
        bool open = false;
        for (auto& od : EffectGraphOps()) {
            if (std::string(od.op) == "output" || !OpAllowed(od, kind)) continue;
            if (!q.empty() && Lower(std::string(od.label) + " " + od.op + " " + od.category).find(q) == std::string::npos) continue;
            if (od.category != lastCat) {
                if (open) ImGui::TreePop();
                lastCat = od.category;
                if (!q.empty()) ImGui::SetNextItemOpen(true);
                open = ImGui::TreeNodeEx(od.category, ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen);
            }
            if (!open) continue;
            ImGui::PushID(od.op);
            item(od.label, od.op, nullptr);
            ImGui::PopID();
        }
        if (open) ImGui::TreePop();
    }
    ImGui::EndChild();
    ImGui::End();
}

void App::DrawNodeEditor() {
    graphFocused = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Graphe", PanelFlag("Graphe"), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    DocView* v = Active();
    if (!open || !v) { ImGui::End(); return; }
    Document& d = *v->doc;
    Graph& g = d.S().graph;
    NodeEditorState& ed = nodeEd;
    const bool editableAtStart = d.Mode() == DocMode::Nodes;
    ImGuiIO& io = ImGui::GetIO();
    std::erase_if(ed.selection, [&](Id id) { return !g.Find(id); });

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 10 || size.y < 10) { ImGui::End(); return; }
    ImGui::InvisibleButton("##graph", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    graphFocused = focused;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    auto toScreen = [&](float2 p) { return ImVec2(origin.x + (p.x + ed.pan.x) * ed.zoom, origin.y + (p.y + ed.pan.y) * ed.zoom); };
    auto toGraph = [&](ImVec2 s) { return float2{(s.x - origin.x) / ed.zoom - ed.pan.x, (s.y - origin.y) / ed.zoom - ed.pan.y}; };
    const float aspect = (float)d.Width() / d.Height();

    // Pan & zoom
    if (hovered && io.MouseWheel != 0) {
        float2 before = toGraph(io.MousePos);
        ed.zoom = std::clamp(ed.zoom * std::pow(1.15f, io.MouseWheel), 0.2f, 2.5f);
        float2 after = toGraph(io.MousePos);
        ed.pan = ed.pan + (after - before);
    }
    bool panDrag = ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4) ||
                                             (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0) && (io.KeyAlt || spaceHeld)));
    if (panDrag) ed.pan = ed.pan + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};

    // Grid
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(19, 20, 24, 255));
    float step = 24.0f * ed.zoom;
    if (step > 6) {
        float ox = std::fmod(ed.pan.x * ed.zoom, step), oy = std::fmod(ed.pan.y * ed.zoom, step);
        for (float x = ox; x < size.x; x += step) dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y), ui::col::Grid);
        for (float y = oy; y < size.y; y += step) dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y), ui::col::Grid);
    }

    // Comment frames, behind links and nodes.
    const float kCommentHeader = 30.0f;
    auto commentByColor = [](const float4& c, int a) { return IM_COL32((int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255), a); };
    ed.commentRects.clear();
    for (const GraphComment& c : g.comments) {
        const ImVec2 a = toScreen(c.pos), b = toScreen(c.pos + c.size);
        ed.commentRects[c.id] = ImVec4(a.x, a.y, b.x, a.y + kCommentHeader * ed.zoom);
        if (b.x < origin.x || b.y < origin.y || a.x > origin.x + size.x || a.y > origin.y + size.y) continue;
        const bool sel = ed.selComment == c.id, hot = ed.hoverComment == c.id;
        const float rr = 6 * ed.zoom;
        dl->AddRectFilled(a, b, commentByColor(c.color, 34), rr);
        dl->AddRectFilled(a, ImVec2(b.x, a.y + kCommentHeader * ed.zoom), commentByColor(c.color, 150), rr, ImDrawFlags_RoundCornersTop);
        dl->AddRect(a, b, sel ? ui::col::Selection : commentByColor(c.color, hot ? 255 : 170), rr, 0, sel ? 2.5f : 1.5f);
        if (ed.zoom > 0.25f) {
            const float tfs = ImGui::GetFontSize() * ed.zoom * 1.25f;
            dl->PushClipRect(a, ImVec2(b.x - 4, a.y + kCommentHeader * ed.zoom), true);
            dl->AddText(ImGui::GetFont(), tfs, ImVec2(a.x + 10 * ed.zoom, a.y + (kCommentHeader * ed.zoom - tfs) * 0.5f), IM_COL32_WHITE, c.text.c_str());
            dl->PopClipRect();
        }
        // Resize grip.
        const float gs = 12 * ed.zoom;
        dl->AddTriangleFilled(ImVec2(b.x - gs, b.y - 2), ImVec2(b.x - 2, b.y - gs), ImVec2(b.x - 2, b.y - 2), commentByColor(c.color, hot ? 255 : 140));
    }

    ed.valRects.clear();
    // Geometry (layouts computed once per frame)
    std::unordered_map<Id, Layout> layCache;
    auto lay = [&](const Node& n) -> const Layout& {
        auto it = layCache.find(n.id);
        if (it != layCache.end()) return it->second;
        return layCache.emplace(n.id, NodeLayout(n, aspect)).first->second;
    };
    auto outPin = [&](const Node& n, int k) { return toScreen(n.pos + float2{lay(n).w, kHeader + kRow * (k + 0.5f)}); };
    auto valRect = [&](const Node& n, int i, ImVec2& va, ImVec2& vb) {
        const Layout& L = lay(n);
        if (i >= (int)L.valX.size() || L.valX[i] < 0) return false;
        va = toScreen(n.pos + float2{L.valX[i], kHeader + kRow * i + 3});
        vb = toScreen(n.pos + float2{L.valX[i] + L.valW[i], kHeader + kRow * (i + 1) - 3});
        return true;
    };
    auto inPin = [&](const Node& n, int i) { return toScreen(n.pos + float2{0, kHeader + kRow * (i + 0.5f)}); };
    float pinR = kPin * ed.zoom;
    ImVec2 mouse = io.MousePos;
    auto isNear = [&](ImVec2 a, ImVec2 b, float r) { float dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy <= r * r; };

    // Hit testing (topmost = last)
    Id hoverNode = 0, hoverOut = 0, hoverInNode = 0, hoverValNode = 0;
    int hoverIn = -1, hoverOutCh = 0, hoverValPin = -1;
    if (hovered) {
        for (int k = (int)g.nodes.size() - 1; k >= 0; --k) {
            const Node& n = g.nodes[k];
            const Layout& L = lay(n);
            for (int i = 0; i < L.inputs && !hoverValNode && ed.zoom > 0.45f; ++i) {
                ImVec2 va, vb;
                if (valRect(n, i, va, vb) && mouse.x >= va.x && mouse.x <= vb.x && mouse.y >= va.y && mouse.y <= vb.y) {
                    hoverValNode = n.id;
                    hoverValPin = i;
                }
            }
            if (hoverValNode) { hoverNode = n.id; break; }
            bool pinHit = false;
            for (int o = 0; o < L.outputs && !pinHit; ++o)
                if (isNear(mouse, outPin(n, o), pinR * 2.0f)) { hoverOut = n.id; hoverOutCh = o; pinHit = true; }
            for (int i = 0; i < L.inputs && !pinHit; ++i)
                if (isNear(mouse, inPin(n, i), pinR * 2.0f)) { hoverInNode = n.id; hoverIn = i; pinHit = true; }
            if (pinHit) break;
            ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{L.w, L.h});
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) { hoverNode = n.id; break; }
        }
    }

    ed.hoverComment = 0;
    ed.hoverCommentPart = 0;
    if (hovered && !hoverNode && !hoverOut && !hoverInNode) {
        for (int k = (int)g.comments.size() - 1; k >= 0 && !ed.hoverComment; --k) {
            const GraphComment& c = g.comments[k];
            const ImVec2 a = toScreen(c.pos), b = toScreen(c.pos + c.size);
            const float gs = 14 * ed.zoom;
            if (mouse.x >= b.x - gs && mouse.x <= b.x + 2 && mouse.y >= b.y - gs && mouse.y <= b.y + 2) { ed.hoverComment = c.id; ed.hoverCommentPart = 2; }
            else if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= a.y + kCommentHeader * ed.zoom) { ed.hoverComment = c.id; ed.hoverCommentPart = 1; }
        }
        if (ed.hoverCommentPart == 2) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
    }
    auto findComment = [&](Id id) -> GraphComment* {
        for (auto& c : g.comments) if (c.id == id) return &c;
        return nullptr;
    };
    // Comment around the selection (or a default box at `at`).
    auto addComment = [&](float2 at) {
        DocState before = d.S();
        GraphComment c;
        c.id = d.NewId();
        if (!ed.selection.empty()) {
            float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
            for (Id id : ed.selection)
                if (const Node* n = g.Find(id)) {
                    mn = {std::min(mn.x, n->pos.x), std::min(mn.y, n->pos.y)};
                    mx = {std::max(mx.x, n->pos.x + lay(*n).w), std::max(mx.y, n->pos.y + lay(*n).h)};
                }
            c.pos = mn - float2{30, 30 + kCommentHeader};
            c.size = (mx - mn) + float2{60, 60 + kCommentHeader};
        } else {
            c.pos = at;
        }
        g.comments.push_back(c);
        ed.selComment = c.id;
        v->history.PushState("Commentaire", before, d.S());
    };
    // Double click on a comment title: rename.
    if (hovered && ed.hoverCommentPart == 1 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ed.selComment = ed.hoverComment;
        ImGui::OpenPopup("##commentctx");
    }

    // Double click on a function: its graph opens in a tab.
    if (hovered && hoverNode && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        if (const Node* fn = g.Find(hoverNode); fn && fn->kind == NodeKind::Function) functionEditRequest = {v, hoverNode};
    // Layer mode: editing a link (or adding a node) turns the document into a free graph.
    bool editable = editableAtStart;
    auto toNodeMode = [&] {
        if (editable) return;
        Edit e(d, v->history, "Passer en mode nodal");
        d.ConvertToNodes();
        e.Commit();
        editable = true;
        Status("Graphe modifié : le document passe en mode nodal (Ctrl+Z pour revenir aux calques)");
    };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !spaceHeld && (hoverOut || hoverInNode)) toNodeMode();

    // Mouse interactions
    bool openAddMenu = false, openValuePopup = false;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !spaceHeld) {
        if (editable && hoverValNode) {
            // A value field: Bool toggles, numbers drag (click alone: type it), others open a small editor.
            Node* vn = g.Find(hoverValNode);
            const PinDesc pin = NodeInputPin(*vn, hoverValPin);
            d.S().activeNode = vn->id;
            ed.valNode = vn->id;
            ed.valPin = hoverValPin;
            ed.valBefore = d.S();
            if (pin.type == PinType::Bool && !pin.array) {
                Value x = NodePinDefault(*vn, hoverValPin);
                x.f.x = x.f.x != 0 ? 0.0f : 1.0f;
                NodeSetPinValue(*vn, hoverValPin, x);
                d.dirty = true;
                v->history.PushState("Valeur", *ed.valBefore, d.S());
                ed.valBefore.reset();
            } else if ((pin.type == PinType::Float || pin.type == PinType::Int) && !pin.array) {
                ed.valDrag = true;
                ed.valMoved = false;
                ed.valMouseX = mouse.x;
                ed.valStart = NodePinDefault(*vn, hoverValPin);
            } else {
                openValuePopup = true;
            }
        } else if (editable && hoverOut) {
            ed.draggingLink = true;
            ed.linkFrom = hoverOut;
            if (const Node* sn = g.Find(hoverOut)) ed.linkChannel = NodeOutputChannel(*sn, hoverOutCh);
            ed.linkTo = 0;
            ed.linkInput = -1;
        } else if (editable && hoverInNode) {
            Node* n = g.Find(hoverInNode);
            Id src = n && hoverIn < (int)n->inputs.size() ? n->inputs[hoverIn] : 0;
            u8 srcCh = n ? InputChannel(*n, hoverIn) : 0;
            if (n)  // an execution input with several flows: the last added is picked up first
                for (auto& e : n->execLinks)
                    if (e.pin == hoverIn) { src = e.src; srcCh = e.channel; }
            ed.moveBefore = d.S();
            if (src) {  // pick up the existing link
                ed.linkChannel = srcCh;
                d.Unlink(hoverInNode, hoverIn);
                ed.linkFrom = src;
                ed.linkTo = 0;
            } else {
                ed.linkFrom = 0;
                ed.linkTo = hoverInNode;
                ed.linkInput = hoverIn;
            }
            ed.draggingLink = true;
        } else if (hoverNode) {
            d.S().activeNode = hoverNode;
            if (io.KeyCtrl || io.KeyShift) {
                if (!ed.selection.erase(hoverNode)) ed.selection.insert(hoverNode);
            } else if (!ed.selection.count(hoverNode)) {
                ed.selection = {hoverNode};
            }
            if (Node* n = g.Find(hoverNode))
                if (!editable && n->layer) { d.S().activeLayer = n->layer; d.S().editMask = false; }
            // Nodes move in both modes (layer mode keeps positions over rebuilds).
            ed.dragNode = hoverNode;
            ed.moveBefore = d.S();
            auto it = std::find_if(g.nodes.begin(), g.nodes.end(), [&](const Node& n) { return n.id == hoverNode; });
            if (it != g.nodes.end() && editable) std::rotate(it, it + 1, g.nodes.end());
        } else if (editable && ed.hoverComment) {
            // Comment: the title moves it with the nodes inside, the corner resizes it.
            ed.selComment = ed.hoverComment;
            ed.commentDrag = ed.hoverCommentPart;
            ed.commentBefore = d.S();
            ed.commentNodes.clear();
            if (const GraphComment* c = findComment(ed.selComment); c && ed.commentDrag == 1)
                for (auto& n : g.nodes) {
                    const Layout& L = lay(n);
                    if (n.pos.x >= c->pos.x && n.pos.y >= c->pos.y && n.pos.x + L.w <= c->pos.x + c->size.x && n.pos.y + L.h <= c->pos.y + c->size.y)
                        ed.commentNodes.push_back(n.id);
                }
            ed.selection.clear();
        } else {
            // Empty space: rectangle selection (Ctrl/Shift adds).
            ed.selComment = 0;
            if (!io.KeyCtrl && !io.KeyShift) {
                ed.selection.clear();
                d.S().activeNode = 0;
            }
            ed.boxSelecting = true;
            ed.boxStart = toGraph(mouse);
        }
    }
    if (ed.valDrag) {
        // Horizontal drag on a number field (Shift faster, Ctrl slower).
        Node* vn = g.Find(ed.valNode);
        if (vn && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const float dx = mouse.x - ed.valMouseX;
            if (std::abs(dx) > 3) ed.valMoved = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (ed.valMoved) {
                const PinDesc pin = NodeInputPin(*vn, ed.valPin);
                // Speed follows the size of the value (0.01 per pixel around 1, 0.1 around 10...).
                const float mag = std::max(1.0f, std::abs(ed.valStart.f.x));
                const float speed = (pin.type == PinType::Int ? 0.1f : 0.01f * std::pow(10.0f, std::floor(std::log10(mag)))) *
                                    (io.KeyShift ? 10.0f : io.KeyCtrl ? 0.1f : 1.0f);
                Value x = ed.valStart;
                x.f.x = ed.valStart.f.x + dx * speed;
                if (pin.type == PinType::Int) x.f.x = std::round(x.f.x);
                NodeSetPinValue(*vn, ed.valPin, x);
                d.dirty = true;
            }
        } else {
            ed.valDrag = false;
            if (vn && ed.valMoved && ed.valBefore) v->history.PushState("Valeur", *ed.valBefore, d.S());
            else if (vn) openValuePopup = true;  // a click: type the value
            if (ed.valMoved) ed.valBefore.reset();
        }
    }
    if (ed.dragNode && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
        float2 delta = {io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
        std::set<Id> moving = ed.selection.count(ed.dragNode) ? ed.selection : std::set<Id>{ed.dragNode};
        for (Id id : moving)
            if (Node* n = g.Find(id)) {
                n->pos = n->pos + delta;
                if (!editable) d.S().layerNodePos[n->id] = n->pos;
            }
    }
    if (ed.boxSelecting) {
        ImVec2 a = toScreen(ed.boxStart);
        dl->AddRectFilled(ImVec2(std::min(a.x, mouse.x), std::min(a.y, mouse.y)), ImVec2(std::max(a.x, mouse.x), std::max(a.y, mouse.y)), IM_COL32(120, 100, 255, 40));
        dl->AddRect(ImVec2(std::min(a.x, mouse.x), std::min(a.y, mouse.y)), ImVec2(std::max(a.x, mouse.x), std::max(a.y, mouse.y)), ui::col::Accent);
    }
    if (ed.commentDrag) {
        GraphComment* c = findComment(ed.selComment);
        if (c && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const float2 delta = {io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
            if (ed.commentDrag == 1) {
                c->pos = c->pos + delta;
                for (Id id : ed.commentNodes)
                    if (Node* n = g.Find(id)) n->pos = n->pos + delta;
            } else {
                c->size = {std::max(140.0f, c->size.x + delta.x), std::max(80.0f, c->size.y + delta.y)};
            }
        } else {
            if (c && ed.commentBefore) v->history.PushState(ed.commentDrag == 1 ? "Déplacer le commentaire" : "Redimensionner le commentaire", *ed.commentBefore, d.S());
            ed.commentDrag = 0;
            ed.commentBefore.reset();
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (ed.draggingLink) {
            bool linked = false;
            if (ed.linkFrom && hoverInNode) linked = d.Link(ed.linkFrom, hoverInNode, hoverIn, ed.linkChannel);
            else if (ed.linkTo && hoverOut) {
                const Node* sn = g.Find(hoverOut);
                linked = d.Link(hoverOut, ed.linkTo, ed.linkInput, sn ? NodeOutputChannel(*sn, hoverOutCh) : 0);
            }
            else if (ed.linkFrom && hoverNode) {
                // Dropped on a node: its first free input (else the first).
                if (Node* t = g.Find(hoverNode); t && NodeInputCount(*t) > 0) {
                    int in = 0;
                    for (int i = 0; i < NodeInputCount(*t); ++i) if (i >= (int)t->inputs.size() || !t->inputs[i]) { in = i; break; }
                    linked = d.Link(ed.linkFrom, hoverNode, in, ed.linkChannel);
                }
            } else if (!hoverNode && !hoverOut && !hoverInNode && hovered) {
                // Dropped on empty space: pick a node to create, connected to this link.
                ed.pendFrom = ed.linkFrom;
                ed.pendChannel = ed.linkChannel;
                ed.pendTo = ed.linkTo;
                ed.pendInput = ed.linkInput;
                ed.contextPos = toGraph(mouse);
                openAddMenu = true;
            }
            if (!linked && ed.linkFrom && hoverInNode) Status("Lien refusé (cycle)");
            if (ed.moveBefore) v->history.PushState(linked ? "Relier" : "Délier", *ed.moveBefore, d.S());
            else if (linked) v->history.PushState("Relier", frameBefore, d.S());
            ed.draggingLink = false;
            ed.linkFrom = ed.linkTo = 0;
            ed.moveBefore.reset();
        }
        if (ed.dragNode) {
            bool moved = false;
            if (ed.moveBefore)
                for (auto& n : g.nodes)
                    if (const Node* b = ed.moveBefore->graph.Find(n.id); b && !(b->pos == n.pos)) moved = true;
            if (moved) v->history.PushState(ed.selection.size() > 1 ? "Déplacer les nœuds" : "Déplacer le nœud", *ed.moveBefore, d.S());
            ed.dragNode = 0;
            ed.moveBefore.reset();
        }
        if (ed.boxSelecting) {
            float2 p = toGraph(mouse);
            float2 mn{std::min(p.x, ed.boxStart.x), std::min(p.y, ed.boxStart.y)}, mx{std::max(p.x, ed.boxStart.x), std::max(p.y, ed.boxStart.y)};
            for (auto& n : g.nodes) {
                const Layout& L = lay(n);
                if (n.pos.x < mx.x && n.pos.x + L.w > mn.x && n.pos.y < mx.y && n.pos.y + L.h > mn.y) ed.selection.insert(n.id);
            }
            if (ed.selection.size() == 1) d.S().activeNode = *ed.selection.begin();
            ed.boxSelecting = false;
        }
    }

    auto linkPin = [&](const Node& src, int ch) { return NodeOutputPortOf(src, (u8)ch); };
    auto linkColor = [&](const Node& src, int ch, int alpha) { return TypeColor(NodeOutputType(src, NodeOutputPortOf(src, (u8)ch)), alpha); };
    // Links (colored by the type they carry)
    auto drawLink = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
        float dx = std::max(std::abs(b.x - a.x) * 0.5f, 40.0f * ed.zoom);
        dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, col, thick * ed.zoom);
    };
    for (const Node& n : g.nodes) {
        for (int i = 0; i < (int)n.inputs.size() && i < NodeInputCount(n); ++i) {
            const Node* src = n.inputs[i] ? g.Find(n.inputs[i]) : nullptr;
            if (!src) continue;
            int ch = InputChannel(n, i);
            bool sel = ed.selection.count(src->id) || ed.selection.count(n.id);
            const bool exec = NodeOutputType(*src, linkPin(*src, ch)) == PinType::Exec;
            drawLink(outPin(*src, linkPin(*src, ch)), inPin(n, i), sel ? ui::col::Selection : linkColor(*src, ch, 200), sel ? 3.0f : exec ? 3.0f : 2.0f);
        }
        for (auto& e : n.execLinks) {  // extra flows joining an execution input
            const Node* src = g.Find(e.src);
            if (!src || e.pin >= NodeInputCount(n)) continue;
            const bool sel = ed.selection.count(src->id) || ed.selection.count(n.id);
            drawLink(outPin(*src, linkPin(*src, e.channel)), inPin(n, e.pin), sel ? ui::col::Selection : linkColor(*src, e.channel, 200), 3.0f);
        }
    }
    if (ed.draggingLink) {
        if (ed.linkFrom) { if (const Node* s = g.Find(ed.linkFrom)) drawLink(outPin(*s, linkPin(*s, ed.linkChannel)), mouse, linkColor(*s, ed.linkChannel, 255), 2.5f); }
        else if (const Node* t = g.Find(ed.linkTo)) drawLink(mouse, inPin(*t, ed.linkInput), ui::col::Accent, 2.5f);
    }

    // Nodes before a baked tool are frozen (not evaluated): dimmed.
    std::unordered_set<Id> frozen;
    GraphFrozenNodes(g, frozen);
    // Nodes
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * ed.zoom;
    bool drawText = ed.zoom > 0.45f;
    for (const Node& n : g.nodes) {
        const Layout& L = lay(n);
        ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{L.w, L.h});
        if (b.x < origin.x || b.y < origin.y || a.x > origin.x + size.x || a.y > origin.y + size.y) continue;
        float r = 6 * ed.zoom;
        bool selected = ed.selection.count(n.id) || n.id == d.S().activeNode;
        bool failedNode = v->renderer.NodeFailed(n.id);
        bool isPaint = n.kind == NodeKind::Surface && n.surface == d.PaintTarget();
        dl->AddRectFilled(ImVec2(a.x + 3, a.y + 4), ImVec2(b.x + 3, b.y + 4), IM_COL32(0, 0, 0, 70), r);
        dl->AddRectFilled(a, b, n.bypass ? IM_COL32(36, 37, 42, 255) : ui::col::PanelLight, r);
        ImU32 hc = HeaderColor(n);
        if (n.bypass) hc = (hc & 0x00FFFFFF) | 0x60000000;
        dl->AddRectFilled(a, ImVec2(b.x, a.y + kHeader * ed.zoom), hc, r, ImDrawFlags_RoundCornersTop);
        ImU32 border = failedNode ? ui::col::Error : selected ? ui::col::Selection : (n.id == hoverNode ? IM_COL32(110, 110, 130, 255) : ui::col::Border);
        if (selected) dl->AddRect(ImVec2(a.x - 3, a.y - 3), ImVec2(b.x + 3, b.y + 3), IM_COL32(255, 140, 0, 90), r + 3, 0, 4.0f);
        dl->AddRect(a, b, border, r, 0, selected ? 3.0f : failedNode ? 2.0f : 1.0f);
        if (drawText) {
            std::string title = n.name;
            if (isPaint) title = "\xE2\x9C\x8E " + title;  // pencil mark on the paint target
            // Type tag on the right of the header.
            const NodeTag tag = NodeTypeTag(d, n);
            const float tfs = fs * 0.72f;
            const ImVec2 tsz = font->CalcTextSizeA(tfs, FLT_MAX, 0, tag.text);
            const float th = kHeader * ed.zoom, pad = 4 * ed.zoom;
            const ImVec2 t1(b.x - pad, a.y + th * 0.5f + tsz.y * 0.5f + pad * 0.5f), t0(t1.x - tsz.x - pad * 2, a.y + th * 0.5f - tsz.y * 0.5f - pad * 0.5f);
            dl->AddRectFilled(t0, t1, IM_COL32(16, 16, 20, 200), 3 * ed.zoom);
            dl->AddRect(t0, t1, tag.color, 3 * ed.zoom);
            dl->AddText(font, tfs, ImVec2(t0.x + pad, t0.y + pad * 0.5f), tag.color | IM_COL32(60, 60, 60, 0), tag.text);
            dl->PushClipRect(a, ImVec2(t0.x - 4, a.y + kHeader * ed.zoom), true);
            dl->AddText(font, fs, ImVec2(a.x + 8 * ed.zoom, a.y + (kHeader * ed.zoom - fs) * 0.5f), IM_COL32_WHITE, title.c_str());
            dl->PopClipRect();
        }
        // Inputs (filled with the color of the linked channel)
        for (int i = 0; i < L.inputs; ++i) {
            ImVec2 p = inPin(n, i);
            bool connected = i < (int)n.inputs.size() && n.inputs[i];
            bool hot = hoverInNode == n.id && hoverIn == i;
            float pr = pinR * (hot ? 1.4f : 1.0f);
            const PinDesc pin = NodeInputPin(n, i);
            DrawPin(dl, p, pr, pin, connected, ed.zoom);
            if (drawText) {
                dl->AddText(font, fs * 0.9f, ImVec2(p.x + 10 * ed.zoom, p.y - fs * 0.45f), ui::col::TextDim, pin.name.c_str());
                // Unlinked value input: its value field (click / drag to edit).
                ImVec2 va, vb;
                if (valRect(n, i, va, vb)) {
                    ed.valRects[((u64)n.id << 8) | (u64)i] = ImVec4(va.x, va.y, vb.x, vb.y);
                    const Value val = NodePinDefault(n, i);
                    const bool hotv = hoverValNode == n.id && hoverValPin == i;
                    const ImU32 edge = hotv ? ui::col::Accent : IM_COL32(64, 66, 78, 255);
                    const float rr = 3 * ed.zoom;
                    if (pin.array) {
                        std::string t = "[" + std::to_string(val.Size()) + "]";
                        dl->AddText(font, fs * 0.85f, ImVec2(va.x + 2 * ed.zoom, va.y + (vb.y - va.y - fs * 0.85f) * 0.5f), TypeColor(pin.type), t.c_str());
                    } else if (pin.type == PinType::Bool) {
                        const float sz = vb.y - va.y;
                        const ImVec2 c0(va.x, va.y), c1(va.x + sz, va.y + sz);
                        dl->AddRectFilled(c0, c1, IM_COL32(14, 14, 18, 255), rr);
                        dl->AddRect(c0, c1, edge, rr);
                        if (val.f.x != 0) {
                            dl->AddLine(ImVec2(c0.x + sz * 0.2f, c0.y + sz * 0.55f), ImVec2(c0.x + sz * 0.42f, c0.y + sz * 0.78f), TypeColor(PinType::Bool), 2 * ed.zoom);
                            dl->AddLine(ImVec2(c0.x + sz * 0.42f, c0.y + sz * 0.78f), ImVec2(c0.x + sz * 0.82f, c0.y + sz * 0.22f), TypeColor(PinType::Bool), 2 * ed.zoom);
                        }
                    } else if (pin.type == PinType::Color) {
                        auto toByte = [](float x) { return (int)std::lround(std::clamp(std::pow(std::max(x, 0.0f), 1.0f / 2.2f), 0.0f, 1.0f) * 255); };
                        dl->AddRectFilled(va, vb, IM_COL32(toByte(val.f.x), toByte(val.f.y), toByte(val.f.z), 255), rr);
                        dl->AddRect(va, vb, edge, rr);
                    } else {
                        dl->AddRectFilled(va, vb, IM_COL32(14, 14, 18, 255), rr);
                        dl->AddRect(va, vb, edge, rr);
                        std::string t = ValueToText(val, 1);
                        dl->PushClipRect(va, vb, true);
                        dl->AddText(font, fs * 0.8f, ImVec2(va.x + 4 * ed.zoom, va.y + (vb.y - va.y - fs * 0.8f) * 0.5f), IM_COL32(225, 225, 232, 255), t.c_str());
                        dl->PopClipRect();
                    }
                }
            }
        }
        // Outputs
        for (int o = 0; o < L.outputs; ++o) {
            ImVec2 p = outPin(n, o);
            bool hot = hoverOut == n.id && hoverOutCh == o;
            float pr = pinR * (hot ? 1.4f : 1.0f);
            DrawPin(dl, p, pr, NodeOutputPin(n, o), true, ed.zoom);
            if (drawText) {
                std::string lbl = NodeOutputName(n, o);
                ImVec2 ts = font->CalcTextSizeA(fs * 0.85f, FLT_MAX, 0, lbl.c_str());
                dl->AddText(font, fs * 0.85f, ImVec2(p.x - 10 * ed.zoom - ts.x, p.y - fs * 0.43f), LabelColor(n, o), lbl.c_str());
            }
        }
        // Thumbnail
        gpu::Texture* out = v->renderer.NodeOutput(n.id);
        u64 outHash = v->renderer.NodeHash(n.id);
        // Tiled mode (large documents) keeps no per-node images: surfaces and the
        // final image can still be previewed.
        if (!out && n.kind == NodeKind::Surface)
            if (Surface* s = d.GetSurface(n.surface)) { out = s->tex.get(); outHash = s->version; }
        if (!out && n.kind == NodeKind::Output && d.tiled) { out = v->lastImage; outHash = v->renderer.ImageVersion(); }
        ImVec2 ta = toScreen(n.pos + float2{8, kHeader + L.rows * kRow + 4});
        if (!L.image) {
            // Value / operation / execution nodes: one line of text instead of an image.
            std::string line;
            switch (n.kind) {
                case NodeKind::Value: line = ValueToText(n.value); break;
                case NodeKind::SubInput: case NodeKind::SubOutput: line = std::string(PinTypeLabel(n.value.type)) + (n.value.array ? " []" : ""); break;
                case NodeKind::VarGet: case NodeKind::VarSet:
                    if (const GraphVariable* gv = g.Var(n.op)) line = n.op + " = " + ValueToText(gv->value, 3);
                    else line = "variable inconnue : " + n.op;
                    break;
                case NodeKind::Event: line = "bouton d'exécution"; break;
                case NodeKind::Math: {
                    Value r = v->renderer.EvalValue(d, n.id, 0);
                    line = "= " + ValueToText(r, 3);
                    break;
                }
                default: break;
            }
            if (drawText && !line.empty()) {
                dl->PushClipRect(ta, ImVec2(ta.x + L.thumbW * ed.zoom, ta.y + L.thumbH * ed.zoom), true);
                dl->AddText(font, fs * 0.85f, ImVec2(ta.x + 2 * ed.zoom, ta.y + 2 * ed.zoom), ui::col::TextDim, line.c_str());
                dl->PopClipRect();
            }
            out = nullptr;
        }
        if (!out && L.image && d.tiled && ed.zoom > 0.4f)
            dl->AddText(ImVec2(ta.x + 4, ta.y + 4), ui::col::TextDim, "Aperçu indisponible\n(grand document)");
        if (out) {
            gpu::Texture* t = v->renderer.Thumbnail(*ctx, kThumbKeyBit | n.id, out, outHash, (u32)L.thumbW, (u32)L.thumbH);
            ImVec2 tb(ta.x + L.thumbW * ed.zoom, ta.y + L.thumbH * ed.zoom);
            dl->AddImageRounded((ImTextureID)gpu::Dev().UiTextureId(t), ta, tb, ImVec2(0, 0), ImVec2(1, 1), n.bypass ? IM_COL32(255, 255, 255, 90) : IM_COL32_WHITE, 3 * ed.zoom);
        }
        // Tool / function state badge: BAKED / LIVE / APPLIQUÉ / running.
        if (n.kind == NodeKind::Action && n.surface && drawText) {
            const char* st = "RÉSULTAT";
            const ImU32 sc = IM_COL32(230, 160, 40, 255);
            const float bfs = fs * 0.72f;
            const ImVec2 bs = font->CalcTextSizeA(bfs, FLT_MAX, 0, st);
            const ImVec2 b0(ta.x + 4 * ed.zoom, ta.y + 4 * ed.zoom), b1(b0.x + bs.x + 8 * ed.zoom, b0.y + bs.y + 4 * ed.zoom);
            dl->AddRectFilled(b0, b1, IM_COL32(16, 16, 20, 220), 3 * ed.zoom);
            dl->AddRect(b0, b1, sc, 3 * ed.zoom);
            dl->AddText(font, bfs, ImVec2(b0.x + 4 * ed.zoom, b0.y + 2 * ed.zoom), sc, st);
        }
        if ((n.kind == NodeKind::Tool || n.kind == NodeKind::Function) && drawText) {
            const bool busy = toolBusy.count(n.id) > 0;
            const char* st = busy                                         ? "EN COURS"
                             : n.kind == NodeKind::Function && !n.applyMode ? "LIVE"
                             : n.kind == NodeKind::Function                ? (FunctionApplied(n) ? "APPLIQUÉ" : "À APPLIQUER")
                             : ToolBaked(n)                                 ? "BAKED"
                             : n.surface                                    ? "PASSLIVE"
                                                                            : "LIVE";
            const ImU32 sc = busy ? IM_COL32(240, 200, 60, 255) : NodeBaked(n) ? IM_COL32(230, 160, 40, 255) : IM_COL32(80, 210, 120, 255);
            const float bfs = fs * 0.72f;
            const ImVec2 bs = font->CalcTextSizeA(bfs, FLT_MAX, 0, st);
            const ImVec2 b0(ta.x + 4 * ed.zoom, ta.y + 4 * ed.zoom), b1(b0.x + bs.x + 8 * ed.zoom, b0.y + bs.y + 4 * ed.zoom);
            dl->AddRectFilled(b0, b1, IM_COL32(16, 16, 20, 220), 3 * ed.zoom);
            dl->AddRect(b0, b1, sc, 3 * ed.zoom);
            dl->AddText(font, bfs, ImVec2(b0.x + 4 * ed.zoom, b0.y + 2 * ed.zoom), sc, st);
        }
        if (frozen.count(n.id)) {
            dl->AddRectFilled(a, b, IM_COL32(14, 14, 18, 150), r);
            if (drawText) dl->AddText(font, fs * 0.8f, ImVec2(a.x + 8 * ed.zoom, b.y - fs - 6 * ed.zoom), IM_COL32(230, 160, 40, 230), "figé (baké plus loin)");
        }
    }

    // Context menus
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 16.0f) {
        ed.contextPos = toGraph(mouse);
        if (hoverNode) {
            d.S().activeNode = hoverNode;
            if (!ed.selection.count(hoverNode)) ed.selection = {hoverNode};
            ImGui::OpenPopup("##nodectx");
        } else if (ed.hoverComment) {
            ed.selComment = ed.hoverComment;
            ImGui::OpenPopup("##commentctx");
        } else {
            ed.pendFrom = ed.pendTo = 0;
            openAddMenu = true;
        }
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && ImGui::IsItemActive())
        ed.pan = ed.pan + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
    if (openAddMenu) {
        ed.search[0] = 0;
        ImGui::OpenPopup("##addnode");
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(260 * dpi, 0), ImVec2(360 * dpi, 520 * dpi));
    if (ImGui::BeginPopup("##addnode")) {
        ImGui::TextDisabled(ed.pendFrom ? "Nouveau nœud relié à cette sortie" : ed.pendTo ? "Nouveau nœud relié à cette entrée" : "Ajouter un nœud");
        if (!ed.pendFrom && !ed.pendTo && editable && ImGui::SmallButton("+ Commentaire ici")) {
            const auto keep = ed.selection;
            ed.selection.clear();
            addComment(ed.contextPos);
            ed.selection = keep;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool enter = ImGui::InputTextWithHint("##search", "Rechercher...", ed.search, sizeof(ed.search), ImGuiInputTextFlags_EnterReturnsTrue);
        std::string q = Lower(ed.search);
        static std::vector<NodeEntry> entries;
        static u64 entriesGen = ~0ull;
        if (entriesGen != EntriesGeneration()) { entries = NodeEntries(); entriesGen = EntriesGeneration(); }
        const NodeEntry* picked = nullptr;
        ImGui::BeginChild("##nodelist", ImVec2(0, 380 * dpi), ImGuiChildFlags_None);
        if (!q.empty()) {
            // Flat list of matches; Enter takes the first.
            const NodeEntry* first = nullptr;
            for (auto& e : entries) {
                if (Lower(e.label + " " + e.category + " " + e.keywords).find(q) == std::string::npos) continue;
                if (!first) first = &e;
                if (ImGui::Selectable((e.label + "##" + e.category).c_str())) picked = &e;
                ImGui::SameLine();
                ImGui::TextDisabled("%s", e.category.c_str());
            }
            if (!first) ImGui::TextDisabled("Aucun nœud ne correspond.");
            if (enter && first) picked = first;
        } else {
            // Tree by category.
            std::vector<std::string> cats;
            for (auto& e : entries) if (std::find(cats.begin(), cats.end(), e.category) == cats.end()) cats.push_back(e.category);
            for (auto& c : cats) {
                if (!ImGui::TreeNodeEx(c.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) continue;
                for (auto& e : entries)
                    if (e.category == c && ImGui::Selectable(e.label.c_str())) picked = &e;
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();
        if (picked) {
            toNodeMode();
            DocState before = d.S();
            Id nid = picked->make(d, ed.contextPos);
            if (Node* nn = g.Find(nid)) {
                if (ed.pendFrom && NodeInputCount(*nn) > 0) d.Link(ed.pendFrom, nid, 0, ed.pendChannel);
                if (ed.pendTo) d.Link(nid, ed.pendTo, ed.pendInput, 0);
            }
            d.S().activeNode = nid;
            ed.selection = {nid};
            ed.pendFrom = ed.pendTo = 0;
            v->history.PushState("Ajouter un nœud", before, d.S());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    auto deleteSelection = [&] {
        DocState before = d.S();
        int n = 0;
        for (Id id : ed.selection)
            if (id != g.output && g.Find(id)) { d.RemoveNode(id); ++n; }
        if (!n && d.S().activeNode && d.S().activeNode != g.output) { d.RemoveNode(d.S().activeNode); ++n; }
        ed.selection.clear();
        if (n) v->history.PushState(n > 1 ? "Supprimer les nœuds" : "Supprimer le nœud", before, d.S());
    };
    // Value of a node input, edited in place (opened by a click on its field).
    if (openValuePopup) {
        ImGui::OpenPopup("##pinval");
        ed.valChanged = false;
    }
    if (ImGui::BeginPopup("##pinval")) {
        if (Node* vn = g.Find(ed.valNode); vn && ed.valPin >= 0 && ed.valPin < NodeInputCount(*vn)) {
            const PinDesc pin = NodeInputPin(*vn, ed.valPin);
            ImGui::TextDisabled("%s  -  %s%s", vn->name.c_str(), PinTypeLabel(pin.type), pin.array ? " (tableau)" : "");
            Value x = NodePinDefault(*vn, ed.valPin);
            if (ImGui::IsWindowAppearing() && !pin.array && pin.type != PinType::Color) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(220 * dpi);
            if (pin.type == PinType::Color && !pin.array) {
                if (ImGui::ColorPicker4(pin.name.c_str(), &x.f.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar)) {
                    NodeSetPinValue(*vn, ed.valPin, x);
                    d.dirty = ed.valChanged = true;
                }
            } else if (ValueEditor(pin.name.c_str(), x)) {
                NodeSetPinValue(*vn, ed.valPin, x);
                d.dirty = ed.valChanged = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) ImGui::CloseCurrentPopup();
        } else {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        ed.valPopup = true;
    } else if (ed.valPopup) {
        ed.valPopup = false;
        if (ed.valChanged && ed.valBefore) v->history.PushState("Valeur", *ed.valBefore, d.S());
        ed.valBefore.reset();
        ed.valChanged = false;
    }
    if (ImGui::BeginPopup("##commentctx")) {
        if (GraphComment* c = findComment(ed.selComment)) {
            static DocState snap;
            if (ImGui::IsWindowAppearing()) {
                snap = d.S();
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(260 * dpi);
            ImGui::InputText("##ctext", &c->text);
            if (ImGui::IsItemDeactivatedAfterEdit()) v->history.PushState("Renommer le commentaire", snap, d.S());
            static const float4 kColors[] = {{0.35f, 0.45f, 0.75f, 1}, {0.30f, 0.60f, 0.40f, 1}, {0.75f, 0.55f, 0.20f, 1},
                                             {0.70f, 0.30f, 0.35f, 1}, {0.55f, 0.35f, 0.70f, 1}, {0.40f, 0.42f, 0.46f, 1}};
            for (int k = 0; k < 6; ++k) {
                if (k) ImGui::SameLine();
                ImGui::PushID(k);
                if (ImGui::ColorButton("##col", ImVec4(kColors[k].x, kColors[k].y, kColors[k].z, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(22 * dpi, 22 * dpi))) {
                    DocState before = d.S();
                    c->color = kColors[k];
                    v->history.PushState("Couleur du commentaire", before, d.S());
                }
                ImGui::PopID();
            }
            if (ImGui::MenuItem("Ajuster aux nœuds contenus")) {
                float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
                bool any = false;
                for (auto& n : g.nodes) {
                    const Layout& L = lay(n);
                    if (n.pos.x >= c->pos.x && n.pos.y >= c->pos.y && n.pos.x + L.w <= c->pos.x + c->size.x && n.pos.y + L.h <= c->pos.y + c->size.y) {
                        mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
                        mx = {std::max(mx.x, n.pos.x + L.w), std::max(mx.y, n.pos.y + L.h)};
                        any = true;
                    }
                }
                if (any) {
                    DocState before = d.S();
                    c->pos = mn - float2{30, 30 + kCommentHeader};
                    c->size = (mx - mn) + float2{60, 60 + kCommentHeader};
                    v->history.PushState("Ajuster le commentaire", before, d.S());
                }
            }
            if (ImGui::MenuItem("Supprimer le commentaire (garde les nœuds)")) {
                DocState before = d.S();
                const Id id = c->id;
                g.comments.erase(std::remove_if(g.comments.begin(), g.comments.end(), [&](const GraphComment& x) { return x.id == id; }), g.comments.end());
                ed.selComment = 0;
                v->history.PushState("Supprimer le commentaire", before, d.S());
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) ImGui::CloseCurrentPopup();
        } else {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    // Search in the graph (Ctrl+F): jump to a node.
    if (focused && !io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) {
        ed.findText[0] = 0;
        ImGui::OpenPopup("##graphsearch");
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(300 * dpi, 0), ImVec2(420 * dpi, 460 * dpi));
    if (ImGui::BeginPopup("##graphsearch")) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool enter = ImGui::InputTextWithHint("##find", "Chercher un nœud, un commentaire...", ed.findText, sizeof(ed.findText), ImGuiInputTextFlags_EnterReturnsTrue);
        const std::string q = Lower(ed.findText);
        Id pick = 0, first = 0;
        float2 pickPos{};
        bool pickComment = false;
        ImGui::BeginChild("##findres", ImVec2(0, 300 * dpi));
        for (auto& c : g.comments)
            if (q.empty() || Lower(c.text).find(q) != std::string::npos) {
                if (!first) first = c.id, pickComment = true;
                if (ImGui::Selectable(("\xE2\x96\xA2 " + c.text + "##c" + std::to_string(c.id)).c_str())) pick = c.id, pickComment = true;
            }
        for (auto& n : g.nodes) {
            const std::string tag = NodeTypeTag(d, n).text;
            if (!q.empty() && Lower(n.name + " " + tag + " " + n.op + " " + n.fx.effectId).find(q) == std::string::npos) continue;
            if (!first) first = n.id, pickComment = false;
            if (ImGui::Selectable((n.name + "##n" + std::to_string(n.id)).c_str())) pick = n.id, pickComment = false;
            ImGui::SameLine();
            ImGui::TextDisabled("%s", tag.c_str());
        }
        ImGui::EndChild();
        if (enter && !pick) pick = first;
        if (pick) {
            if (pickComment) {
                ed.selComment = pick;
                if (const GraphComment* c = findComment(pick)) {
                    ed.zoom = std::clamp(std::min(size.x / (c->size.x + 80), size.y / (c->size.y + 80)), 0.2f, 1.5f);
                    ed.pan = float2{size.x / ed.zoom * 0.5f, size.y / ed.zoom * 0.5f} - (c->pos + c->size * 0.5f);
                }
            } else {
                ed.selection = {pick};
                d.S().activeNode = pick;
                graphFrameOnly = {pick};
                graphFrameRequest = true;
            }
            ImGui::CloseCurrentPopup();
        }
        (void)pickPos;
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##nodectx")) {
        Node* n = g.Find(d.S().activeNode);
        if (n) {
            ImGui::TextDisabled("%s%s", n->name.c_str(), ed.selection.size() > 1 ? "  (+ sélection)" : "");
            if (editable && n->kind != NodeKind::Output) {
                if (ImGui::MenuItem("Désactiver (bypass)", nullptr, n->bypass)) {
                    DocState before = d.S();
                    n->bypass = !n->bypass;
                    v->history.PushState("Bypass", before, d.S());
                }
                if (ImGui::MenuItem("Dupliquer")) {
                    DocState before = d.S();
                    Node copy = *n;
                    copy.id = d.NewId();
                    copy.pos = copy.pos + float2{30, 30};
                    copy.layer = 0;
                    if (copy.kind == NodeKind::Surface) {
                        Id src = copy.surface;
                        copy.surface = d.CreateSurface({0, 0, 0, 0});
                        d.pendingCopies.push_back({copy.surface, src});
                    }
                    g.nodes.push_back(copy);
                    d.S().activeNode = copy.id;
                    ed.selection = {copy.id};
                    v->history.PushState("Dupliquer le nœud", before, d.S());
                }
                if (n->kind == NodeKind::Event && !v->funcLink.parent && ImGui::MenuItem(("Exécuter : " + n->name).c_str(), nullptr, false, !execRun))
                    StartExec(*v, 0, n->id);
                if (n->kind == NodeKind::Action && n->surface && ImGui::MenuItem("Effacer le résultat")) {
                    DocState before = d.S();
                    n->surface = 0;
                    n->smart = 0;
                    v->history.PushState("Effacer le résultat", before, d.S());
                }
                if (n->kind == NodeKind::Function && n->sub) {
                    const Id fid = n->id;
                    for (auto& m : n->sub->nodes)
                        if (m.kind == NodeKind::Event && ImGui::MenuItem(("Exécuter : " + m.name).c_str(), nullptr, false, !execRun)) StartExec(*v, fid, m.id);
                }
                if (n->kind == NodeKind::Function) {
                    ImGui::Separator();
                    const Id fid = n->id;
                    if (ImGui::MenuItem("Modifier la fonction", "double-clic")) EditFunction(*v, fid);
                    if (ImGui::MenuItem("Mode Apply (sinon Live)", nullptr, n->applyMode)) RequestToolAction(*v, fid, ToolAction::ToggleLive);
                    if (n->applyMode && ImGui::MenuItem(FunctionApplied(*n) ? "Réappliquer" : "Appliquer")) RequestToolAction(*v, fid, ToolAction::Bake);
                    if (ImGui::MenuItem("Convertir en image", nullptr, false, FunctionApplied(*n))) RequestToolAction(*v, fid, ToolAction::ConvertToImage);
                    if (ImGui::MenuItem("Enregistrer dans la bibliothèque")) SaveFunctionToLibrary(*v, fid);
                    ImGui::Separator();
                }
                if (n->kind != NodeKind::Output) {
                    std::set<Id> selNodes = ed.selection;
                    if (selNodes.empty()) selNodes = {n->id};
                    if (ImGui::MenuItem("Créer un sous-graphe depuis la sélection")) { FunctionFromSelection(*v, selNodes, "subgraph"); ed.selection.clear(); }
                    if (ImGui::MenuItem("Créer un outil depuis la sélection")) { FunctionFromSelection(*v, selNodes, "tool"); ed.selection.clear(); }
                    n = g.Find(d.S().activeNode);
                }
                if (n && n->kind == NodeKind::Tool) {
                    ImGui::Separator();
                    const Id tid = n->id;
                    if (ImGui::MenuItem(n->surface ? "Rebaker" : "Baker", nullptr, false, !toolBusy.count(tid))) RequestToolAction(*v, tid, ToolAction::Bake);
                    if (ImGui::MenuItem("PassLive (entrée en direct)", nullptr, n->passLive, n->surface != 0)) RequestToolAction(*v, tid, ToolAction::ToggleLive);
                    if (ImGui::MenuItem("Convertir en image", nullptr, false, n->surface != 0)) RequestToolAction(*v, tid, ToolAction::ConvertToImage);
                    if (ImGui::MenuItem("Exporter l'image...", nullptr, false, n->surface != 0)) RequestToolAction(*v, tid, ToolAction::Export);
                    ImGui::Separator();
                }
                if (ImGui::MenuItem("Commentaire autour de la sélection", "C")) addComment(ed.contextPos);
                if (ed.selection.size() > 1 && ImGui::BeginMenu("Aligner")) {
                    std::vector<Node*> sel;
                    for (Id id : ed.selection) if (Node* s = g.Find(id)) sel.push_back(s);
                    auto apply = [&](const char* label, auto fn) {
                        if (!ImGui::MenuItem(label)) return;
                        DocState before = d.S();
                        fn();
                        v->history.PushState(label, before, d.S());
                    };
                    apply("À gauche", [&] { float x = 1e9f; for (Node* s : sel) x = std::min(x, s->pos.x); for (Node* s : sel) s->pos.x = x; });
                    apply("En haut", [&] { float y = 1e9f; for (Node* s : sel) y = std::min(y, s->pos.y); for (Node* s : sel) s->pos.y = y; });
                    apply("Centres verticaux (même colonne)", [&] {
                        float c = 0;
                        for (Node* s : sel) c += s->pos.x + lay(*s).w * 0.5f;
                        c /= sel.size();
                        for (Node* s : sel) s->pos.x = c - lay(*s).w * 0.5f;
                    });
                    apply("Répartir horizontalement", [&] {
                        std::sort(sel.begin(), sel.end(), [](Node* a, Node* b) { return a->pos.x < b->pos.x; });
                        float x = sel.front()->pos.x;
                        for (Node* s : sel) { s->pos.x = x; x += lay(*s).w + 60; }
                    });
                    apply("Répartir verticalement", [&] {
                        std::sort(sel.begin(), sel.end(), [](Node* a, Node* b) { return a->pos.y < b->pos.y; });
                        float y = sel.front()->pos.y;
                        for (Node* s : sel) { s->pos.y = y; y += lay(*s).h + 40; }
                    });
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(ed.selection.size() > 1 ? "Supprimer la sélection" : "Supprimer", "Suppr")) deleteSelection();
            } else if (!editable) {
                ImGui::TextDisabled("Mode calques : passez en mode nodal pour modifier ce nœud ici");
            }
        }
        ImGui::EndPopup();
    }

    // Node palette: drag and drop on the canvas, or click (added at the center of the view).
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) paletteFocus = 0;
    if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType(kPalettePayload) && ((const char*)pl->Data)[0] == '0' &&
        ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + size.x, origin.y + size.y))) {
        if (ImGui::BeginDragDropTargetCustom(ImRect(origin, ImVec2(origin.x + size.x, origin.y + size.y)), ImGui::GetID("##paldrop"))) {
            if (const ImGuiPayload* got = ImGui::AcceptDragDropPayload(kPalettePayload)) {
                paletteAdd.pending = true;
                paletteAdd.target = 0;
                paletteAdd.key = (const char*)got->Data + 1;
                paletteAdd.hasPos = true;
                paletteAdd.screenPos = mouse;
            }
            ImGui::EndDragDropTarget();
        }
    }
    if (paletteAdd.pending && paletteAdd.target == 0) {
        paletteAdd.pending = false;
        static std::vector<NodeEntry> entries;
        static u64 gen = ~0ull;
        if (gen != EntriesGeneration()) { entries = NodeEntries(); gen = EntriesGeneration(); }
        for (auto& e : entries) {
            if (e.label + "\x1f" + e.category != paletteAdd.key) continue;
            float2 at = paletteAdd.hasPos ? toGraph(paletteAdd.screenPos) : toGraph(ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f));
            at = at - float2{kNodeW * 0.5f, 20};
            toNodeMode();
            DocState before = d.S();
            Id nid = e.make(d, at);
            d.S().activeNode = nid;
            ed.selection = {nid};
            v->history.PushState("Ajouter un nœud", before, d.S());
            break;
        }
    }

    // Effect dropped from the effects browser: on a layer node -> filter of that
    // layer, on a link -> inserted into it, elsewhere -> new node.
    if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType(kEffectPayload) &&
        ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + size.x, origin.y + size.y))) {
        Id overNode = 0;
        for (int k = (int)g.nodes.size() - 1; k >= 0 && !overNode; --k) {
            const Node& n = g.nodes[k];
            ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{lay(n).w, lay(n).h});
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) overNode = n.id;
        }
        Id linkSrc = 0, linkDst = 0;
        int linkIn = -1;
        u8 linkCh = 0;
        if (!overNode) {
            float best = 9.0f * dpi;
            for (const Node& n : g.nodes)
                for (int i = 0; i < (int)n.inputs.size() && i < NodeInputCount(n); ++i) {
                    const Node* src = n.inputs[i] ? g.Find(n.inputs[i]) : nullptr;
                    if (!src) continue;
                    int ch = InputChannel(n, i);
                    ImVec2 a = outPin(*src, linkPin(*src, ch)), b = inPin(n, i);
                    float dx = std::max(std::abs(b.x - a.x) * 0.5f, 40.0f * ed.zoom);
                    for (int t = 0; t <= 24; ++t) {
                        ImVec2 p = ImBezierCubicCalc(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, t / 24.0f);
                        float dd = std::sqrt((p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y));
                        if (dd < best) { best = dd; linkSrc = src->id; linkDst = n.id; linkIn = i; linkCh = (u8)ch; }
                    }
                }
        }
        const Node* on = g.Find(overNode);
        Id layerOf = on && !editable ? on->layer : 0;
        // Feedback.
        if (on) {
            const Layout& L = lay(*on);
            dl->AddRect(toScreen(on->pos), toScreen(on->pos + float2{L.w, L.h}), ui::col::Accent2, 6 * ed.zoom, 0, 3.0f);
        } else if (const Node* s2 = g.Find(linkSrc); s2 && g.Find(linkDst)) {
            drawLink(outPin(*s2, linkPin(*s2, linkCh)), inPin(*g.Find(linkDst), linkIn), ui::col::Accent2, 4.0f);
        }
        const char* what = layerOf ? "Filtre sur ce calque" : on ? "Insérer après ce nœud" : linkSrc ? "Insérer dans le lien" : "Nouveau nœud";
        ImVec2 ts = ImGui::CalcTextSize(what);
        dl->AddRectFilled(ImVec2(mouse.x + 14, mouse.y - 22), ImVec2(mouse.x + 22 + ts.x, mouse.y - 18 + ts.y), IM_COL32(20, 20, 26, 230), 4);
        dl->AddText(ImVec2(mouse.x + 18, mouse.y - 20), ui::col::Accent2, what);
        if (ImGui::BeginDragDropTargetCustom(ImRect(origin, ImVec2(origin.x + size.x, origin.y + size.y)), ImGui::GetID("##graphdrop"))) {
            if (const ImGuiPayload* got = ImGui::AcceptDragDropPayload(kEffectPayload)) {
                std::string id((const char*)got->Data);
                float2 at = toGraph(mouse) - float2{kNodeW * 0.5f, 20};
                if (layerOf) {
                    DropEffectOnLayer(*v, layerOf, id);
                } else if (Effects().Find(id)) {
                    toNodeMode();
                    Edit e(d, v->history, "Ajouter un effet");
                    Id nid = d.AddEffectNode(id, "", at).id;
                    if (overNode && overNode != g.output) {
                        // After the node: its consumers now read the effect.
                        Id src = overNode;
                        for (auto& n : g.nodes)
                            for (auto& in : n.inputs) if (in == src && n.id != nid) in = nid;
                        d.Link(src, nid, 0);
                    } else if (linkSrc) {
                        d.Link(linkSrc, nid, 0, linkCh);
                        d.Link(nid, linkDst, linkIn);
                    }
                    d.S().activeNode = nid;
                    ed.selection = {nid};
                    e.Commit();
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    // Keyboard
    if (focused && editable && !io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        if (ed.selComment && ed.selection.empty()) {
            DocState before = d.S();
            const Id id = ed.selComment;
            g.comments.erase(std::remove_if(g.comments.begin(), g.comments.end(), [&](const GraphComment& x) { return x.id == id; }), g.comments.end());
            ed.selComment = 0;
            v->history.PushState("Supprimer le commentaire", before, d.S());
        } else {
            deleteSelection();
        }
    }
    if (focused && editable && !io.WantTextInput && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !ed.selection.empty()) addComment(ed.contextPos);
    if (focused && !io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) for (auto& n : g.nodes) ed.selection.insert(n.id);
    if (((focused && !io.WantTextInput && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) || std::exchange(graphFrameRequest, false)) && !g.nodes.empty()) {
        float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
        for (auto& n : g.nodes) {
            if (!graphFrameOnly.empty() ? !graphFrameOnly.count(n.id) : (!ed.selection.empty() && !ed.selection.count(n.id))) continue;  // F frames the selection when there is one
            mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
            mx = {std::max(mx.x, n.pos.x + lay(n).w), std::max(mx.y, n.pos.y + lay(n).h)};
        }
        float2 ext = mx - mn;
        ed.zoom = std::clamp(std::min(size.x / (ext.x + 80), size.y / (ext.y + 80)), 0.2f, 1.5f);
        ed.pan = float2{size.x / ed.zoom * 0.5f, size.y / ed.zoom * 0.5f + 18.0f / ed.zoom} - (mn + ext * 0.5f);  // below the banner
        graphFrameOnly.clear();
    }

    // Header banner
    ImVec2 bp(origin.x + 10, origin.y + 8);
    if (!editable) {
        const char* msg = "Mode calques : graphe généré depuis les calques. Modifier un lien ou ajouter un nœud passe en mode nodal.";
        dl->AddRectFilled(ImVec2(bp.x - 6, bp.y - 4), ImVec2(bp.x + ImGui::CalcTextSize(msg).x + 190 * dpi, bp.y + ImGui::GetFrameHeight()), IM_COL32(20, 20, 26, 220), 5);
        dl->AddText(ImVec2(bp.x, bp.y + 3), ui::col::TextDim, msg);
        ImGui::SetCursorScreenPos(ImVec2(bp.x + ImGui::CalcTextSize(msg).x + 10, bp.y));
        if (ImGui::SmallButton("Passer en mode nodal")) {
            Edit e(d, v->history, "Convertir en nœuds");
            d.ConvertToNodes();
            e.Commit();
        }
        ui::HelpTooltip("Le graphe devient libre : relier, séparer les canaux R V B A, ajouter des nœuds.");
    } else if (v->funcLink.parent) {
        // Inside a function: where we are (document > function > inner function) and the way back.
        std::vector<std::string> crumbs;
        for (DocView* c = v; c;) {
            if (c->funcLink.parent && ViewAlive(c->funcLink.parent)) {
                const Node* fn = c->funcLink.parent->doc->S().graph.Find(c->funcLink.node);
                crumbs.insert(crumbs.begin(), fn ? fn->name : std::string("?"));
                c = c->funcLink.parent;
            } else {
                crumbs.insert(crumbs.begin(), c->doc->Name());
                break;
            }
        }
        std::string path;
        for (size_t k = 0; k < crumbs.size(); ++k) path += (k ? "  ›  " : "") + crumbs[k];
        const char* hint = "chaque modification met à jour le nœud  |  Entrée / Sortie de fonction = ses broches";
        const float bw = ImGui::CalcTextSize("< Retour").x + ImGui::GetStyle().FramePadding.x * 2;
        const float pw = ImGui::CalcTextSize(path.c_str()).x, hw = ImGui::CalcTextSize(hint).x;
        const float sw = ImGui::CalcTextSize("Enregistrer dans la bibliothèque").x + ImGui::GetStyle().FramePadding.x * 2;
        const float total = bw + 12 + pw + 16 + hw + 16 + sw;
        dl->AddRectFilled(ImVec2(bp.x - 6, bp.y - 4), ImVec2(bp.x + total + 6, bp.y + ImGui::GetFrameHeight() + 2), IM_COL32(52, 30, 66, 240), 5);
        dl->AddRect(ImVec2(bp.x - 6, bp.y - 4), ImVec2(bp.x + total + 6, bp.y + ImGui::GetFrameHeight() + 2), IM_COL32(170, 90, 200, 255), 5);
        ImGui::SetCursorScreenPos(bp);
        if (ImGui::SmallButton("< Retour")) {
            for (int k = 0; k < (int)docs.size(); ++k)
                if (docs[k].get() == v->funcLink.parent) activeDoc = focusDocRequest = k;
        }
        ui::HelpTooltip("Revenir au graphe qui contient cette fonction (l'onglet reste ouvert).");
        dl->AddText(ImVec2(bp.x + bw + 12, bp.y + 3), IM_COL32(240, 210, 255, 255), path.c_str());
        dl->AddText(ImVec2(bp.x + bw + 12 + pw + 16, bp.y + 3), IM_COL32(190, 170, 205, 255), hint);
        ImGui::SetCursorScreenPos(ImVec2(bp.x + total - sw, bp.y));
        if (ImGui::SmallButton("Enregistrer dans la bibliothèque")) SaveFunctionToLibrary(*v->funcLink.parent, v->funcLink.node);
    } else {
        const char* hint = "Clic droit : nœuds  |  tirer un lien dans le vide : nouveau nœud relié  |  rectangle / Ctrl+clic : sélection  |  F : cadrer  |  Suppr";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        dl->AddRectFilled(ImVec2(bp.x - 6, bp.y - 4), ImVec2(bp.x + ts.x + 6, bp.y + ts.y + 4), IM_COL32(20, 20, 26, 220), 5);
        dl->AddText(ImVec2(bp.x, bp.y), ui::col::TextDim, hint);
    }
    dl->PopClipRect();
    ImGui::End();
}

}  // namespace sx
