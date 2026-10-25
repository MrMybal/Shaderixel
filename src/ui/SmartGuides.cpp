// Smart guides (View > Smart guides): while a layer is moved, the edges and the center of its box snap to those of
// the other layers and of the canvas, and magenta lines show what it lines up with. Ctrl while dragging: off.
#include "ui/App.h"

#include <cmath>

namespace sx {

Rect App::ShapeBox(const ShapeStyle& s) {
    float2 mn = {1e30f, 1e30f}, mx = {-1e30f, -1e30f};
    auto add = [&](const float4& r, float degrees) {
        const float2 c = {(r.x + r.z) * 0.5f, (r.y + r.w) * 0.5f}, h = {std::abs(r.z - r.x) * 0.5f, std::abs(r.w - r.y) * 0.5f};
        const float a = degrees * 3.14159265f / 180.0f, ca = std::abs(std::cos(a)), sa = std::abs(std::sin(a));
        const float2 e = {h.x * ca + h.y * sa, h.x * sa + h.y * ca};  // the rotated frame's half extents
        mn = {std::min(mn.x, c.x - e.x), std::min(mn.y, c.y - e.y)};
        mx = {std::max(mx.x, c.x + e.x), std::max(mx.y, c.y + e.y)};
    };
    add(s.rect, s.type == 2 ? 0 : s.rotation);  // (a line: its two ends)
    for (const ShapePart& p : s.parts)
        if (p.op == 0 || p.op == 3) add(p.rect, p.rotation);
    // The stroke when drawn; a lone line always has its width (drawn as a thick segment).
    const float pad = s.type == 2 ? std::max(s.strokeWidth, 1.0f) * 0.5f : s.strokeOn ? s.strokeWidth * 0.5f : 0.0f;
    return {(int)std::floor(mn.x - pad), (int)std::floor(mn.y - pad), (int)std::ceil(mx.x + pad), (int)std::ceil(mx.y + pad)};
}

void App::SmartGuidesCollect(DocView& v, Id exclude, Id selection) {
    sg.rects.clear();
    sg.pending.clear();
    sg.view = &v;
    sg.self = {};
    sg.selfRb.reset();
    Document& d = *v.doc;
    // The moved content: the surface's pixels (inside the selection when one is moved).
    Surface* selS = selection ? d.GetSurface(selection) : nullptr;
    if (Surface* me = d.GetSurface(exclude)) sg.selfRb = v.renderer.ComputeBounds(*ctx, me->tex.get(), selS ? selS->tex.get() : nullptr, me->tex->GetFormat() == gpu::Format::R8, me->tex->GetFormat() == gpu::Format::R8);
    if (d.Mode() != DocMode::Layers) return;
    const Layer* active = d.ActiveLayer();
    auto shown = [&](const Layer& l) {
        // Visible, and every enclosing group too.
        if (!l.visible) return false;
        for (Id p = l.parent; p;) {
            const Layer* g = d.FindLayer(p);
            if (!g || !g->visible) return false;
            p = g->parent;
        }
        return true;
    };
    for (auto& l : d.S().layers) {
        if (!shown(l) || l.surface == exclude || (active && l.id == active->id)) continue;
        if (l.kind == LayerKind::Smart) {
            float2 mn = l.smartCorners[0], mx = l.smartCorners[0];
            for (auto& c : l.smartCorners) {
                mn = {std::min(mn.x, c.x), std::min(mn.y, c.y)};
                mx = {std::max(mx.x, c.x), std::max(mx.y, c.y)};
            }
            sg.rects.push_back({(int)std::floor(mn.x), (int)std::floor(mn.y), (int)std::ceil(mx.x), (int)std::ceil(mx.y)});
        } else if (l.kind == LayerKind::Shape) {
            sg.rects.push_back(ShapeBox(l.shape));
        } else if (l.kind == LayerKind::Text) {
            if (auto it = v.textRects.find(l.surface); it != v.textRects.end() && !it->second.Empty()) sg.rects.push_back(it->second);
        } else if (l.kind == LayerKind::Pixel) {
            if (Surface* s = d.GetSurface(l.surface))
                if (auto rb = v.renderer.ComputeBounds(*ctx, s->tex.get(), nullptr, false)) sg.pending.push_back(rb);
        }
    }
}

float2 App::SmartGuidesSnap(DocView& v, float2 center, float2 half) {
    sg.lineX = sg.lineY = NAN;
    if (sg.view != &v) return center;
    // Bounds read back since the click.
    for (size_t i = 0; i < sg.pending.size();) {
        auto& rb = sg.pending[i];
        if (!rb->ready) { ++i; continue; }
        const u32* b = (const u32*)rb->data.data();
        if (rb->data.size() >= 16 && b[2] > b[0] && b[3] > b[1]) sg.rects.push_back({(int)b[0], (int)b[1], (int)b[2], (int)b[3]});
        sg.pending.erase(sg.pending.begin() + i);
    }
    if (sg.selfRb && sg.selfRb->ready) {
        const u32* b = (const u32*)sg.selfRb->data.data();
        if (sg.selfRb->data.size() >= 16 && b[2] > b[0] && b[3] > b[1]) sg.self = {(int)b[0], (int)b[1], (int)b[2], (int)b[3]};
        sg.selfRb.reset();
    }
    const Document& d = *v.doc;
    const float tol = 7.0f / std::max(v.zoom, 1e-3f);
    std::vector<Rect> targets = sg.rects;
    targets.push_back({0, 0, (int)d.Width(), (int)d.Height()});  // the canvas
    auto axis = [&](float c, float hs, bool xAxis, float& line, Rect& with) {
        // The box's left / center / right (or top / middle / bottom) against every target's.
        const float mine[3] = {c - hs, c, c + hs};
        float best = tol, shift = 0;
        for (const Rect& r : targets) {
            const float lo = xAxis ? (float)r.x0 : (float)r.y0, hi = xAxis ? (float)r.x1 : (float)r.y1;
            const float theirs[3] = {lo, (lo + hi) * 0.5f, hi};
            for (float m : mine)
                for (float t : theirs)
                    if (std::abs(t - m) < best) {
                        best = std::abs(t - m);
                        shift = t - m;
                        line = t;
                        with = r;
                    }
        }
        return shift;
    };
    Rect wx, wy;
    float lx = NAN, ly = NAN;
    const float dx = axis(center.x, half.x, true, lx, wx), dy = axis(center.y, half.y, false, ly, wy);
    center = center + float2{dx, dy};
    sg.lineX = lx;
    sg.lineY = ly;
    sg.withX = wx;
    sg.withY = wy;
    sg.box = Rect{(int)std::floor(center.x - half.x), (int)std::floor(center.y - half.y), (int)std::ceil(center.x + half.x), (int)std::ceil(center.y + half.y)};
    return center;
}

float2 App::SmartGuidesVector(DocView& v, float2 delta, bool shift) {
    if (!smartGuides || !snapEnabled || ImGui::GetIO().KeyCtrl || sg.vectorStart.Empty() || sg.view != &v) {
        sg.lineX = sg.lineY = NAN;
        return delta;
    }
    const Rect& r = sg.vectorStart;
    const float2 half = {r.Width() * 0.5f, r.Height() * 0.5f}, c = float2{(r.x0 + r.x1) * 0.5f, (r.y0 + r.y1) * 0.5f} + delta;
    float2 d = SmartGuidesSnap(v, c, half) - c;
    if (shift) {
        if (delta.x == 0) { d.x = 0; sg.lineX = NAN; }
        if (delta.y == 0) { d.y = 0; sg.lineY = NAN; }
    }
    return delta + d;
}

void App::SmartGuidesOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (sg.vectorDrag && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        // A text / shape drag that ended anywhere (tool change, released elsewhere): no lines left behind.
        sg.vectorDrag = false;
        sg.lineX = sg.lineY = NAN;
    }
    if (sg.view != &v) return;
    if (!sg.vectorDrag && (!xf.active || xf.view != &v || xf.drag != 8)) return;
    auto S = [&](float x, float y) { return ImVec2(canvasMin.x + v.pan.x + x * v.zoom, canvasMin.y + v.pan.y + y * v.zoom); };
    const ImU32 col = IM_COL32(255, 40, 200, 230);
    if (!std::isnan(sg.lineX)) {
        // From the top of both boxes to the bottom of both.
        const float y0 = (float)std::min(sg.box.y0, sg.withX.y0), y1 = (float)std::max(sg.box.y1, sg.withX.y1);
        dl->AddLine(S(sg.lineX, y0), S(sg.lineX, y1), col, 1.2f);
    }
    if (!std::isnan(sg.lineY)) {
        const float x0 = (float)std::min(sg.box.x0, sg.withY.x0), x1 = (float)std::max(sg.box.x1, sg.withY.x1);
        dl->AddLine(S(x0, sg.lineY), S(x1, sg.lineY), col, 1.2f);
    }
}

}  // namespace sx
