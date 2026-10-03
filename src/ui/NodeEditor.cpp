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
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            if (!n.fx.customCode.empty()) return IM_COL32(200, 92, 150, 255);
            if (d && d->inputs.empty()) return IM_COL32(210, 130, 50, 255);
            return IM_COL32(112, 84, 220, 255);
        }
    }
    return IM_COL32(90, 90, 90, 255);
}

struct Layout {
    float h;
    float thumbW, thumbH;
    int inputs;
};

Layout NodeLayout(const Node& n, float aspect) {
    Layout l;
    l.inputs = NodeInputCount(n);
    l.thumbW = kNodeW - 16;
    l.thumbH = std::clamp(l.thumbW / std::max(aspect, 0.1f), 40.0f, l.thumbW);
    l.h = kHeader + std::max(l.inputs, 1) * kRow + l.thumbH + 12;
    return l;
}
}  // namespace

void App::DrawNodeEditor() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Graphe", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    DocView* v = Active();
    if (!open || !v) { ImGui::End(); return; }
    Document& d = *v->doc;
    Graph& g = d.S().graph;
    NodeEditorState& ed = nodeEd;
    const bool editable = d.Mode() == DocMode::Nodes;
    ImGuiIO& io = ImGui::GetIO();

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
    bool panDrag = ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0) ||
                                             (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0) && (io.KeyAlt || spaceHeld)));
    if (panDrag) ed.pan = ed.pan + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};

    // Grid
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(19, 20, 24, 255));
    float step = 24.0f * ed.zoom;
    if (step > 6) {
        float ox = std::fmod(ed.pan.x * ed.zoom, step), oy = std::fmod(ed.pan.y * ed.zoom, step);
        int i = 0;
        for (float x = ox; x < size.x; x += step, ++i)
            dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y), ui::col::Grid);
        for (float y = oy; y < size.y; y += step)
            dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y), ui::col::Grid);
    }

    // Geometry
    auto outPin = [&](const Node& n) { return toScreen(n.pos + float2{kNodeW, kHeader * 0.5f}); };
    auto inPin = [&](const Node& n, int i) { return toScreen(n.pos + float2{0, kHeader + kRow * (i + 0.5f)}); };
    float pinR = kPin * ed.zoom;
    ImVec2 mouse = io.MousePos;
    auto isNear = [&](ImVec2 a, ImVec2 b, float r) { float dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy <= r * r; };

    // Hit testing (topmost = last)
    Id hoverNode = 0, hoverOut = 0, hoverInNode = 0;
    int hoverIn = -1;
    if (hovered) {
        for (int k = (int)g.nodes.size() - 1; k >= 0; --k) {
            const Node& n = g.nodes[k];
            Layout L = NodeLayout(n, aspect);
            if (n.kind != NodeKind::Output && isNear(mouse, outPin(n), pinR * 2.2f)) { hoverOut = n.id; break; }
            bool pinHit = false;
            for (int i = 0; i < L.inputs; ++i)
                if (isNear(mouse, inPin(n, i), pinR * 2.2f)) { hoverInNode = n.id; hoverIn = i; pinHit = true; break; }
            if (pinHit) break;
            ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{kNodeW, L.h});
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) { hoverNode = n.id; break; }
        }
    }

    // Mouse interactions
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !spaceHeld) {
        if (editable && hoverOut) {
            ed.draggingLink = true;
            ed.linkFrom = hoverOut;
            ed.linkTo = 0;
            ed.linkInput = -1;
        } else if (editable && hoverInNode) {
            Node* n = g.Find(hoverInNode);
            Id src = n && hoverIn < (int)n->inputs.size() ? n->inputs[hoverIn] : 0;
            ed.moveBefore = d.S();
            if (src) {  // pick up the existing link
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
            if (Node* n = g.Find(hoverNode)) {
                if (!editable && n->layer) { d.S().activeLayer = n->layer; d.S().editMask = false; }
            }
            // Nodes can be moved in both modes (layer mode keeps the position over rebuilds).
            ed.dragNode = hoverNode;
            ed.moveBefore = d.S();
            // Bring to front
            auto it = std::find_if(g.nodes.begin(), g.nodes.end(), [&](const Node& n) { return n.id == hoverNode; });
            if (it != g.nodes.end() && editable) std::rotate(it, it + 1, g.nodes.end());
        } else {
            d.S().activeNode = 0;
        }
    }
    if (ed.dragNode && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
        if (Node* n = g.Find(ed.dragNode)) {
            n->pos = n->pos + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
            if (!editable) d.S().layerNodePos[n->id] = n->pos;
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (ed.draggingLink) {
            bool linked = false;
            if (ed.linkFrom && hoverInNode) linked = d.Link(ed.linkFrom, hoverInNode, hoverIn);
            else if (ed.linkTo && hoverOut) linked = d.Link(hoverOut, ed.linkTo, ed.linkInput);
            if (!linked && ed.linkFrom && hoverInNode) Status("Lien refusé (cycle)");
            if (ed.moveBefore) v->history.PushState(linked ? "Relier" : "Délier", *ed.moveBefore, d.S());
            else if (linked) v->history.PushState("Relier", frameBefore, d.S());
            ed.draggingLink = false;
            ed.linkFrom = ed.linkTo = 0;
            ed.moveBefore.reset();
        }
        if (ed.dragNode) {
            const Node* before = ed.moveBefore ? ed.moveBefore->graph.Find(ed.dragNode) : nullptr;
            const Node* now = g.Find(ed.dragNode);
            if (before && now && !(before->pos == now->pos)) v->history.PushState("Déplacer le nœud", *ed.moveBefore, d.S());
            ed.dragNode = 0;
            ed.moveBefore.reset();
        }
    }

    // Links
    auto drawLink = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
        float dx = std::max(std::abs(b.x - a.x) * 0.5f, 40.0f * ed.zoom);
        dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, col, thick * ed.zoom);
    };
    for (const Node& n : g.nodes) {
        for (int i = 0; i < (int)n.inputs.size() && i < NodeInputCount(n); ++i) {
            const Node* src = n.inputs[i] ? g.Find(n.inputs[i]) : nullptr;
            if (!src) continue;
            bool sel = src->id == d.S().activeNode || n.id == d.S().activeNode;
            drawLink(outPin(*src), inPin(n, i), sel ? ui::col::Accent2 : IM_COL32(150, 150, 170, 200), sel ? 3.0f : 2.0f);
        }
    }
    if (ed.draggingLink) {
        if (ed.linkFrom) { if (const Node* s = g.Find(ed.linkFrom)) drawLink(outPin(*s), mouse, ui::col::Accent, 2.5f); }
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
        bool selected = n.id == d.S().activeNode;
        bool failedNode = v->renderer.NodeFailed(n.id);
        bool isPaint = n.kind == NodeKind::Surface && n.surface == d.PaintTarget();
        dl->AddRectFilled(ImVec2(a.x + 3, a.y + 4), ImVec2(b.x + 3, b.y + 4), IM_COL32(0, 0, 0, 70), r);
        dl->AddRectFilled(a, b, n.bypass ? IM_COL32(36, 37, 42, 255) : ui::col::PanelLight, r);
        ImU32 hc = HeaderColor(n);
        if (n.bypass) hc = (hc & 0x00FFFFFF) | 0x60000000;
        dl->AddRectFilled(a, ImVec2(b.x, a.y + kHeader * ed.zoom), hc, r, ImDrawFlags_RoundCornersTop);
        ImU32 border = failedNode ? ui::col::Error : selected ? ui::col::Accent : (n.id == hoverNode ? IM_COL32(110, 110, 130, 255) : ui::col::Border);
        dl->AddRect(a, b, border, r, 0, selected || failedNode ? 2.0f : 1.0f);
        if (drawText) {
            std::string title = n.name;
            if (isPaint) title = "\xE2\x9C\x8E " + title;  // pencil mark on the paint target
            dl->PushClipRect(a, ImVec2(b.x - 4, a.y + kHeader * ed.zoom), true);
            dl->AddText(font, fs, ImVec2(a.x + 8 * ed.zoom, a.y + (kHeader * ed.zoom - fs) * 0.5f), IM_COL32_WHITE, title.c_str());
            dl->PopClipRect();
        }
        // Inputs
        for (int i = 0; i < L.inputs; ++i) {
            ImVec2 p = inPin(n, i);
            bool connected = i < (int)n.inputs.size() && n.inputs[i];
            bool hot = hoverInNode == n.id && hoverIn == i;
            dl->AddCircleFilled(p, pinR * (hot ? 1.4f : 1.0f), connected ? ui::col::Accent2 : IM_COL32(70, 72, 86, 255));
            dl->AddCircle(p, pinR * (hot ? 1.4f : 1.0f), IM_COL32(20, 20, 24, 255), 0, 1.5f);
            if (drawText) dl->AddText(font, fs * 0.9f, ImVec2(p.x + 10 * ed.zoom, p.y - fs * 0.45f), ui::col::TextDim, NodeInputName(n, i).c_str());
        }
        // Output
        if (n.kind != NodeKind::Output) {
            ImVec2 p = outPin(n);
            bool hot = hoverOut == n.id;
            dl->AddCircleFilled(p, pinR * (hot ? 1.4f : 1.0f), ui::col::Accent);
            dl->AddCircle(p, pinR * (hot ? 1.4f : 1.0f), IM_COL32(20, 20, 24, 255), 0, 1.5f);
        }
        // Thumbnail
        gpu::Texture* out = v->renderer.NodeOutput(n.id);
        u64 outHash = v->renderer.NodeHash(n.id);
        // Tiled mode (large documents) keeps no per-node images: surfaces and the
        // final image can still be previewed.
        if (!out && n.kind == NodeKind::Surface)
            if (Surface* s = d.GetSurface(n.surface)) { out = s->tex.get(); outHash = s->version; }
        if (!out && n.kind == NodeKind::Output && d.tiled) { out = v->lastImage; outHash = v->renderer.ImageVersion(); }
        ImVec2 ta = toScreen(n.pos + float2{8, kHeader + std::max(L.inputs, 1) * kRow + 4});
        if (!out && d.tiled && ed.zoom > 0.4f)
            dl->AddText(ImVec2(ta.x + 4, ta.y + 4), ui::col::TextDim, "Aperçu indisponible\n(grand document)");
        if (out) {
            gpu::Texture* t = v->renderer.Thumbnail(*ctx, kThumbKeyBit | n.id, out, outHash, (u32)L.thumbW, (u32)L.thumbH);
            ImVec2 tb(ta.x + L.thumbW * ed.zoom, ta.y + L.thumbH * ed.zoom);
            dl->AddImageRounded((ImTextureID)gpu::Dev().UiTextureId(t), ta, tb, ImVec2(0, 0), ImVec2(1, 1), n.bypass ? IM_COL32(255, 255, 255, 90) : IM_COL32_WHITE, 3 * ed.zoom);
        }
    }

    // Context menu
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        ed.contextPos = toGraph(mouse);
        if (hoverNode) {
            d.S().activeNode = hoverNode;
            ImGui::OpenPopup("##nodectx");
        } else if (editable) {
            ed.search[0] = 0;
            ImGui::OpenPopup("##addnode");
        }
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && ImGui::IsItemActive())
        ed.pan = ed.pan + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};

    if (ImGui::BeginPopup("##addnode")) {
        ImGui::TextDisabled("Ajouter un nœud");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##search", "Rechercher...", ed.search, sizeof(ed.search));
        std::string q = ed.search;
        for (auto& ch : q) ch = (char)tolower((unsigned char)ch);
        auto match = [&](const std::string& s) {
            if (q.empty()) return true;
            std::string l = s;
            for (auto& ch : l) ch = (char)tolower((unsigned char)ch);
            return l.find(q) != std::string::npos;
        };
        auto add = [&](auto&& fn, const char* label) {
            DocState before = d.S();
            fn();
            v->history.PushState(label, before, d.S());
            ImGui::CloseCurrentPopup();
        };
        if (match("calque pixel surface") && ImGui::MenuItem("Calque pixel"))
            add([&] { d.AddNode(NodeKind::Surface, ed.contextPos); }, "Ajouter un calque pixel");
        if (match("fusion blend") && ImGui::MenuItem("Fusion (mode de fusion)"))
            add([&] { d.AddNode(NodeKind::Blend, ed.contextPos); }, "Ajouter une fusion");
        if (match("effet visuel graphe") && ImGui::MenuItem("Effet visuel (graphe)"))
            add([&] {
                Node& nn = d.AddEffectNode("", "", ed.contextPos);
                nn.fx.graph = EffectGraph::Default().Serialize();
                nn.name = "Effet visuel";
                nn.fx.params.clear();
                nn.inputs.assign(NodeInputCount(nn), 0);
                ImGui::SetWindowFocus("Éditeur d'effets");
            }, "Ajouter un effet visuel");
        if (match("hlsl personnalisé custom code") && ImGui::MenuItem("HLSL personnalisé"))
            add([&] { d.AddEffectNode("", kCustomEffectTemplate, ed.contextPos); }, "Ajouter un nœud HLSL");
        ImGui::Separator();
        if (q.empty()) {
            EffectMenu([&](const std::string& id) { add([&] { d.AddEffectNode(id, "", ed.contextPos); }, "Ajouter un effet"); }, true, true);
        } else {
            for (auto& e : Effects().All())
                if (match(e->name) || match(e->category))
                    if (ImGui::MenuItem((e->name + "  (" + e->category + ")").c_str()))
                        add([&] { d.AddEffectNode(e->id, "", ed.contextPos); }, "Ajouter un effet");
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##nodectx")) {
        Node* n = g.Find(d.S().activeNode);
        if (n) {
            ImGui::TextDisabled("%s", n->name.c_str());
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
                    v->history.PushState("Dupliquer le nœud", before, d.S());
                }
                if (ImGui::MenuItem("Supprimer", "Suppr")) {
                    DocState before = d.S();
                    d.RemoveNode(n->id);
                    v->history.PushState("Supprimer le nœud", before, d.S());
                }
            } else if (!editable) {
                ImGui::TextDisabled("Graphe en lecture seule (mode calques)");
            }
        }
        ImGui::EndPopup();
    }

    // Keyboard
    if (focused && editable && !io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        Id sel = d.S().activeNode;
        if (sel && sel != g.output) {
            DocState before = d.S();
            d.RemoveNode(sel);
            v->history.PushState("Supprimer le nœud", before, d.S());
        }
    }
    if (focused && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F) && !g.nodes.empty()) {
        float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
        for (auto& n : g.nodes) {
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
        const char* msg = "Mode calques : graphe généré, lecture seule.";
        dl->AddRectFilled(ImVec2(bp.x - 6, bp.y - 4), ImVec2(bp.x + ImGui::CalcTextSize(msg).x + 190 * dpi, bp.y + ImGui::GetFrameHeight()), IM_COL32(20, 20, 26, 220), 5);
        dl->AddText(ImVec2(bp.x, bp.y + 3), ui::col::TextDim, msg);
        ImGui::SetCursorScreenPos(ImVec2(bp.x + ImGui::CalcTextSize(msg).x + 10, bp.y));
        if (ImGui::SmallButton("Passer en mode nodal")) {
            Edit e(d, v->history, "Convertir en nœuds");
            d.ConvertToNodes();
            e.Commit();
        }
    } else {
        const char* hint = "Clic droit : ajouter  |  glisser une sortie vers une entrée : relier  |  F : cadrer  |  Suppr : supprimer";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        dl->AddRectFilled(ImVec2(bp.x - 6, bp.y - 4), ImVec2(bp.x + ts.x + 6, bp.y + ts.y + 4), IM_COL32(20, 20, 26, 220), 5);
        dl->AddText(ImVec2(bp.x, bp.y), ui::col::TextDim, hint);
    }
    dl->PopClipRect();
    ImGui::End();
}

}  // namespace sx
