// Merge Down, Merge Visible, Flatten and bake one layer to editable pixels.
#include "ui/App.h"
#include "ui/Theme.h"
#include <imgui.h>

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
    RecordAction("merge_down");
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
    RecordAction(flatten ? "flatten" : "merge_visible");
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

bool App::CanRasterizeLayer(const Document& d, const Layer* l, std::string* why) const {
    auto reject = [&](const char* reason) { if (why) *why = reason; return false; };
    if (why) why->clear();
    if (d.Mode() != DocMode::Layers || !l) return reject("Choisissez un calque de contenu.");
    if (l->locked) return reject("Déverrouillez le calque avant de le convertir en pixels.");
    if (l->kind == LayerKind::Group) return reject("Pour un groupe, utilisez les commandes de fusion des calques.");
    if (l->kind == LayerKind::Adjustment) return reject("Un réglage dépend des calques du dessous : utilisez Fusionner vers le bas.");
    if (l->kind == LayerKind::Pixel && l->filters.empty() && l->projection.empty() && !l->material)
        return reject("Ce calque contient déjà des pixels modifiables au pinceau.");
    return true;
}

void App::RequestRasterizeLayer(DocView& v, Id layer) {
    std::string why;
    if (!CanRasterizeLayer(*v.doc, v.doc->FindLayer(layer), &why)) { Status(cyui::tr(why.c_str())); return; }
    if (std::any_of(layerBakes.begin(), layerBakes.end(), [&](const auto& r) { return r.view == &v && r.layer == layer; })) return;
    RecordAction("rasterize_layer");
    layerBakes.push_back({&v, layer, ImGui::GetTime() + 60.0});
    Status(cyui::tr("Conversion du calque en pixels en cours..."));
}

App::LayerBakeResult App::RasterizeLayerNow(DocView& v, Id layer) {
    Document& d = *v.doc;
    Layer* l = d.FindLayer(layer);
    std::string why;
    if (!CanRasterizeLayer(d, l, &why)) { Status(cyui::tr(why.c_str())); return LayerBakeResult::Failed; }
    if (v.renderer.Stroking() || (xf.active && xf.view == &v)) return LayerBakeResult::Pending;
    auto& shaders = gpu::Shaders();
    // Derived source pixels must be ready before taking the snapshot. A previous
    // preview can still be present while its replacement shader is compiling.
    const char* sourceKernel = l->kind == LayerKind::Shape ? "shape.slang" : l->kind == LayerKind::Smart ? "smart_place.slang" : nullptr;
    if (sourceKernel && !shaders.Builtin(sourceKernel)) return LayerBakeResult::Pending;
    v.renderer.Prepare(d, *ctx);
    if (!d.pendingClears.empty() || !d.pendingCopies.empty()) return LayerBakeResult::Pending;
    // Shape/smart caches may have observed a not-yet-ready source kernel earlier.
    if (sourceKernel) v.textHashes.erase(l->surface);
    SyncTextLayers(v);
    SyncSmartLayers(v);
    Id content = 0;
    int last = -2;
    for (const auto& n : d.S().graph.nodes) {
        if (n.layer != layer) continue;
        const u32 role = (u32)(n.id & 31);
        int index = LayerFilterOfRole(role);
        if (role != 0 && index < 0) continue;  // exclude mask, blend and clipped neighbours
        if (n.kind == NodeKind::Effect && !n.bypass) {
            auto def = n.fx.Def();
            if (!def || !def->error.empty()) {
                Status(cyui::tr("Conversion annulée : un effet du calque est invalide."));
                return LayerBakeResult::Failed;
            }
            auto program = shaders.Request(def->name, BuildEffectSource(*def));
            if (program->state.load() == gpu::Program::State::Failed) {
                Status(cyui::tr("Conversion annulée : un effet du calque est invalide."));
                return LayerBakeResult::Failed;
            }
            if (program->state.load() != gpu::Program::State::Ready) return LayerBakeResult::Pending;
        }
        if (index > last) { content = n.id; last = index; }
    }
    if (!content) { Status(cyui::tr("Conversion annulée : contenu du calque introuvable.")); return LayerBakeResult::Failed; }
    v.renderer.NewEvalPass();
    gpu::Texture* image = v.renderer.EvaluateNode(d, *ctx, content);
    if (!image || !v.renderer.NodeHash(content)) return LayerBakeResult::Pending;
    // Allocate a new surface instead of overwriting the old one: undo and linked
    // smart-object instances must retain their original pixels and editability.
    DocState before = d.S();
    Id surface = d.CreateSurfaceRaw(SurfaceFormat(d.GetPrecision()), "RasterizedLayer");
    Surface* dst = d.GetSurface(surface);
    if (!dst || !dst->tex) { d.S() = before; Status(cyui::tr("Conversion annulée : mémoire GPU insuffisante.")); return LayerBakeResult::Failed; }
    ctx->CopyTexture(dst->tex.get(), image);
    d.TouchSurface(surface);
    // Material masks can be shared and regenerated; keep an independent snapshot.
    Id mask = l->mask;
    if (l->material && mask) {
        Surface* src = d.GetSurface(mask);
        if (src && src->tex) {
            Id copy = d.CreateSurfaceRaw(src->tex->GetFormat(), "RasterizedMask");
            Surface* target = d.GetSurface(copy);
            if (!target || !target->tex) { d.S() = before; Status(cyui::tr("Conversion annulée : mémoire GPU insuffisante.")); return LayerBakeResult::Failed; }
            ctx->CopyTexture(target->tex.get(), src->tex.get()); d.TouchSurface(copy); mask = copy;
        }
    }
    l = d.FindLayer(layer);
    l->kind = LayerKind::Pixel;
    l->surface = surface;
    l->effect = {};
    l->filters.clear();
    l->smart = 0;
    l->text = {};
    l->shape = {};
    l->projection.clear();
    l->generator.clear();
    l->material = 0;
    l->maskEdit = 0;
    l->mask = mask;
    if (d.S().activeLayer == layer) { d.S().editMask = false; d.S().editInpaint = false; }
    d.SyncGraph();
    v.renderer.DropNodeCaches(d);
    v.history.PushState("Convertir en calque de pixels", before, d.S());
    Status(cyui::tr("Calque converti en pixels : vous pouvez peindre dessus. Ctrl+Z pour annuler."));
    return LayerBakeResult::Done;
}

void App::UpdateLayerBakes() {
    for (size_t i = 0; i < layerBakes.size();) {
        const auto& r = layerBakes[i];
        bool done = !ViewAlive(r.view);
        if (!done && ImGui::GetTime() > r.deadline) {
            Status(cyui::tr("Conversion annulée : le rendu du calque n'est pas prêt.")); done = true;
        }
        if (!done) done = RasterizeLayerNow(*r.view, r.layer) != LayerBakeResult::Pending;
        if (done) layerBakes.erase(layerBakes.begin() + i); else ++i;
    }
}

void App::RasterizeLayerUI(DocView& v, Layer& l) {
    std::string why;
    const bool available = CanRasterizeLayer(*v.doc, &l, &why);
    const bool busy = std::any_of(layerBakes.begin(), layerBakes.end(), [&](const auto& r) { return r.view == &v && r.layer == l.id; });
    ImGui::BeginDisabled(!available || busy);
    if (ImGui::Button("Convertir en calque de pixels")) RequestRasterizeLayer(v, l.id);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", cyui::tr(available ? "Intègre le contenu et les filtres dans des pixels modifiables au pinceau. Le masque, l'opacité et la fusion restent éditables. Ctrl+Z pour annuler." : why.c_str()).c_str());
}

}  // namespace sx
