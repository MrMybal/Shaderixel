// Pen tool and Paths panel.
//
// Pen: click adds a corner anchor, click-drag pulls symmetric handles (Alt: only
// the outgoing one), clicking the first anchor closes the subpath, Enter/Esc
// ends it, Backspace removes the last anchor. Ctrl+drag edits anchors/handles
// (Alt breaks the handle symmetry). Paths live in the document state, so every
// edit is undoable and saved in .sxl.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>

#include <cstring>

namespace sx {

namespace {
VectorPath* FindPath(DocState& s, Id id) {
    for (auto& p : s.paths) if (p.id == id) return &p;
    return nullptr;
}
}  // namespace

VectorPath* App::ActivePath(Document& doc) { return FindPath(doc.S(), doc.S().activePath); }

void App::PenInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = *v.doc;
    float2 dp = Snap(v, ScreenToDoc(v, canvasMin, io.MousePos));
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    auto isNear = [&](float2 p) {
        ImVec2 s = toScreen(p);
        float dx = s.x - io.MousePos.x, dy = s.y - io.MousePos.y;
        return dx * dx + dy * dy <= 49.0f;
    };
    VectorPath* path = ActivePath(doc);
    bool click = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // Keys while drawing.
    if (pen.drawing && path && !path->subpaths.empty() && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape)) pen.drawing = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            DocState before = doc.S();
            SubPath& sp = path->subpaths.back();
            if (!sp.pts.empty()) sp.pts.pop_back();
            if (sp.pts.empty()) { path->subpaths.pop_back(); pen.drawing = false; }
            v.history.PushState("Plume", before, doc.S());
            return;
        }
    }

    // Ctrl+drag: direct selection of anchors and handles.
    if (click && io.KeyCtrl && path) {
        pen.editSub = -1;
        for (int s = 0; s < (int)path->subpaths.size() && pen.editSub < 0; ++s)
            for (int i = 0; i < (int)path->subpaths[s].pts.size(); ++i) {
                const PathAnchor& a = path->subpaths[s].pts[i];
                int part = isNear(a.p) ? 0 : isNear(a.out) && Length(a.out - a.p) > 0.01f ? 2 : isNear(a.in) && Length(a.in - a.p) > 0.01f ? 1 : -1;
                if (part < 0) continue;
                pen.editSub = s;
                pen.editPt = i;
                pen.editPart = part;
                pen.editStart = dp;
                pen.editOrig = a;
                pen.before = doc.S();
                break;
            }
        return;
    }
    if (pen.editSub >= 0) {
        if (path && pen.editSub < (int)path->subpaths.size() && pen.editPt < (int)path->subpaths[pen.editSub].pts.size()) {
            PathAnchor& a = path->subpaths[pen.editSub].pts[pen.editPt];
            const PathAnchor& o = pen.editOrig;
            float2 d = dp - pen.editStart;
            bool symmetric = Length((o.in - o.p) + (o.out - o.p)) < 0.5f && !io.KeyAlt;
            if (pen.editPart == 0) {
                a = {o.p + d, o.in + d, o.out + d};
            } else if (pen.editPart == 1) {
                a.in = o.in + d;
                if (symmetric) a.out = a.p * 2.0f - a.in;
            } else {
                a.out = o.out + d;
                if (symmetric) a.in = a.p * 2.0f - a.out;
            }
        }
        if (!down) {
            if (pen.before) v.history.PushState("Modifier le tracé", *pen.before, doc.S());
            pen.before.reset();
            pen.editSub = -1;
        }
        return;
    }

    if (click) {
        pen.before = doc.S();
        if (!path) {
            VectorPath p;
            p.id = doc.NewId();
            p.name = "Tracé " + std::to_string(doc.S().paths.size() + 1);
            doc.S().paths.push_back(p);
            doc.S().activePath = p.id;
            path = ActivePath(doc);
            pen.drawing = false;
        }
        SubPath* sp = pen.drawing && !path->subpaths.empty() && !path->subpaths.back().closed ? &path->subpaths.back() : nullptr;
        if (sp && sp->pts.size() >= 2 && isNear(sp->pts.front().p)) {
            sp->closed = true;
            pen.drawing = false;
            pen.dragPt = 0;  // dragging now shapes the first anchor
        } else if (sp) {
            sp->pts.push_back({dp, dp, dp});
            pen.dragPt = (int)sp->pts.size() - 1;
        } else {
            SubPath n;
            n.pts.push_back({dp, dp, dp});
            path->subpaths.push_back(n);
            pen.drawing = true;
            pen.dragPt = 0;
        }
        pen.dragging = true;
        pen.dragOrigin = path->subpaths.back().pts[pen.dragPt].p;
        return;
    }
    if (pen.dragging) {
        if (path && !path->subpaths.empty() && pen.dragPt < (int)path->subpaths.back().pts.size()) {
            PathAnchor& a = path->subpaths.back().pts[pen.dragPt];
            if (Length(dp - pen.dragOrigin) * v.zoom > 2.0f) {
                a.out = dp;
                if (!io.KeyAlt) a.in = a.p * 2.0f - dp;
            }
        }
        if (!down) {
            pen.dragging = false;
            if (pen.before) v.history.PushState("Plume", *pen.before, doc.S());
            pen.before.reset();
        }
    }
}

void App::DrawPenOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    Document& doc = *v.doc;
    VectorPath* path = ActivePath(doc);
    if (!path) return;
    bool penTool = tool == Tool::Pen;
    auto S = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    const ImU32 lineCol = IM_COL32(80, 160, 255, 255);
    for (const SubPath& sp : path->subpaths) {
        size_t n = sp.pts.size();
        size_t segs = sp.closed ? n : (n ? n - 1 : 0);
        for (size_t i = 0; i < segs; ++i) {
            const PathAnchor& a = sp.pts[i];
            const PathAnchor& b = sp.pts[(i + 1) % n];
            dl->AddBezierCubic(S(a.p), S(a.out), S(b.in), S(b.p), IM_COL32(0, 0, 0, 120), penTool ? 3.0f : 2.0f);
            dl->AddBezierCubic(S(a.p), S(a.out), S(b.in), S(b.p), lineCol, penTool ? 1.5f : 1.0f);
        }
        if (!penTool) continue;
        for (const PathAnchor& a : sp.pts) {
            for (float2 h : {a.in, a.out}) {
                if (Length(h - a.p) < 0.01f) continue;
                dl->AddLine(S(a.p), S(h), IM_COL32(200, 200, 210, 200));
                dl->AddCircleFilled(S(h), 3.5f, IM_COL32(255, 255, 255, 230));
                dl->AddCircle(S(h), 3.5f, lineCol);
            }
            ImVec2 c = S(a.p);
            dl->AddRectFilled(ImVec2(c.x - 3.5f, c.y - 3.5f), ImVec2(c.x + 3.5f, c.y + 3.5f), IM_COL32(255, 255, 255, 255));
            dl->AddRect(ImVec2(c.x - 3.5f, c.y - 3.5f), ImVec2(c.x + 3.5f, c.y + 3.5f), lineCol);
        }
    }
    // Rubber band to the cursor while drawing.
    if (penTool && pen.drawing && !pen.dragging && !path->subpaths.empty() && !path->subpaths.back().pts.empty()) {
        const PathAnchor& last = path->subpaths.back().pts.back();
        ImVec2 m = ImGui::GetIO().MousePos;
        dl->AddBezierCubic(S(last.p), S(last.out), m, m, IM_COL32(80, 160, 255, 140), 1.0f);
        const SubPath& sp = path->subpaths.back();
        if (sp.pts.size() >= 2) {
            ImVec2 f = S(sp.pts.front().p);
            float dx = f.x - m.x, dy = f.y - m.y;
            if (dx * dx + dy * dy <= 49.0f) dl->AddCircle(f, 7, IM_COL32(255, 255, 255, 220), 0, 1.5f);  // close hint
        }
    }
}

// ------------------------------------------------------------------ path operations

bool App::PathToSelection(DocView& v, SelMode mode) {
    Document& doc = *v.doc;
    VectorPath* path = ActivePath(doc);
    if (!path || path->Empty()) return false;
    Rect r;
    std::vector<u8> cov;
    RasterizeContours(FlattenPath(*path), (int)doc.Width(), (int)doc.Height(), selAntialias, r, cov);
    if (r.Empty()) return false;
    v.renderer.SelectCoverage(doc, *ctx, v.history, cov, r, mode, selFeather, "Sélection depuis le tracé");
    return true;
}

void App::MakePathFromSelection(DocView& v) {
    Surface* sel = v.doc->Selection();
    if (!sel) { Status("Créer un tracé : faites d'abord une sélection"); return; }
    selToPath.view = &v;
    selToPath.mask = ctx->ReadbackTexture(sel->tex.get());
}

void App::UpdateSelectionToPath() {
    if (!selToPath.mask || !selToPath.mask->ready) return;
    gpu::ReadbackPtr rb = std::move(selToPath.mask);
    DocView* v = selToPath.view;
    selToPath = {nullptr, nullptr, selToPath.tolerance};
    bool alive = false;
    for (auto& d : docs) alive |= d.get() == v;
    if (!alive || rb->data.empty()) return;
    Document& doc = *v->doc;
    std::vector<SubPath> subs = TraceMask(rb->data.data(), (int)rb->width, (int)rb->height, selToPath.tolerance);
    if (subs.empty()) { Status("Créer un tracé : sélection vide"); return; }
    DocState before = doc.S();
    VectorPath p;
    p.id = doc.NewId();
    p.name = "Tracé " + std::to_string(doc.S().paths.size() + 1);
    p.subpaths = std::move(subs);
    size_t anchors = 0;
    for (auto& sp : p.subpaths) anchors += sp.pts.size();
    doc.S().paths.push_back(std::move(p));
    doc.S().activePath = doc.S().paths.back().id;
    pen.drawing = false;
    v->history.PushState("Tracé depuis la sélection", before, doc.S());
    Status("Tracé créé : " + std::to_string(doc.S().paths.back().subpaths.size()) + " contour(s), " + std::to_string(anchors) + " points");
}

void App::FillPath(DocView& v) {
    Document& doc = *v.doc;
    VectorPath* path = ActivePath(doc);
    Id target = doc.PaintTarget();
    if (!path || path->Empty() || !doc.GetSurface(target)) { Status("Rien à remplir"); return; }
    Rect r;
    std::vector<u8> cov;
    const int W = (int)doc.Width(), H = (int)doc.Height();
    RasterizeContours(FlattenPath(*path), W, H, true, r, cov);
    if (r.Empty()) return;
    std::vector<u8> full((size_t)W * H, 0);
    for (int y = 0; y < r.Height(); ++y) memcpy(&full[(size_t)(r.y0 + y) * W + r.x0], &cov[(size_t)y * r.Width()], r.Width());
    auto tex = gpu::Dev().CreateTexture((u32)W, (u32)H, gpu::Format::R8, "PathCoverage");
    if (!tex) return;
    ctx->Upload(tex.get(), full.data(), (u32)W);
    v.renderer.FillSelection(doc, *ctx, &v.history, target, 0, LinearColor(fg), 1.0f, "Remplir le tracé", nullptr, tex.get());
}

void App::StrokePath(DocView& v) {
    Document& doc = *v.doc;
    VectorPath* path = ActivePath(doc);
    Id target = doc.PaintTarget();
    if (!path || path->Empty() || !v.renderer.BeginStroke(doc, *ctx, target)) { Status("Rien à tracer"); return; }
    std::vector<Dab> dabs;
    const float spacing = std::max(1.0f, brush.size * std::max(brush.spacing, 0.01f));
    for (const SubPath& sp : path->subpaths) {
        VectorPath one;
        one.subpaths.push_back(sp);
        auto polys = FlattenPath(one);
        if (polys.empty() || polys[0].empty()) continue;
        std::vector<float2> pl = polys[0];
        if (sp.closed) pl.push_back(pl.front());
        float carry = 0;
        dabs.push_back({pl[0].x, pl[0].y, brush.size * 0.5f, brush.flow});
        for (size_t i = 0; i + 1 < pl.size(); ++i) {
            float2 a = pl[i], d = pl[i + 1] - pl[i];
            float len = Length(d), t = spacing - carry;
            for (; t <= len; t += spacing) {
                float2 p = a + d * (t / len);
                dabs.push_back({p.x, p.y, brush.size * 0.5f, brush.flow});
            }
            carry = len - (t - spacing);
        }
    }
    float4 color = LinearColor(fg);
    if (doc.S().editMask) {
        float l = 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
        color = {l, l, l, 1};
    }
    v.renderer.StrokeDabs(doc, *ctx, dabs, brush, color, false);
    v.renderer.EndStroke(doc, *ctx, v.history, "Contour du tracé");
}

void App::DrawPathsPanel() {
    if (!ImGui::Begin("Tracés", PanelFlag("Tracés"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v) { ImGui::End(); return; }
    Document& d = *v->doc;
    auto& paths = d.S().paths;
    if (paths.empty()) ImGui::TextDisabled("Aucun tracé. Outil Plume (P) pour en créer un.");
    static Id renaming = 0;
    static std::string buffer;
    for (auto& p : paths) {
        ImGui::PushID((int)p.id);
        bool sel = d.S().activePath == p.id;
        if (renaming == p.id) {
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##name", &buffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) ||
                ImGui::IsItemDeactivated()) {
                if (!buffer.empty() && buffer != p.name) {
                    DocState before = d.S();
                    for (auto& q : d.S().paths) if (q.id == renaming) q.name = buffer;
                    v->history.PushState("Renommer le tracé", before, d.S());
                }
                renaming = 0;
            }
        } else {
            size_t anchors = 0;
            for (auto& sp : p.subpaths) anchors += sp.pts.size();
            std::string label = p.name + "  (" + std::to_string(anchors) + " points)";
            if (ImGui::Selectable(label.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick)) {
                d.S().activePath = p.id;
                pen.drawing = false;
                if (ImGui::IsMouseDoubleClicked(0)) { renaming = p.id; buffer = p.name; }
            }
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    VectorPath* active = ActivePath(d);
    if (ImGui::Button("Nouveau")) {
        d.S().activePath = 0;
        pen.drawing = false;
        tool = Tool::Pen;
    }
    ImGui::BeginDisabled(!active);
    ImGui::SameLine();
    if (ImGui::Button("Sélection")) PathToSelection(*v, ModeFromModifiers());
    ImGui::SameLine();
    if (ImGui::Button("Remplir")) FillPath(*v);
    ImGui::SameLine();
    if (ImGui::Button("Contour")) StrokePath(*v);
    ImGui::SameLine();
    if (ImGui::Button("Supprimer") && active) {
        DocState before = d.S();
        Id id = active->id;
        std::erase_if(d.S().paths, [&](const VectorPath& p) { return p.id == id; });
        d.S().activePath = 0;
        pen.drawing = false;
        v->history.PushState("Supprimer le tracé", before, d.S());
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!d.Selection() || selToPath.mask);
    if (ImGui::Button("Depuis la sélection")) MakePathFromSelection(*v);
    ImGui::EndDisabled();
    ui::HelpTooltip("Crée un tracé qui suit le contour de la sélection");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x));
    ImGui::SliderFloat("##tol", &selToPath.tolerance, 0.5f, 10.0f, "Tolérance %.1f px");
    ImGui::TextDisabled("Contour : pinceau et couleur de premier plan actuels.");
    ImGui::End();
}

}  // namespace sx
