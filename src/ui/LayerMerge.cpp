// Merge Down, Merge Visible, Flatten.
#include "ui/App.h"

namespace sx {

namespace {
// Content node of a layer in the generated graph = top input of its blend node.
Id ContentNode(Document& doc, Id layer) {
    for (auto& n : doc.S().graph.nodes)
        if (n.kind == NodeKind::Blend && n.layer == layer && n.inputs.size() > 1) return n.inputs[1];
    return 0;
}

void MakePixelLayer(Layer& l, Id surface) {
    l.kind = LayerKind::Pixel;
    l.surface = surface;
    l.effect = {};
    l.filters.clear();
    l.mask = 0;
    l.maskEnabled = true;
    l.text = {};
    l.shape = {};
}
}  // namespace

void App::MergeDown(DocView& v) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) return;
    Layer* L = d.ActiveLayer();
    if (!L) return;
    int s = d.BlockStart(L->id);
    Layer* B = s > 0 && d.S().layers[s - 1].parent == L->parent ? &d.S().layers[s - 1] : nullptr;
    if (!B) { Status("Aucun calque en dessous à fusionner"); return; }
    if (L->kind == LayerKind::Group || B->kind == LayerKind::Group) { Status("Fusion des groupes : utilisez Fusionner les calques visibles"); return; }
    if (B->kind == LayerKind::Adjustment) { Status("Impossible de fusionner sur un calque de réglage"); return; }
    const u32 W = d.Width(), H = d.Height();
    const gpu::Format fmt = SurfaceFormat(d.GetPrecision());
    Renderer& r = v.renderer;
    r.EvaluateComplete(d, *ctx);
    gpu::Texture* bContent = r.EvaluateNode(d, *ctx, ContentNode(d, B->id));
    Surface* bMask = B->mask && B->maskEnabled ? d.GetSurface(B->mask) : nullptr;
    Surface* lMask = L->mask && L->maskEnabled ? d.GetSurface(L->mask) : nullptr;
    // Lower layer with its mask applied.
    auto lower = gpu::Dev().CreateTexture(W, H, fmt, "MergeLower");
    if (!lower) return;
    r.Composite(d, *ctx, lower.get(), nullptr, bContent, bMask ? bMask->tex.get() : nullptr, BlendMode::Normal, 1.0f, false, nullptr);
    // Upper content: adjustments apply to the lower layer only (like Photoshop).
    gpu::TexturePtr adjusted;
    gpu::Texture* lContent = nullptr;
    if (L->kind == LayerKind::Adjustment) {
        adjusted = r.ApplyEffect(d, *ctx, L->effect, lower.get());
        for (auto& f : L->filters)
            if (f.enabled && adjusted) adjusted = r.ApplyEffect(d, *ctx, f, adjusted.get());
        if (!adjusted) { Status("Effet en cours de compilation, réessayez"); return; }
        lContent = adjusted.get();
    } else {
        lContent = r.EvaluateNode(d, *ctx, ContentNode(d, L->id));
    }
    auto result = gpu::Dev().CreateTexture(W, H, fmt, "MergeResult");
    if (!result) return;
    r.Composite(d, *ctx, result.get(), lower.get(), L->visible ? lContent : nullptr, lMask ? lMask->tex.get() : nullptr, L->blend, L->opacity,
                L->clipped, &L->blendIf);
    r.DropNodeCaches(d);

    Edit e(d, v.history, "Fusionner vers le bas");
    Id surface = d.CreateSurfaceRaw(fmt, "Surface");
    ctx->CopyTexture(d.GetSurface(surface)->tex.get(), result.get());
    d.TouchSurface(surface);
    Id lid = L->id, bid = B->id;
    if (Layer* b = d.FindLayer(bid)) MakePixelLayer(*b, surface);
    d.RemoveLayer(lid);
    d.S().activeLayer = bid;
    d.SyncGraph();
    e.Commit();
}

void App::MergeVisible(DocView& v, bool flatten) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers || d.S().layers.empty()) return;
    const u32 W = d.Width(), H = d.Height();
    const gpu::Format fmt = SurfaceFormat(d.GetPrecision());
    gpu::Texture* image = v.renderer.EvaluateComplete(d, *ctx);
    if (!image) return;
    Edit e(d, v.history, flatten ? "Aplatir l'image" : "Fusionner les calques visibles");
    Id surface = d.CreateSurfaceRaw(fmt, "Surface");
    gpu::Texture* dst = d.GetSurface(surface)->tex.get();
    if (flatten) {
        // Transparent areas become white, like Photoshop's background.
        auto white = gpu::Dev().CreateTexture(W, H, fmt, "White");
        v.renderer.FillTexture(*ctx, white.get(), {1, 1, 1, 1});
        v.renderer.Composite(d, *ctx, dst, white.get(), image, nullptr, BlendMode::Normal, 1.0f, false, nullptr);
    } else {
        ctx->CopyTexture(dst, image);
    }
    d.TouchSurface(surface);
    // Effective visibility (a layer inside a hidden group is hidden).
    auto visible = [&](const Layer& l) {
        for (const Layer* p = &l; p; p = p->parent ? d.FindLayer(p->parent) : nullptr)
            if (!p->visible) return false;
        return true;
    };
    std::vector<Layer> keep;
    for (const Layer& l : d.S().layers) {
        if (flatten || visible(l)) continue;
        keep.push_back(l);
    }
    // Groups left without children disappear; hidden layers move to the root when their group merged.
    std::vector<Layer> out;
    for (Layer& l : keep) {
        if (l.parent && !d.FindLayer(l.parent)) l.parent = 0;
        bool parentKept = !l.parent || std::any_of(keep.begin(), keep.end(), [&](const Layer& k) { return k.id == l.parent; });
        if (!parentKept) l.parent = 0;
        out.push_back(l);
    }
    std::erase_if(out, [&](const Layer& l) {
        return l.kind == LayerKind::Group && std::none_of(out.begin(), out.end(), [&](const Layer& c) { return c.parent == l.id; });
    });
    Layer merged;
    merged.id = d.NewId();
    merged.name = flatten ? "Arrière-plan" : "Fusionné";
    MakePixelLayer(merged, surface);
    if (flatten) out.clear();
    out.push_back(merged);
    d.S().layers = out;
    d.S().activeLayer = merged.id;
    d.S().editMask = false;
    d.SyncGraph();
    e.Commit();
}

}  // namespace sx
