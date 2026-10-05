// Node graph editor (custom ImDrawList widget): pan/zoom, links, live thumbnails.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/EffectGraph.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sx {

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
// generators, text, shapes), Blend, Effect (input -> output), Transform, SubGraph (a graph inside:
// group, smart object holding a document), plus the channel split and the output.
struct NodeTag { const char* text; ImU32 color; };
NodeTag NodeTypeTag(Document& d, const Node& n) {
    const NodeTag image = {"IMAGE", IM_COL32(36, 150, 168, 255)}, draw = {"DRAW", IM_COL32(210, 130, 50, 255)},
                  blend = {"BLEND", IM_COL32(64, 104, 200, 255)}, effect = {"EFFECT", IM_COL32(112, 84, 220, 255)},
                  xform = {"TRANSFORM", IM_COL32(220, 170, 40, 255)}, sub = {"SUBGRAPH", IM_COL32(170, 90, 200, 255)};
    Layer* l = n.layer ? d.FindLayer(n.layer) : nullptr;
    switch (n.kind) {
        case NodeKind::Output: return {"SORTIE", IM_COL32(52, 160, 110, 255)};
        case NodeKind::Split: return {"CANAUX", IM_COL32(70, 150, 90, 255)};
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
            if (n.fx.effectId == "transform_2d") return xform;
            auto def = n.fx.Def();
            if ((def && def->inputs.empty()) || (l && l->kind == LayerKind::Fill)) return draw;
            return effect;
        }
    }
    return effect;
}

// Pins and links are colored by type (Unreal-style): RGBA image pink, single channel green.
ImU32 TypeColor(PinType t, int alpha = 255) {
    return t == PinType::Channel ? IM_COL32(110, 210, 120, alpha) : IM_COL32(230, 120, 210, alpha);
}
// Labels of the Split outputs keep their channel color.
ImU32 LabelColor(const Node& n, int k) {
    if (n.kind != NodeKind::Split) return TypeColor(PinType::RGBA, 230);
    static const ImU32 c[] = {IM_COL32(235, 90, 90, 255), IM_COL32(100, 215, 110, 255), IM_COL32(90, 150, 255, 255), IM_COL32(210, 210, 215, 255)};
    return c[std::clamp(k, 0, 3)];
}

struct Layout {
    float h;
    float thumbW, thumbH;
    int inputs, outputs, rows;
};

Layout NodeLayout(const Node& n, float aspect) {
    Layout l;
    l.inputs = NodeInputCount(n);
    l.outputs = NodeOutputCount(n);
    l.rows = std::max({l.inputs, l.outputs, 1});
    l.thumbW = kNodeW - 16;
    l.thumbH = std::clamp(l.thumbW / std::max(aspect, 0.1f), 40.0f, l.thumbW);
    l.h = kHeader + l.rows * kRow + l.thumbH + 12;
    return l;
}

// Node menu entries (tree by category).
struct NodeEntry {
    std::string category, label, keywords;
    std::function<Id(Document&, float2)> make;
};

std::vector<NodeEntry> NodeEntries() {
    std::vector<NodeEntry> e;
    e.push_back({"Canaux", "Séparer RGBA (R, V, B, A)", "split channels canal", [](Document& d, float2 p) { return d.AddNode(NodeKind::Split, p).id; }});
    e.push_back({"Calques", "Calque pixel", "surface peinture", [](Document& d, float2 p) { return d.AddNode(NodeKind::Surface, p).id; }});
    e.push_back({"Fusion", "Fusion (mode de fusion)", "blend mélange", [](Document& d, float2 p) { return d.AddNode(NodeKind::Blend, p).id; }});
    e.push_back({"Fusion", "Fusion personnalisée (A, B, masque)", "blend custom hlsl mélange",
                 [](Document& d, float2 p) { return d.AddEffectNode("", kCustomBlendTemplate, p).id; }});
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
        e.push_back({def->category, def->name, id, [id](Document& d, float2 p) { return d.AddEffectNode(id, "", p).id; }});
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
        if (gen != Effects().Generation()) { entries = NodeEntries(); gen = Effects().Generation(); }
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

    // Geometry
    auto outPin = [&](const Node& n, int k) { return toScreen(n.pos + float2{kNodeW, kHeader + kRow * (k + 0.5f)}); };
    auto inPin = [&](const Node& n, int i) { return toScreen(n.pos + float2{0, kHeader + kRow * (i + 0.5f)}); };
    float pinR = kPin * ed.zoom;
    ImVec2 mouse = io.MousePos;
    auto isNear = [&](ImVec2 a, ImVec2 b, float r) { float dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy <= r * r; };

    // Hit testing (topmost = last)
    Id hoverNode = 0, hoverOut = 0, hoverInNode = 0;
    int hoverIn = -1, hoverOutCh = 0;
    if (hovered) {
        for (int k = (int)g.nodes.size() - 1; k >= 0; --k) {
            const Node& n = g.nodes[k];
            Layout L = NodeLayout(n, aspect);
            bool pinHit = false;
            for (int o = 0; o < L.outputs && !pinHit; ++o)
                if (isNear(mouse, outPin(n, o), pinR * 2.0f)) { hoverOut = n.id; hoverOutCh = o; pinHit = true; }
            for (int i = 0; i < L.inputs && !pinHit; ++i)
                if (isNear(mouse, inPin(n, i), pinR * 2.0f)) { hoverInNode = n.id; hoverIn = i; pinHit = true; }
            if (pinHit) break;
            ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{kNodeW, L.h});
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) { hoverNode = n.id; break; }
        }
    }

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
    bool openAddMenu = false;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !spaceHeld) {
        if (editable && hoverOut) {
            ed.draggingLink = true;
            ed.linkFrom = hoverOut;
            if (const Node* sn = g.Find(hoverOut)) ed.linkChannel = NodeOutputChannel(*sn, hoverOutCh);
            ed.linkTo = 0;
            ed.linkInput = -1;
        } else if (editable && hoverInNode) {
            Node* n = g.Find(hoverInNode);
            Id src = n && hoverIn < (int)n->inputs.size() ? n->inputs[hoverIn] : 0;
            ed.moveBefore = d.S();
            if (src) {  // pick up the existing link
                ed.linkChannel = InputChannel(*n, hoverIn);
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
        } else {
            // Empty space: rectangle selection (Ctrl/Shift adds).
            if (!io.KeyCtrl && !io.KeyShift) {
                ed.selection.clear();
                d.S().activeNode = 0;
            }
            ed.boxSelecting = true;
            ed.boxStart = toGraph(mouse);
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
                Layout L = NodeLayout(n, aspect);
                if (n.pos.x < mx.x && n.pos.x + kNodeW > mn.x && n.pos.y < mx.y && n.pos.y + L.h > mn.y) ed.selection.insert(n.id);
            }
            if (ed.selection.size() == 1) d.S().activeNode = *ed.selection.begin();
            ed.boxSelecting = false;
        }
    }

    auto linkPin = [&](const Node& src, int ch) { return src.kind == NodeKind::Split && ch > 0 ? ch - 1 : 0; };
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
            drawLink(outPin(*src, linkPin(*src, ch)), inPin(n, i), sel ? ui::col::Selection : TypeColor(ch ? PinType::Channel : PinType::RGBA, 200), sel ? 3.0f : 2.0f);
        }
    }
    if (ed.draggingLink) {
        if (ed.linkFrom) { if (const Node* s = g.Find(ed.linkFrom)) drawLink(outPin(*s, linkPin(*s, ed.linkChannel)), mouse, TypeColor(ed.linkChannel ? PinType::Channel : PinType::RGBA), 2.5f); }
        else if (const Node* t = g.Find(ed.linkTo)) drawLink(mouse, inPin(*t, ed.linkInput), ui::col::Accent, 2.5f);
    }

    // Nodes
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * ed.zoom;
    bool drawText = ed.zoom > 0.45f;
    for (const Node& n : g.nodes) {
        Layout L = NodeLayout(n, aspect);
        ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{kNodeW, L.h});
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
            ImU32 tc = TypeColor(NodeInputType(n, i));
            if (connected) {
                dl->AddCircleFilled(p, pr, tc);
                dl->AddCircle(p, pr, IM_COL32(20, 20, 24, 255), 0, 1.5f);
            } else {
                dl->AddCircleFilled(p, pr, IM_COL32(40, 42, 50, 255));
                dl->AddCircle(p, pr, tc, 0, 2.0f * ed.zoom);
            }
            if (drawText) dl->AddText(font, fs * 0.9f, ImVec2(p.x + 10 * ed.zoom, p.y - fs * 0.45f), ui::col::TextDim, NodeInputName(n, i).c_str());
        }
        // Outputs
        for (int o = 0; o < L.outputs; ++o) {
            ImVec2 p = outPin(n, o);
            bool hot = hoverOut == n.id && hoverOutCh == o;
            float pr = pinR * (hot ? 1.4f : 1.0f);
            dl->AddCircleFilled(p, pr, TypeColor(NodeOutputType(n, o)));
            dl->AddCircle(p, pr, IM_COL32(20, 20, 24, 255), 0, 1.5f);
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
        if (!out && d.tiled && ed.zoom > 0.4f)
            dl->AddText(ImVec2(ta.x + 4, ta.y + 4), ui::col::TextDim, "Aperçu indisponible\n(grand document)");
        if (out) {
            gpu::Texture* t = v->renderer.Thumbnail(*ctx, kThumbKeyBit | n.id, out, outHash, (u32)L.thumbW, (u32)L.thumbH);
            ImVec2 tb(ta.x + L.thumbW * ed.zoom, ta.y + L.thumbH * ed.zoom);
            dl->AddImageRounded((ImTextureID)gpu::Dev().UiTextureId(t), ta, tb, ImVec2(0, 0), ImVec2(1, 1), n.bypass ? IM_COL32(255, 255, 255, 90) : IM_COL32_WHITE, 3 * ed.zoom);
        }
    }

    // Context menus
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 16.0f) {
        ed.contextPos = toGraph(mouse);
        if (hoverNode) {
            d.S().activeNode = hoverNode;
            if (!ed.selection.count(hoverNode)) ed.selection = {hoverNode};
            ImGui::OpenPopup("##nodectx");
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
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool enter = ImGui::InputTextWithHint("##search", "Rechercher...", ed.search, sizeof(ed.search), ImGuiInputTextFlags_EnterReturnsTrue);
        std::string q = Lower(ed.search);
        static std::vector<NodeEntry> entries;
        static u64 entriesGen = ~0ull;
        if (entriesGen != Effects().Generation()) { entries = NodeEntries(); entriesGen = Effects().Generation(); }
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
        if (gen != Effects().Generation()) { entries = NodeEntries(); gen = Effects().Generation(); }
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
            ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{kNodeW, NodeLayout(n, aspect).h});
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
            Layout L = NodeLayout(*on, aspect);
            dl->AddRect(toScreen(on->pos), toScreen(on->pos + float2{kNodeW, L.h}), ui::col::Accent2, 6 * ed.zoom, 0, 3.0f);
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
    if (focused && editable && !io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) deleteSelection();
    if (focused && !io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) for (auto& n : g.nodes) ed.selection.insert(n.id);
    if (focused && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F) && !g.nodes.empty()) {
        float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
        for (auto& n : g.nodes) {
            if (!ed.selection.empty() && !ed.selection.count(n.id)) continue;  // F frames the selection when there is one
            mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
            mx = {std::max(mx.x, n.pos.x + kNodeW), std::max(mx.y, n.pos.y + NodeLayout(n, aspect).h)};
        }
        float2 ext = mx - mn;
        ed.zoom = std::clamp(std::min(size.x / (ext.x + 80), size.y / (ext.y + 80)), 0.2f, 1.5f);
        ed.pan = float2{size.x / ed.zoom * 0.5f, size.y / ed.zoom * 0.5f} - (mn + ext * 0.5f);
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
