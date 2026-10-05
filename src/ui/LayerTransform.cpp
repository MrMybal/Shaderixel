// Layer transform tool (A): move / rotate / scale / pivot of image layers, as in CyTexture and
// Photoshop's free transform, with Unreal-style snapping grids. Image layers are smart objects:
// the whole image is kept (what goes past the document is shown dimmed outside it), and the
// transform stays editable (Propriétés > Transformation). Pixel layers are converted on first use.
//  - W move (arrows: one axis, elsewhere: free), E rotate (around the pivot), R scale (X / Y
//    handles, center or corners: uniform, edges: one axis), P pivot.
//  - Shift: 4x faster, Ctrl: 4x slower; grids per mode (bar at the top right of the view).
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <cmath>

namespace sx {

namespace {

constexpr float kDeg = 57.29578f;

// Parametric transform of a smart layer: the pivot (contents UV) lands on `pos` (document px),
// scaled then rotated around it. Derived from the corners, written back as corners.
struct XfP {
    float2 pos, scale = {1, 1};
    float rot = 0;  // radians
    float2 pivot = {0.5f, 0.5f};
};

float2 Rot(float2 p, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {c * p.x - s * p.y, s * p.x + c * p.y};
}

XfP FromCorners(const float2 c[4], float W, float H, float2 pivot) {
    XfP p;
    float2 ax = c[1] - c[0], ay = c[3] - c[0];
    p.rot = std::atan2(ax.y, ax.x);
    p.scale = {Length({ax.x, ax.y, 0}) / W, Length({ay.x, ay.y, 0}) / H};
    if (ax.x * ay.y - ax.y * ay.x < 0) p.scale.y = -p.scale.y;  // mirrored
    p.pivot = pivot;
    p.pos = c[0] + ax * pivot.x + ay * pivot.y;
    return p;
}

float2 Local(const XfP& p, float W, float H, float u, float v) {
    return p.pos + Rot({(u - p.pivot.x) * W * p.scale.x, (v - p.pivot.y) * H * p.scale.y}, p.rot);
}

void ToCorners(const XfP& p, float W, float H, float2 c[4]) {
    c[0] = Local(p, W, H, 0, 0);
    c[1] = Local(p, W, H, 1, 0);
    c[2] = Local(p, W, H, 1, 1);
    c[3] = Local(p, W, H, 0, 1);
}

// Document point -> contents UV.
float2 ToUv(const XfP& p, float W, float H, float2 d) {
    float2 l = Rot(d - p.pos, -p.rot);
    return {l.x / (W * (std::abs(p.scale.x) > 1e-6f ? p.scale.x : 1e-6f)) + p.pivot.x, l.y / (H * (std::abs(p.scale.y) > 1e-6f ? p.scale.y : 1e-6f)) + p.pivot.y};
}

float SnapStep(float v, float step) { return step > 0 ? std::round(v / step) * step : v; }

float DistToSegment(ImVec2 p, ImVec2 a, ImVec2 b) {
    float vx = b.x - a.x, vy = b.y - a.y, wx = p.x - a.x, wy = p.y - a.y;
    float t = std::clamp((wx * vx + wy * vy) / std::max(vx * vx + vy * vy, 1e-6f), 0.0f, 1.0f);
    float dx = a.x + vx * t - p.x, dy = a.y + vy * t - p.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Unit square -> quad homography (row-major, column vector), as in SmartPlace.
bool SquareToQuadInv(const float2 c[4], double inv[9]) {
    double x0 = c[0].x, y0 = c[0].y, x1 = c[1].x, y1 = c[1].y, x2 = c[2].x, y2 = c[2].y, x3 = c[3].x, y3 = c[3].y;
    double dx1 = x1 - x2, dx2 = x3 - x2, dy1 = y1 - y2, dy2 = y3 - y2;
    double sx = x0 - x1 + x2 - x3, sy = y0 - y1 + y2 - y3;
    double m[9];
    if (std::abs(sx) < 1e-9 && std::abs(sy) < 1e-9) {
        m[0] = x1 - x0; m[1] = x3 - x0; m[2] = x0;
        m[3] = y1 - y0; m[4] = y3 - y0; m[5] = y0;
        m[6] = 0; m[7] = 0; m[8] = 1;
    } else {
        double den = dx1 * dy2 - dx2 * dy1;
        if (std::abs(den) < 1e-12) return false;
        double g = (sx * dy2 - dx2 * sy) / den, h = (dx1 * sy - sx * dy1) / den;
        m[0] = x1 - x0 + g * x1; m[1] = x3 - x0 + h * x3; m[2] = x0;
        m[3] = y1 - y0 + g * y1; m[4] = y3 - y0 + h * y3; m[5] = y0;
        m[6] = g; m[7] = h; m[8] = 1;
    }
    double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (std::abs(det) < 1e-12) return false;
    double id = 1.0 / det;
    inv[0] = (m[4] * m[8] - m[5] * m[7]) * id; inv[1] = (m[2] * m[7] - m[1] * m[8]) * id; inv[2] = (m[1] * m[5] - m[2] * m[4]) * id;
    inv[3] = (m[5] * m[6] - m[3] * m[8]) * id; inv[4] = (m[0] * m[8] - m[2] * m[6]) * id; inv[5] = (m[2] * m[3] - m[0] * m[5]) * id;
    inv[6] = (m[3] * m[7] - m[4] * m[6]) * id; inv[7] = (m[1] * m[6] - m[0] * m[7]) * id; inv[8] = (m[0] * m[4] - m[1] * m[3]) * id;
    return true;
}

// transform_2d parameters: 0 position (px offset of the pivot), 1 scale, 2 scale X / Y, 3 rotation (deg), 4 pivot (document fraction).
XfP FxGet(const EffectInstance& fx, float W, float H) {
    XfP p;
    auto at = [&](size_t i, float4 def) { return i < fx.params.size() ? fx.params[i] : def; };
    float4 pos = at(0, {0, 0, 0, 0}), sc = at(1, {1, 0, 0, 0}), sxy = at(2, {1, 1, 0, 0}), rot = at(3, {0, 0, 0, 0}), pv = at(4, {0.5f, 0.5f, 0, 0});
    p.pivot = {pv.x, pv.y};
    p.pos = {pv.x * W + pos.x, pv.y * H + pos.y};
    p.scale = {sc.x * sxy.x, sc.x * sxy.y};
    p.rot = rot.x / kDeg;
    return p;
}

void FxSet(EffectInstance& fx, const XfP& p, float W, float H) {
    if (fx.params.size() < 5) fx.params.resize(5, float4{0, 0, 0, 0});
    fx.params[0] = {p.pos.x - p.pivot.x * W, p.pos.y - p.pivot.y * H, 0, 0};
    fx.params[1] = {1, 0, 0, 0};
    fx.params[2] = {p.scale.x, p.scale.y, 0, 0};
    fx.params[3] = {p.rot * kDeg, 0, 0, 0};
    fx.params[4] = {p.pivot.x, p.pivot.y, 0, 0};
}

}  // namespace

bool App::LayerXfResolve(DocView& v, bool create, LayerXfTarget& t) {
    Document& d = *v.doc;
    t = {};
    if (d.Mode() == DocMode::Nodes) {
        Node* n = d.S().graph.Find(d.S().activeNode);
        if (n && n->kind == NodeKind::Effect && n->fx.effectId == "transform_2d") {
            t.fx = &n->fx;
        } else if (create && n && n->kind != NodeKind::Output) {
            // A Transform node after the active node (its consumers rewired), as for effects.
            ApplyLibraryEffect(v, "transform_2d", FxApply::Filter);
            t.created = true;
            for (auto& m : d.S().graph.nodes)
                if (m.kind == NodeKind::Effect && m.fx.effectId == "transform_2d" && !m.inputs.empty() && m.inputs[0] == n->id) {
                    d.S().activeNode = m.id;
                    t.fx = &m.fx;
                }
            if (t.fx) Status("Nœud Transform ajouté après « " + d.S().graph.Find(t.fx ? d.S().activeNode : 0)->name + " »");
        }
        t.W = (float)d.Width();
        t.H = (float)d.Height();
        return t.fx != nullptr;
    }
    Layer* l = d.ActiveLayer();
    if (!l || l->locked) return false;
    if (l->kind == LayerKind::Pixel && create) {
        // Image: Photoshop keeps transformed images whole as smart objects; same here, on first use.
        ConvertToSmartObject(v);
        t.created = true;
        l = d.ActiveLayer();
        Status("Calque converti en objet dynamique : l'image entière est conservée, la transformation reste modifiable");
    }
    if (l && l->kind == LayerKind::Smart) {
        SmartObject* so = d.GetSmart(l->smart);
        if (!so) return false;
        t.layer = l;
        t.W = (float)so->width;
        t.H = (float)so->height;
        return true;
    }
    if (l && (l->kind == LayerKind::Fill || l->kind == LayerKind::Text || l->kind == LayerKind::Shape)) {
        // Draw: a Transform filter on the layer (first, before its other filters); stays editable.
        for (auto& f : l->filters)
            if (f.effectId == "transform_2d") t.fx = &f;
        if (!t.fx && create) {
            auto def = Effects().Find("transform_2d");
            if (!def) return false;
            EffectInstance fx;
            fx.effectId = "transform_2d";
            fx.params = def->DefaultParams();
            l->filters.insert(l->filters.begin(), fx);
            t.created = true;
            d.SyncGraph();
            t.fx = &l->filters.front();
        }
        t.W = (float)d.Width();
        t.H = (float)d.Height();
        return t.fx != nullptr;
    }
    return false;
}

bool App::LayerTransformOverflow(DocView& v, gpu::Texture*& content, float out[9]) {
    Document& d = *v.doc;
    Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
    if (!l || l->kind != LayerKind::Smart || !l->visible) return false;
    SmartObject* so = d.GetSmart(l->smart);
    if (!so || !so->content) return false;
    double inv[9];
    if (!SquareToQuadInv(l->smartCorners, inv)) return false;
    for (int i = 0; i < 9; ++i) out[i] = (float)inv[i];
    content = so->content.get();
    return true;
}

void App::LayerTransformInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    Document& d = *v.doc;
    ImGuiIO& io = ImGui::GetIO();
    Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    const float2 mouse = ScreenToDoc(v, canvasMin, io.MousePos);
    if (lxf.drag < 0) {
        if (!hovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
        lxf.before = d.S();  // before a conversion / an added Transform: one undo step with the drag
        LayerXfTarget t;
        if (!LayerXfResolve(v, true, t)) {
            Status(d.Mode() == DocMode::Nodes ? "Transformation : sélectionnez un nœud (un nœud Transform est ajouté après lui)"
                                              : l && l->locked ? "Calque verrouillé" : "Transformation : choisissez un calque d'image, de remplissage, de texte ou de forme");
            return;
        }
        const float W = t.W, H = t.H;
        XfP p = t.layer ? FromCorners(t.layer->smartCorners, W, H, t.layer->smartPivot) : FxGet(*t.fx, W, H);
        // Hit test (screen space).
        const ImVec2 ps = toScreen(p.pos), mp = io.MousePos;
        const float2 axX = lxf.world ? float2{1, 0} : Rot({1, 0}, p.rot), axY = lxf.world ? float2{0, 1} : Rot({0, 1}, p.rot);
        const float2 lx = Rot({1, 0}, p.rot), ly = Rot({0, 1}, p.rot);
        const float len = 90.0f * dpi;
        auto tip = [&](float2 a) { return ImVec2(ps.x + a.x * len, ps.y + a.y * len); };
        auto hitBox = [&](ImVec2 a, float r) { return std::abs(mp.x - a.x) <= r && std::abs(mp.y - a.y) <= r; };
        float2 c[4];
        ToCorners(p, W, H, c);
        int drag = 0;
        const float hr = 7.0f * dpi;
        if (lxf.mode == 0) {
            if (lxf.arrows && DistToSegment(mp, ps, tip(axX)) < 6 * dpi && !hitBox(ps, hr)) drag = 1;
            else if (lxf.arrows && DistToSegment(mp, ps, tip(axY)) < 6 * dpi && !hitBox(ps, hr)) drag = 2;
            else drag = 0;
        } else if (lxf.mode == 1) {
            drag = 3;
        } else if (lxf.mode == 2) {
            drag = 4;
            if (lxf.arrows && hitBox(tip(lx), hr * 1.4f)) drag = 5;
            else if (lxf.arrows && hitBox(tip(ly), hr * 1.4f)) drag = 6;
            else {
                for (int k = 0; k < 4; ++k)
                    if (hitBox(toScreen(c[k]), hr)) drag = 8;
                const int ex[4] = {6, 5, 6, 5};  // top, right, bottom, left edges: Y, X, Y, X
                for (int k = 0; k < 4; ++k)
                    if (hitBox(toScreen((c[k] + c[(k + 1) % 4]) * 0.5f), hr)) drag = ex[k];
            }
        } else {
            drag = 7;
        }
        lxf.drag = drag;
        lxf.startMouse = lxf.prevMouse = lxf.virt = mouse;
        lxf.accRot = 0;
        lxf.accLog[0] = lxf.accLog[1] = 0;
        lxf.sPos = p.pos;
        lxf.sScale = p.scale;
        lxf.sRot = p.rot;
        lxf.sPivot = p.pivot;
        lxf.changed = t.created;
        return;
    }
    // Dragging.
    LayerXfTarget t;
    if (!LayerXfResolve(v, false, t)) { lxf.drag = -1; return; }
    const float W = t.W, H = t.H;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        static const char* labels[] = {"Déplacer", "Déplacer (X)", "Déplacer (Y)", "Rotation", "Échelle", "Échelle (X)", "Échelle (Y)", "Pivot", "Échelle"};
        if (lxf.changed) v.history.PushState(std::string("Transformation : ") + labels[std::clamp(lxf.drag, 0, 8)], lxf.before, d.S());
        lxf.drag = -1;
        return;
    }
    // Speed: Shift faster, Ctrl slower (the gizmo follows a virtual cursor).
    const float speed = io.KeyShift ? 4.0f : io.KeyCtrl ? 0.25f : 1.0f;
    const float2 delta = mouse - lxf.prevMouse;
    if (delta.x == 0 && delta.y == 0) return;
    XfP p;
    p.pos = lxf.sPos;
    p.scale = lxf.sScale;
    p.rot = lxf.sRot;
    p.pivot = lxf.sPivot;
    const float2 prev = lxf.virt;
    lxf.virt = lxf.virt + delta * speed;
    lxf.prevMouse = mouse;
    const float2 moved = lxf.virt - lxf.startMouse;
    const float2 axX = lxf.world ? float2{1, 0} : Rot({1, 0}, lxf.sRot), axY = lxf.world ? float2{0, 1} : Rot({0, 1}, lxf.sRot);
    switch (lxf.drag) {
        case 0: {
            p.pos = lxf.sPos + moved;
            if (lxf.snap[0]) {
                // Snapped along the chosen axes (document grid, or the layer's).
                float a = SnapStep(moved.x * axX.x + moved.y * axX.y, lxf.step[0]), b = SnapStep(moved.x * axY.x + moved.y * axY.y, lxf.step[0]);
                p.pos = lxf.sPos + axX * a + axY * b;
            }
            break;
        }
        case 1:
        case 2: {
            float2 a = lxf.drag == 1 ? axX : axY;
            float t = moved.x * a.x + moved.y * a.y;
            if (lxf.snap[0]) t = SnapStep(t, lxf.step[0]);
            p.pos = lxf.sPos + a * t;
            break;
        }
        case 3: {
            float2 r0 = prev - lxf.sPos, r1 = lxf.virt - lxf.sPos;
            if (Length({r0.x, r0.y, 0}) > 2 && Length({r1.x, r1.y, 0}) > 2) {
                double da = std::atan2(r1.y, r1.x) - std::atan2(r0.y, r0.x);
                while (da > 3.14159265) da -= 6.2831853;
                while (da < -3.14159265) da += 6.2831853;
                lxf.accRot += da;
            }
            float deg = (lxf.sRot + (float)lxf.accRot) * kDeg;
            if (lxf.snap[1]) deg = SnapStep(deg, lxf.step[1]);
            p.rot = deg / kDeg;
            break;
        }
        case 4:
        case 8:
        case 5:
        case 6: {
            // Scale from the pivot: ratio of distances (uniform) or of the projections on the layer's axes.
            const float2 lx = Rot({1, 0}, lxf.sRot), ly = Rot({0, 1}, lxf.sRot);
            float2 r0 = prev - lxf.sPos, r1 = lxf.virt - lxf.sPos;
            auto logRatio = [](float a, float b) { return std::abs(a) > 1.0f && std::abs(b) > 0.5f ? std::log(std::abs(b) / std::abs(a)) : 0.0; };
            const bool uniform = lxf.drag == 4 || (lxf.drag == 8 && lxf.lockAspect);
            if (uniform) {
                double lr = logRatio(Length({r0.x, r0.y, 0}), Length({r1.x, r1.y, 0}));
                lxf.accLog[0] += lr;
                lxf.accLog[1] += lr;
            } else {
                if (lxf.drag != 6) lxf.accLog[0] += logRatio(r0.x * lx.x + r0.y * lx.y, r1.x * lx.x + r1.y * lx.y);
                if (lxf.drag != 5) lxf.accLog[1] += logRatio(r0.x * ly.x + r0.y * ly.y, r1.x * ly.x + r1.y * ly.y);
                if (lxf.lockAspect && lxf.drag != 8) lxf.accLog[lxf.drag == 5 ? 1 : 0] = lxf.accLog[lxf.drag == 5 ? 0 : 1];
            }
            p.scale = {lxf.sScale.x * (float)std::exp(lxf.accLog[0]), lxf.sScale.y * (float)std::exp(lxf.accLog[1])};
            if (lxf.snap[2]) {
                auto sn = [&](float s) { float m = std::max(SnapStep(std::abs(s), lxf.step[2]), lxf.step[2]); return s < 0 ? -m : m; };
                if (uniform && std::abs(lxf.sScale.x) > 1e-6f) {
                    float k = sn(p.scale.x) / p.scale.x;
                    p.scale = {p.scale.x * k, p.scale.y * k};
                } else {
                    p.scale = {sn(p.scale.x), sn(p.scale.y)};
                }
            }
            break;
        }
        case 7: {
            // Pivot: moved on the document, the layer stays in place.
            float2 np = lxf.sPos + moved;
            if (lxf.snap[0]) np = {SnapStep(np.x, lxf.step[0]), SnapStep(np.y, lxf.step[0])};
            p.pivot = ToUv(p, W, H, np);
            p.pos = np;
            break;
        }
    }
    if (t.layer) {
        float2 c[4];
        ToCorners(p, W, H, c);
        for (int k = 0; k < 4; ++k) t.layer->smartCorners[k] = c[k];
        t.layer->smartPivot = p.pivot;
    } else {
        FxSet(*t.fx, p, W, H);
        d.SyncGraph();  // a layer filter: the generated graph carries a copy of it
    }
    lxf.changed = true;
    d.dirty = true;
}

void App::LayerTransformOverlay(DocView& v, ImVec2 canvasMin, ImVec2 avail, ImDrawList* dl) {
    Document& d = *v.doc;
    ImGuiIO& io = ImGui::GetIO();
    Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    const ImU32 cyan = IM_COL32(0, 217, 255, 242), yellow = IM_COL32(255, 191, 0, 255), red = IM_COL32(235, 70, 60, 255), green = IM_COL32(90, 200, 80, 255);
    LayerXfTarget t;
    const bool has = LayerXfResolve(v, false, t);
    if (has) {
        const float W = t.W, H = t.H;
        XfP p = t.layer ? FromCorners(t.layer->smartCorners, W, H, t.layer->smartPivot) : FxGet(*t.fx, W, H);
        float2 qc[4];
        ToCorners(p, W, H, qc);
        ImVec2 q[4];
        for (int k = 0; k < 4; ++k) q[k] = toScreen(t.layer ? t.layer->smartCorners[k] : qc[k]);
        // Thirds of the layer, then its outline.
        for (int i = 1; i < 3; ++i) {
            float t = i / 3.0f;
            dl->AddLine(toScreen(Local(p, W, H, t, 0)), toScreen(Local(p, W, H, t, 1)), IM_COL32(0, 217, 255, 70));
            dl->AddLine(toScreen(Local(p, W, H, 0, t)), toScreen(Local(p, W, H, 1, t)), IM_COL32(0, 217, 255, 70));
        }
        dl->AddPolyline(q, 4, cyan, ImDrawFlags_Closed, 1.5f);
        const float hs = (lxf.mode == 2 ? 5.0f : 3.5f) * dpi;
        for (int k = 0; k < 4; ++k) {
            ImVec2 m((q[k].x + q[(k + 1) % 4].x) * 0.5f, (q[k].y + q[(k + 1) % 4].y) * 0.5f);
            for (ImVec2 h : {q[k], m}) {
                dl->AddRectFilled(ImVec2(h.x - hs, h.y - hs), ImVec2(h.x + hs, h.y + hs), IM_COL32(20, 20, 24, 230));
                dl->AddRect(ImVec2(h.x - hs, h.y - hs), ImVec2(h.x + hs, h.y + hs), cyan);
            }
        }
        // Gizmo at the pivot.
        const ImVec2 ps = toScreen(p.pos);
        const float len = 90.0f * dpi;
        if (lxf.arrows) {
            const bool local = !lxf.world || lxf.mode == 2;
            const float2 ax = local ? Rot({1, 0}, p.rot) : float2{1, 0}, ay = local ? Rot({0, 1}, p.rot) : float2{0, 1};
            auto arrow = [&](float2 a, ImU32 col, bool box) {
                ImVec2 t(ps.x + a.x * len, ps.y + a.y * len);
                dl->AddLine(ps, t, col, 2.5f * dpi);
                if (box) {
                    dl->AddRectFilled(ImVec2(t.x - 6 * dpi, t.y - 6 * dpi), ImVec2(t.x + 6 * dpi, t.y + 6 * dpi), col);
                } else {
                    float2 n = {-a.y, a.x};
                    const float s = 9 * dpi;
                    dl->AddTriangleFilled(ImVec2(t.x + a.x * s * 1.6f, t.y + a.y * s * 1.6f), ImVec2(t.x + n.x * s * 0.6f, t.y + n.y * s * 0.6f),
                                          ImVec2(t.x - n.x * s * 0.6f, t.y - n.y * s * 0.6f), col);
                }
            };
            if (lxf.mode == 0 || lxf.mode == 2) {
                arrow(ax, red, lxf.mode == 2);
                arrow(ay, green, lxf.mode == 2);
                const float c = 7 * dpi;
                dl->AddRectFilled(ImVec2(ps.x - c, ps.y - c), ImVec2(ps.x + c, ps.y + c), IM_COL32(255, 255, 255, 60));
                dl->AddRect(ImVec2(ps.x - c, ps.y - c), ImVec2(ps.x + c, ps.y + c), IM_COL32(255, 255, 255, 200));
            } else if (lxf.mode == 1) {
                dl->AddCircle(ps, len * 0.8f, IM_COL32(60, 130, 255, 220), 64, 2.5f * dpi);
                ImVec2 t(ps.x + std::cos(p.rot) * len * 0.8f, ps.y + std::sin(p.rot) * len * 0.8f);
                dl->AddLine(ps, t, IM_COL32(60, 130, 255, 220), 1.5f * dpi);
                dl->AddCircleFilled(t, 5 * dpi, IM_COL32(60, 130, 255, 255));
            }
        }
        const float pc = 8 * dpi;
        dl->AddLine(ImVec2(ps.x - pc, ps.y), ImVec2(ps.x + pc, ps.y), yellow, 2.0f);
        dl->AddLine(ImVec2(ps.x, ps.y - pc), ImVec2(ps.x, ps.y + pc), yellow, 2.0f);
        if (lxf.mode == 3) dl->AddCircle(ps, pc * 1.4f, yellow, 0, 1.5f);
        // Values while dragging.
        if (lxf.drag >= 0) {
            char info[128];
            if (lxf.drag == 3) snprintf(info, sizeof(info), "%.2f°", p.rot * kDeg);
            else if (lxf.drag == 4 || lxf.drag >= 5 && lxf.drag != 7) snprintf(info, sizeof(info), "%.1f %% x %.1f %%", p.scale.x * 100, p.scale.y * 100);
            else snprintf(info, sizeof(info), "X %.1f  Y %.1f px", p.pos.x - lxf.sPos.x, p.pos.y - lxf.sPos.y);
            ImVec2 tp(io.MousePos.x + 18, io.MousePos.y + 18), ts = ImGui::CalcTextSize(info);
            dl->AddRectFilled(ImVec2(tp.x - 6, tp.y - 4), ImVec2(tp.x + ts.x + 6, tp.y + ts.y + 4), IM_COL32(20, 20, 24, 230), 4);
            dl->AddText(tp, IM_COL32_WHITE, info);
        }
    }

    // Settings bar, top right of the view (as in Unreal): mode, axes, gizmo, grids.
    const ImVec2 save = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetFrameHeight();
    const float barW = lxf.barMax.x > lxf.barMin.x ? lxf.barMax.x - lxf.barMin.x : 640 * dpi;
    ImVec2 origin(canvasMin.x + avail.x - barW - 10 * dpi, canvasMin.y + 8 * dpi);
    if (lxf.barMax.x > lxf.barMin.x)
        dl->AddRectFilled(ImVec2(origin.x - 6 * dpi, origin.y - 4 * dpi), ImVec2(origin.x + barW + 6 * dpi, origin.y + h + 4 * dpi), IM_COL32(24, 24, 28, 225), 6 * dpi);
    ImGui::SetCursorScreenPos(origin);
    ImVec2 bmin(1e9f, 1e9f), bmax(-1e9f, -1e9f);
    auto track = [&]() {
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        bmin = ImVec2(std::min(bmin.x, a.x), std::min(bmin.y, a.y));
        bmax = ImVec2(std::max(bmax.x, b.x), std::max(bmax.y, b.y));
    };
    ImGui::PushID("lxfbar");
    auto toggle = [&](const char* label, bool on, const char* tip) {
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ui::col::Accent);
        bool r = ImGui::Button(label);
        if (on) ImGui::PopStyleColor();
        track();
        if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        ImGui::SameLine(0, 3 * dpi);
        return r;
    };
    static const char* modes[] = {"Déplacer", "Rotation", "Échelle", "Pivot"};
    static const char* keys[] = {"W", "E", "R", "P"};
    for (int i = 0; i < 4; ++i) {
        char tip[64];
        snprintf(tip, sizeof(tip), "%s (%s)", modes[i], keys[i]);
        if (toggle(modes[i], lxf.mode == i, tip)) lxf.mode = i;
    }
    ImGui::SameLine(0, 8 * dpi);
    if (toggle(lxf.world ? "Monde" : "Relatif", false, "Axes du document (monde) ou du calque (relatif)")) lxf.world = !lxf.world;
    if (toggle("Flèches", lxf.arrows, "Afficher le gizmo (flèches, anneau)")) lxf.arrows = !lxf.arrows;
    if (toggle("Dépassement", lxf.overflow, "Montrer ce qui dépasse du document")) lxf.overflow = !lxf.overflow;
    ImGui::SameLine(0, 8 * dpi);
    // Grids: free, presets, custom.
    struct GridDef { const char* id; const char* unit; std::vector<float> presets; const char* tip; };
    static const GridDef grids[3] = {{"##gm", "px", {1, 2, 5, 10, 25, 50, 100}, "Grille de déplacement / pivot"},
                                     {"##gr", "°", {1, 5, 10, 15, 30, 45, 90}, "Grille de rotation"},
                                     {"##gs", "", {0.01f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f}, "Grille d'échelle"}};
    static const char* gridIcons[] = {"Dépl.", "Rot.", "Éch."};
    for (int g = 0; g < 3; ++g) {
        char preview[48];
        if (!lxf.snap[g]) snprintf(preview, sizeof(preview), "%s libre", gridIcons[g]);
        else snprintf(preview, sizeof(preview), "%s %g%s", gridIcons[g], lxf.step[g], grids[g].unit);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Rot. 100 px").x + h * 1.3f);
        if (lxf.snap[g]) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.32f, 0.55f, 1));
        bool open = ImGui::BeginCombo(grids[g].id, preview, ImGuiComboFlags_HeightLarge);
        if (lxf.snap[g]) ImGui::PopStyleColor();
        track();
        if (ImGui::IsItemHovered() && !open) ImGui::SetTooltip("%s", grids[g].tip);
        if (open) {
            if (ImGui::Selectable("Libre (sans grille)", !lxf.snap[g])) lxf.snap[g] = false;
            for (float s : grids[g].presets) {
                char item[32];
                snprintf(item, sizeof(item), "%g%s", s, grids[g].unit);
                if (ImGui::Selectable(item, lxf.snap[g] && lxf.step[g] == s)) {
                    lxf.snap[g] = true;
                    lxf.step[g] = s;
                }
            }
            ImGui::Separator();
            ImGui::SetNextItemWidth(110 * dpi);
            if (ImGui::InputFloat("Personnalisé", &lxf.step[g], 0, 0, "%g")) {
                lxf.step[g] = std::max(lxf.step[g], 0.0001f);
                lxf.snap[g] = true;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, 3 * dpi);
    }
    if (toggle("Ratio", lxf.lockAspect, "Proportions verrouillées (coins et poignées d'échelle)")) lxf.lockAspect = !lxf.lockAspect;
    ImGui::NewLine();
    ImGui::PopID();
    lxf.barMin = bmin;
    lxf.barMax = bmax;
    ImGui::SetCursorScreenPos(save);
    if (!has) {
        const char* hint = d.Mode() == DocMode::Nodes ? "Cliquez-glissez : un nœud Transform est ajouté après le nœud actif"
                           : l && l->kind == LayerKind::Pixel ? "Cliquez-glissez : le calque devient un objet dynamique (image entière conservée)"
                           : l && (l->kind == LayerKind::Fill || l->kind == LayerKind::Text || l->kind == LayerKind::Shape)
                               ? "Cliquez-glissez : un filtre Transform est ajouté au calque (non destructif)"
                               : "Transformation : sélectionnez un calque d'image, de remplissage, de texte ou de forme";
        dl->AddText(ImVec2(canvasMin.x + 12 * dpi, canvasMin.y + avail.y - 24 * dpi), IM_COL32(255, 210, 140, 230), hint);
    }
}

void App::LayerImagePropertiesUI(DocView& v, Layer& l) {
    Document& d = *v.doc;
    // ---------------------------------------------------------------- transform (as values)
    ImGui::SeparatorText("Transformation");
    SmartObject* so = l.kind == LayerKind::Smart ? d.GetSmart(l.smart) : nullptr;
    const bool draw = l.kind == LayerKind::Fill || l.kind == LayerKind::Text || l.kind == LayerKind::Shape;
    EffectInstance* xfx = nullptr;
    for (auto& f : l.filters)
        if (f.effectId == "transform_2d") xfx = &f;
    const float W = so ? (float)so->width : (float)d.Width(), H = so ? (float)so->height : (float)d.Height();
    XfP p;
    if (so) p = FromCorners(l.smartCorners, W, H, l.smartPivot);
    else if (xfx) p = FxGet(*xfx, W, H);
    else p.pos = {d.Width() * 0.5f, d.Height() * 0.5f};
    XfP n = p;
    bool edit = false;
    float scalePct[2] = {p.scale.x * 100, p.scale.y * 100};
    float deg = p.rot * kDeg;
    edit |= ImGui::DragFloat2("Position (px)", &n.pos.x, 0.5f, -100000, 100000, "%.1f");
    TrackUndo("Position du calque");
    if (ImGui::DragFloat2("Échelle (%)", scalePct, 0.25f, -10000, 10000, "%.2f")) {
        if (lxf.lockAspect) {
            float k = scalePct[0] / std::max(std::abs(p.scale.x * 100), 1e-4f) * (p.scale.x < 0 ? -1.0f : 1.0f);
            if (scalePct[1] != p.scale.y * 100) k = scalePct[1] / std::max(std::abs(p.scale.y * 100), 1e-4f) * (p.scale.y < 0 ? -1.0f : 1.0f);
            scalePct[0] = p.scale.x * 100 * k;
            scalePct[1] = p.scale.y * 100 * k;
        }
        n.scale = {scalePct[0] / 100, scalePct[1] / 100};
        edit = true;
    }
    TrackUndo("Échelle du calque");
    ImGui::SameLine();
    ImGui::Checkbox("##ratio", &lxf.lockAspect);
    ui::HelpTooltip("Proportions verrouillées");
    if (ImGui::DragFloat("Rotation (°)", &deg, 0.25f, -3600, 3600, "%.2f")) {
        n.rot = deg / kDeg;
        edit = true;
    }
    TrackUndo("Rotation du calque");
    if (ImGui::DragFloat2("Pivot", &n.pivot.x, 0.005f, -2, 3, "%.3f")) {
        // Moving the pivot keeps the layer in place.
        n.pos = Local(p, W, H, n.pivot.x, n.pivot.y);
        edit = true;
    }
    TrackUndo("Pivot du calque");
    ui::HelpTooltip("Point de rotation / d'échelle, dans l'image (0,0 coin haut gauche, 1,1 coin bas droit)");
    auto apply = [&](const XfP& np, const char* label) {
        DocState before = d.S();
        LayerXfTarget t;
        if (!LayerXfResolve(v, true, t)) return;
        if (t.layer) {
            float2 c[4];
            ToCorners(np, W, H, c);
            for (int k = 0; k < 4; ++k) t.layer->smartCorners[k] = c[k];
            t.layer->smartPivot = np.pivot;
        } else {
            FxSet(*t.fx, np, W, H);
            d.SyncGraph();
        }
        d.dirty = true;
        if (label) v.history.PushState(label, before, d.S());
    };
    if (edit) apply(n, nullptr);
    auto button = [&](const char* label) {
        bool r = ImGui::SmallButton(label);
        ImGui::SameLine();
        return r;
    };
    if (button("90° anti-horaire")) { XfP q = p; q.rot -= 1.5707963f; apply(q, "Rotation 90°"); }
    if (button("90° horaire")) { XfP q = p; q.rot += 1.5707963f; apply(q, "Rotation 90°"); }
    if (button("Miroir H")) { XfP q = p; q.scale.x = -q.scale.x; apply(q, "Miroir horizontal"); }
    if (button("Miroir V")) { XfP q = p; q.scale.y = -q.scale.y; apply(q, "Miroir vertical"); }
    ImGui::NewLine();
    if (button("Centrer")) {
        XfP q = p;
        float2 c[4];
        ToCorners(p, W, H, c);
        float2 mid = (c[0] + c[2]) * 0.5f;
        q.pos = q.pos + (float2{d.Width() * 0.5f, d.Height() * 0.5f} - mid);
        apply(q, "Centrer le calque");
    }
    if (button("Taille réelle")) { XfP q = p; q.scale = {q.scale.x < 0 ? -1.0f : 1.0f, q.scale.y < 0 ? -1.0f : 1.0f}; apply(q, "Taille réelle (100 %)"); }
    if (button("Ajuster au document")) {
        XfP q;
        float k = std::min(d.Width() / W, d.Height() / H);
        q.scale = {k, k};
        q.pos = {d.Width() * 0.5f, d.Height() * 0.5f};
        apply(q, "Ajuster au document");
    }
    if (button("Réinitialiser")) {
        XfP q;
        q.pos = {d.Width() * 0.5f, d.Height() * 0.5f};
        apply(q, "Réinitialiser la transformation");
    }
    ImGui::NewLine();
    if (so) {
        int interp = l.smartInterp;
        if (ImGui::Combo("Filtrage", &interp, "Plus proche (pixels nets)\0Bilinéaire\0Bicubique (lisse)\0")) {
            DocState before = d.S();
            l.smartInterp = (u8)interp;
            v.history.PushState("Filtrage du calque", before, d.S());
        }
        ui::HelpTooltip("Échantillonnage de l'image transformée");
        if (ImGui::SmallButton("Rogner au document")) {
            // Keeps only what is inside the document: a pixel layer (the full image is dropped).
            Edit e(d, v.history, "Rogner au document");
            l.kind = LayerKind::Pixel;
            l.smart = 0;
            d.SyncGraph();
            e.Commit();
            Status("Calque rogné à la taille du document (pixels)");
            return;
        }
        ui::HelpTooltip("Par défaut l'image entière est gardée (ce qui dépasse reste récupérable) ; ceci la rogne aux limites du document.");
        ImGui::SameLine();
        ImGui::TextDisabled("Image : %u x %u", so->width, so->height);
    } else if (draw) {
        ImGui::TextDisabled(xfx ? "Filtre Transform du calque (non destructif)." : "Modifier une valeur ajoute un filtre Transform au calque (non destructif).");
        if (xfx && ImGui::SmallButton("Retirer la transformation")) {
            DocState before = d.S();
            std::erase_if(l.filters, [](const EffectInstance& f) { return f.effectId == "transform_2d"; });
            d.SyncGraph();
            v.history.PushState("Retirer la transformation", before, d.S());
            return;
        }
    } else {
        ImGui::TextDisabled("Modifier une valeur convertit le calque en objet dynamique (image entière conservée).");
    }

    // ---------------------------------------------------------------- base adjustments (non-destructive filters)
    ImGui::SeparatorText("Réglages de l'image");
    auto filterOf = [&](const char* id) -> EffectInstance* {
        for (auto& f : l.filters)
            if (f.effectId == id) return &f;
        return nullptr;
    };
    auto block = [&](const char* id, const char* label) {
        EffectInstance* fx = filterOf(id);
        EffectInstance temp;
        if (!fx) {
            auto def = Effects().Find(id);
            if (!def) return;
            temp.effectId = id;
            temp.params = def->DefaultParams();
            fx = &temp;
        }
        if (EffectParamsUI(*fx, label) && fx == &temp) {
            // First change: the filter is added to the layer (non-destructive, removable).
            DocState before = d.S();
            l.filters.push_back(temp);
            d.SyncGraph();
            v.history.PushState("Réglages de l'image", before, d.S());
        }
    };
    block("basic_image", "basic");
    // Blur / sharpness: their own filters, the main value only.
    auto slider = [&](const char* id, const char* label, float maxV, const char* fmt) {
        EffectInstance* fx = filterOf(id);
        float val = fx && !fx->params.empty() ? fx->params[0].x : 0.0f;
        if (ImGui::SliderFloat(label, &val, 0, maxV, fmt)) {
            if (!fx) {
                auto def = Effects().Find(id);
                if (!def) return;
                DocState before = d.S();
                EffectInstance f;
                f.effectId = id;
                f.params = def->DefaultParams();
                f.params[0].x = val;
                l.filters.push_back(f);
                d.SyncGraph();
                v.history.PushState(label, before, d.S());
            } else {
                fx->params[0].x = val;
            }
        }
        if (fx) TrackUndo(label);
    };
    slider("gaussian_blur", "Flou (px)", 50, "%.1f");
    slider("sharpen", "Netteté", 5, "%.2f");
    if (ImGui::SmallButton("Réinitialiser les réglages")) {
        DocState before = d.S();
        std::erase_if(l.filters, [](const EffectInstance& f) { return f.effectId == "basic_image" || f.effectId == "gaussian_blur" || f.effectId == "sharpen"; });
        d.SyncGraph();
        v.history.PushState("Réinitialiser les réglages", before, d.S());
    }
}

}  // namespace sx
