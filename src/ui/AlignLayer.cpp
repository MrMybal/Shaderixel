// Layer > Align: the active layer's content lined up on the selection's box (or the canvas): left, centers, right,
// top, bottom. Pixel layers move by whole pixels through the move tool's path (one undo step); text, shapes and
// smart objects move their own geometry.
#include "ui/App.h"

#include <cmath>

namespace sx {

void App::StartAlignLayer(DocView& v, int mode) {
    Document& d = *v.doc;
    Layer* l = d.ActiveLayer();
    if (!l || d.Mode() != DocMode::Layers) return;
    if (xf.active && xf.view) TransformCommit(*xf.view);
    alignJob = {};
    alignJob.view = &v;
    alignJob.mode = mode;
    alignJob.layer = l->id;
    if (Surface* sel = d.Selection()) alignJob.selRb = v.renderer.ComputeBounds(*ctx, sel->tex.get(), nullptr, false, true);
    if (l->kind == LayerKind::Pixel)
        if (Surface* s = d.GetSurface(l->surface)) alignJob.layerRb = v.renderer.ComputeBounds(*ctx, s->tex.get(), nullptr, false);
    alignJob.active = true;
}

void App::AlignLayerTick() {
    auto& J = alignJob;
    if (!J.active) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == J.view;
    if (!alive) { J = {}; return; }
    if ((J.selRb && !J.selRb->ready) || (J.layerRb && !J.layerRb->ready)) return;
    DocView& v = *J.view;
    Document& d = *v.doc;
    Layer* l = d.FindLayer(J.layer);
    auto rectOf = [](const gpu::ReadbackPtr& rb) {
        const u32* b = (const u32*)rb->data.data();
        return rb->data.size() >= 16 && b[2] > b[0] && b[3] > b[1] ? Rect{(int)b[0], (int)b[1], (int)b[2], (int)b[3]} : Rect{};
    };
    // The target: the selection's box, else the canvas.
    Rect target = J.selRb ? rectOf(J.selRb) : Rect{};
    const bool toSel = !target.Empty();
    if (!toSel) target = {0, 0, (int)d.Width(), (int)d.Height()};
    // The layer's box.
    Rect box;
    if (l && l->kind == LayerKind::Pixel && J.layerRb) box = rectOf(J.layerRb);
    else if (l && (l->kind == LayerKind::Text || l->kind == LayerKind::Shape)) {
        if (auto it = v.textRects.find(l->surface); it != v.textRects.end()) box = it->second;
    } else if (l && l->kind == LayerKind::Smart) {
        float2 mn = l->smartCorners[0], mx = l->smartCorners[0];
        for (auto& c : l->smartCorners) {
            mn = {std::min(mn.x, c.x), std::min(mn.y, c.y)};
            mx = {std::max(mx.x, c.x), std::max(mx.y, c.y)};
        }
        box = {(int)std::lround(mn.x), (int)std::lround(mn.y), (int)std::lround(mx.x), (int)std::lround(mx.y)};
    }
    const int mode = J.mode;
    J = {};
    if (!l || box.Empty()) { Status("Aligner : calque vide ou non pris en charge"); return; }
    float dx = 0, dy = 0;
    switch (mode) {
        case 0: dx = (float)(target.x0 - box.x0); break;
        case 1: dx = std::round((target.x0 + target.x1) * 0.5f - (box.x0 + box.x1) * 0.5f); break;
        case 2: dx = (float)(target.x1 - box.x1); break;
        case 3: dy = (float)(target.y0 - box.y0); break;
        case 4: dy = std::round((target.y0 + target.y1) * 0.5f - (box.y0 + box.y1) * 0.5f); break;
        case 5: dy = (float)(target.y1 - box.y1); break;
    }
    if (dx == 0 && dy == 0) { Status("Aligner : déjà aligné"); return; }
    if (l->kind == LayerKind::Pixel) {
        // The move tool's path: the whole layer moved by whole pixels, one undo step.
        if (d.S().activeLayer != l->id) d.S().activeLayer = l->id;
        // (the whole layer: the move tool would carry only the selected pixels)
        const Id keepSel = d.S().selection;
        d.S().selection = 0;
        Nudge(v, dx, dy);
        d.S().selection = keepSel;
    } else {
        Edit e(d, v.history, "Aligner le calque");
        if (l->kind == LayerKind::Text) l->text.pos = l->text.pos + float2{dx, dy};
        else if (l->kind == LayerKind::Shape) l->shape.rect = {l->shape.rect.x + dx, l->shape.rect.y + dy, l->shape.rect.z + dx, l->shape.rect.w + dy};
        else for (auto& c : l->smartCorners) c = c + float2{dx, dy};
        d.SyncGraph();
        e.Commit();
        if (l->kind == LayerKind::Smart) { Prepare(v); SyncSmartLayers(v); }
    }
    static const char* names[] = {"à gauche", "au centre horizontal", "à droite", "en haut", "au centre vertical", "en bas"};
    Status(std::string("Calque aligné ") + names[std::clamp(mode, 0, 5)] + (toSel ? " de la sélection" : " de la zone de travail"));
}

}  // namespace sx
