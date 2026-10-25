// Layer > Align: the chosen layers' content lined up on the selection's box (or, with several layers, on the box
// around them all; or the canvas): left, centers, right, top, bottom; with three layers or more, their centers
// spread evenly (distribute). Pixel layers move by whole pixels through the move tool's path; text, shapes and
// smart objects move their own geometry. One undo step.
#include "ui/App.h"
#include "ui/Theme.h"

#include <cmath>

namespace sx {

namespace {
Rect RectOf(const gpu::ReadbackPtr& rb) {
    if (!rb || !rb->ready) return {};
    const u32* b = (const u32*)rb->data.data();
    return rb->data.size() >= 16 && b[2] > b[0] && b[3] > b[1] ? Rect{(int)b[0], (int)b[1], (int)b[2], (int)b[3]} : Rect{};
}
}  // namespace

Rect App::LayerBox(DocView& v, const Layer& l, const gpu::ReadbackPtr& rb) {
    if (l.kind == LayerKind::Pixel) return RectOf(rb);
    if (l.kind == LayerKind::Shape) return ShapeBox(l.shape);
    if (l.kind == LayerKind::Text) {
        auto it = v.textRects.find(l.surface);
        return it != v.textRects.end() ? it->second : Rect{};
    }
    if (l.kind == LayerKind::Smart) {
        float2 mn = l.smartCorners[0], mx = l.smartCorners[0];
        for (auto& c : l.smartCorners) {
            mn = {std::min(mn.x, c.x), std::min(mn.y, c.y)};
            mx = {std::max(mx.x, c.x), std::max(mx.y, c.y)};
        }
        return {(int)std::lround(mn.x), (int)std::lround(mn.y), (int)std::lround(mx.x), (int)std::lround(mx.y)};
    }
    return {};
}

bool App::MoveLayerBy(DocView& v, Id id, float2 delta, bool& smart) {
    Document& d = *v.doc;
    Layer* l = d.FindLayer(id);
    if (!l || l->locked || (delta.x == 0 && delta.y == 0)) return false;
    const float dx = delta.x, dy = delta.y;
    if (l->kind == LayerKind::Pixel) {
        // The move tool's path: the whole layer moved by whole pixels (its pixels only in the history: the active
        // layer is put back right after, so the steps around never record the borrowed one).
        if (xf.active) return false;  // (a transform opened meanwhile is not ours to move)
        const Id keepActive = d.S().activeLayer;
        d.S().activeLayer = l->id;
        // (the whole layer: the move tool would carry only the selected pixels)
        const Id keepSel = d.S().selection;
        d.S().selection = 0;
        const bool ok = d.PaintTarget() == l->surface;
        if (ok) Nudge(v, dx, dy);
        d.S().selection = keepSel;
        d.S().activeLayer = keepActive;
        return ok;
    }
    if (l->kind != LayerKind::Text && l->kind != LayerKind::Shape && l->kind != LayerKind::Smart) return false;
    Edit e(d, v.history, "Déplacer le calque");
    if (l->kind == LayerKind::Text) l->text.pos = l->text.pos + float2{dx, dy};
    else if (l->kind == LayerKind::Shape) {
        l->shape.rect = {l->shape.rect.x + dx, l->shape.rect.y + dy, l->shape.rect.z + dx, l->shape.rect.w + dy};
        for (auto& p : l->shape.parts) p.rect = {p.rect.x + dx, p.rect.y + dy, p.rect.z + dx, p.rect.w + dy};  // (the whole combined shape)
    }
    else for (auto& c : l->smartCorners) c = c + float2{dx, dy};
    d.SyncGraph();
    e.Commit();
    smart |= l->kind == LayerKind::Smart;
    return true;
}

void App::CarryBegin(DocView& v) {
    carry = {};
    Document& d = *v.doc;
    if (d.S().selection || d.Mode() != DocMode::Layers) return;  // (a pixel selection: only it moves)
    for (Id id : d.WithLinked(d.SelectedLayers())) {  // (the chosen layers and the ones linked to them)
        const Layer* l = d.FindLayer(id);
        if (!l || id == d.S().activeLayer || l->kind == LayerKind::Group || l->locked) continue;
        carry.ids.push_back(id);
        gpu::ReadbackPtr rb;
        if (l->kind == LayerKind::Pixel)
            if (Surface* s = d.GetSurface(l->surface)) rb = v.renderer.ComputeBounds(*ctx, s->tex.get(), nullptr, false);
        carry.rbs.push_back(rb);
        carry.boxes.push_back(rb ? Rect{} : LayerBox(v, *l, nullptr));
    }
    if (!carry.ids.empty()) carry.view = &v;
}

void App::CarryEnd(DocView& v) {
    if (carry.view != &v || carry.ids.empty()) { carry = {}; return; }
    Document& d = *v.doc;
    const float2 delta = {std::round(carry.delta.x), std::round(carry.delta.y)};
    const auto ids = carry.ids;
    carry = {};
    if (delta.x == 0 && delta.y == 0) return;
    bool smart = false;
    for (Id id : ids) MoveLayerBy(v, id, delta, smart);
    if (smart) { Prepare(v); SyncSmartLayers(v); }
}

void App::CarryOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (carry.view != &v || carry.ids.empty()) return;
    if (!(xf.active && xf.drag >= 0) && !shapeDragging && !textDragging) return;  // (only while dragging; the release moves them)
    for (size_t i = 0; i < carry.ids.size(); ++i) {
        if (carry.boxes[i].Empty() && carry.rbs[i] && carry.rbs[i]->ready) {
            carry.boxes[i] = RectOf(carry.rbs[i]);
            carry.rbs[i].reset();
        }
        const Rect& r = carry.boxes[i];
        if (r.Empty()) continue;
        const ImVec2 a(canvasMin.x + v.pan.x + (r.x0 + carry.delta.x) * v.zoom, canvasMin.y + v.pan.y + (r.y0 + carry.delta.y) * v.zoom);
        const ImVec2 b(canvasMin.x + v.pan.x + (r.x1 + carry.delta.x) * v.zoom, canvasMin.y + v.pan.y + (r.y1 + carry.delta.y) * v.zoom);
        dl->AddRect(a, b, IM_COL32(0, 0, 0, 140), 0, 0, 3.0f);
        dl->AddRect(a, b, ui::col::Accent2, 0, 0, 1.5f);
    }
}

void App::StartAlignLayer(DocView& v, int mode) {
    Document& d = *v.doc;
    if (!d.ActiveLayer() || d.Mode() != DocMode::Layers) return;
    if (d.S().editMask || d.S().editInpaint) { Status("Aligner : quittez l'édition du masque d'abord"); return; }
    if (xf.active && xf.view) TransformCommit(*xf.view);
    std::vector<Id> ids;
    for (Id id : d.SelectedLayers()) {
        const Layer* l = d.FindLayer(id);
        if (l && l->kind != LayerKind::Group && !l->locked) ids.push_back(id);  // (a group: choose its layers)
    }
    if (ids.empty()) { Status("Aligner : aucun calque déplaçable (verrouillé ou groupe)"); return; }
    if (mode >= 6 && ids.size() < 3) { Status("Répartir : choisissez au moins trois calques"); return; }
    alignJob = {};
    alignJob.view = &v;
    alignJob.mode = mode;
    alignJob.layers = ids;
    if (Surface* sel = d.Selection(); sel && mode < 6) alignJob.selRb = v.renderer.ComputeBounds(*ctx, sel->tex.get(), nullptr, false, true);
    for (Id id : ids) {
        const Layer* l = d.FindLayer(id);
        if (l->kind == LayerKind::Pixel)
            if (Surface* s = d.GetSurface(l->surface)) alignJob.layerRb[id] = v.renderer.ComputeBounds(*ctx, s->tex.get(), nullptr, false);
    }
    alignJob.active = true;
}

void App::AlignLayerTick() {
    auto& J = alignJob;
    if (!J.active) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == J.view;
    if (!alive) { J = {}; return; }
    if ((xf.active && xf.drag >= 0) || shapeDragging || textDragging) return;  // (a drag under way ends first)
    if (J.selRb && !J.selRb->ready) return;
    for (auto& [id, rb] : J.layerRb)
        if (rb && !rb->ready) return;
    DocView& v = *J.view;
    Document& d = *v.doc;
    if (xf.active && xf.view) TransformCommit(*xf.view);  // (a transform opened while the bounds were read)
    // Each layer's box.
    struct Item { Id id; Rect box; };
    std::vector<Item> items;
    for (Id id : J.layers) {
        const Layer* l = d.FindLayer(id);
        if (!l) continue;
        auto it = J.layerRb.find(id);
        const Rect box = LayerBox(v, *l, it != J.layerRb.end() ? it->second : nullptr);
        if (!box.Empty()) items.push_back({id, box});
    }
    const int mode = J.mode;
    // The target: the selection's box, else the box around the chosen layers (several), else the canvas.
    Rect target = J.selRb ? RectOf(J.selRb) : Rect{};
    const bool toSel = !target.Empty();
    const bool among = !toSel && J.layers.size() > 1;
    J = {};
    if (items.empty()) { Status("Aligner : calque vide ou non pris en charge"); return; }
    if (mode >= 6 && items.size() < 3) { Status("Répartir : il faut au moins trois calques non vides"); return; }
    if (among) {
        target = items[0].box;
        for (auto& it : items)
            target = {std::min(target.x0, it.box.x0), std::min(target.y0, it.box.y0), std::max(target.x1, it.box.x1), std::max(target.y1, it.box.y1)};
    } else if (!toSel) {
        target = {0, 0, (int)d.Width(), (int)d.Height()};
    }
    // The moves.
    std::vector<float2> moves(items.size());
    if (mode >= 6) {
        // Distribute: the centers spread evenly between the outermost two (which stay).
        const bool hz = mode == 6;
        auto center = [&](const Rect& r) { return hz ? (r.x0 + r.x1) * 0.5f : (r.y0 + r.y1) * 0.5f; };
        std::vector<size_t> order(items.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return center(items[a].box) < center(items[b].box); });
        const float c0 = center(items[order.front()].box), c1 = center(items[order.back()].box);
        for (size_t k = 1; k + 1 < order.size(); ++k) {
            const float want = c0 + (c1 - c0) * (float)k / (float)(order.size() - 1);
            const float delta = std::round(want - center(items[order[k]].box));
            moves[order[k]] = hz ? float2{delta, 0} : float2{0, delta};
        }
    } else {
        for (size_t i = 0; i < items.size(); ++i) {
            const Rect& box = items[i].box;
            float dx = 0, dy = 0;
            switch (mode) {
                case 0: dx = (float)(target.x0 - box.x0); break;
                case 1: dx = std::round((target.x0 + target.x1) * 0.5f - (box.x0 + box.x1) * 0.5f); break;
                case 2: dx = (float)(target.x1 - box.x1); break;
                case 3: dy = (float)(target.y0 - box.y0); break;
                case 4: dy = std::round((target.y0 + target.y1) * 0.5f - (box.y0 + box.y1) * 0.5f); break;
                case 5: dy = (float)(target.y1 - box.y1); break;
            }
            moves[i] = {dx, dy};
        }
    }
    const Id keepActive = d.S().activeLayer;
    const auto keepChosen = d.S().layerSel;
    int moved = 0, refused = 0;
    bool smart = false;
    v.history.BeginGroup();
    for (size_t i = 0; i < items.size(); ++i) {
        if (moves[i].x == 0 && moves[i].y == 0) continue;
        ++(MoveLayerBy(v, items[i].id, moves[i], smart) ? moved : refused);
    }
    v.history.EndGroup();
    // The chosen layers stay chosen, the active one too.
    if (d.FindLayer(keepActive)) d.S().activeLayer = keepActive;
    d.S().layerSel = keepChosen;
    if (smart) { Prepare(v); SyncSmartLayers(v); }
    if (!moved) { Status(refused ? "Aligner : ce calque ne peut pas être déplacé" : "Aligner : déjà aligné"); return; }
    if (mode >= 6) { Status(std::to_string(items.size()) + (mode == 6 ? " calques répartis horizontalement" : " calques répartis verticalement")); return; }
    static const char* names[] = {"à gauche", "au centre horizontal", "à droite", "en haut", "au centre vertical", "en bas"};
    Status(std::string(items.size() > 1 ? "Calques alignés " : "Calque aligné ") + names[std::clamp(mode, 0, 5)] +
           (toSel ? " de la sélection" : among ? " entre eux" : " de la zone de travail"));
}

}  // namespace sx
