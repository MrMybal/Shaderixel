// Image > Apply Image (like Photoshop's): a layer (or the merged image) of an open document of the same size, one
// of its channels or RGB, inverted or not, blended into the active pixel layer with a mode and an opacity (inside
// the selection when there is one, the layer's transparency kept if asked); one undo step.
#include "ui/App.h"
#include "render/Dispatch.h"

namespace sx {

void App::DrawApplyImageDialog() {
    auto& A = applyImage;
    if (!A.open) return;
    DocView* v = Active();
    if (!v) { A.open = false; return; }
    Document& d = *v->doc;
    ImGui::SetNextWindowSize(ImVec2(400 * dpi, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Appliquer une image", &A.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    // Source document: the open ones of the same size.
    std::vector<DocView*> same;
    for (auto& dv : docs)
        if (dv->doc->Width() == d.Width() && dv->doc->Height() == d.Height() && dv->doc->Mode() == DocMode::Layers) same.push_back(dv.get());
    bool found = false;
    for (DocView* s : same) found |= s == A.source;
    if (!found) A.source = v, A.layer = 0;
    ImGui::SetNextItemWidth(250 * dpi);
    if (ImGui::BeginCombo("Source", A.source->doc->path.empty() ? "(document sans nom)" : Narrow(Utf8Path(A.source->doc->path).filename().wstring()).c_str())) {
        for (DocView* s : same) {
            const std::string n = s->doc->path.empty() ? "(document sans nom)" : Narrow(Utf8Path(s->doc->path).filename().wstring());
            ImGui::PushID(s);
            if (ImGui::Selectable(n.c_str(), s == A.source)) { A.source = s; A.layer = 0; }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    Document& sd = *A.source->doc;
    const Layer* sl = sd.FindLayer(A.layer);
    ImGui::SetNextItemWidth(250 * dpi);
    if (ImGui::BeginCombo("Calque", sl ? sl->name.c_str() : "Fusionné")) {
        if (ImGui::Selectable("Fusionné", A.layer == 0)) A.layer = 0;
        for (auto it = sd.S().layers.rbegin(); it != sd.S().layers.rend(); ++it)
            if (it->kind == LayerKind::Pixel && ImGui::Selectable((it->name + "##" + std::to_string(it->id)).c_str(), A.layer == it->id)) A.layer = it->id;
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(250 * dpi);
    ImGui::Combo("Couche", &A.channel, "RVB\0Rouge\0Vert\0Bleu\0Transparence\0");
    ImGui::Checkbox("Inverser", &A.invert);
    ImGui::Separator();
    ImGui::SetNextItemWidth(250 * dpi);
    if (ImGui::BeginCombo("Mode", BlendModeName((BlendMode)A.mode))) {
        for (int m = 0; m < (int)BlendMode::Count; ++m)
            if (ImGui::Selectable(BlendModeName((BlendMode)m), m == A.mode)) A.mode = m;
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(250 * dpi);
    ImGui::SliderFloat("Opacité", &A.opacity, 0, 1, "%.2f");
    ImGui::Checkbox("Conserver la transparence", &A.preserve);
    const Layer* target = d.ActiveLayer();
    const bool ok = target && target->kind == LayerKind::Pixel && d.GetSurface(target->surface) && !d.S().editMask;
    if (!ok) ImGui::TextDisabled("Sélectionnez un calque de pixels (la cible).");
    else ImGui::TextDisabled("Cible : %s%s", target->name.c_str(), d.S().selection ? " (dans la sélection)" : "");
    ImGui::BeginDisabled(!ok);
    if (ImGui::Button("Appliquer", ImVec2(120 * dpi, 0))) {
        ApplyImageRun(*v);
        A.open = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Fermer", ImVec2(100 * dpi, 0))) A.open = false;
    ImGui::End();
}

void App::ApplyImageRun(DocView& v) {
    auto& A = applyImage;
    Document& d = *v.doc;
    Layer* target = d.ActiveLayer();
    Surface* ts = target ? d.GetSurface(target->surface) : nullptr;
    auto* k = Kernel("apply_image.slang");
    if (!ts || !k || !A.source) return;
    if (xf.active && xf.view) TransformCommit(*xf.view);
    // The source image: a layer's pixels, or the merged image of its document (copied: renderers reuse textures).
    Document& sd = *A.source->doc;
    gpu::Texture* src = nullptr;
    if (const Layer* sl = sd.FindLayer(A.layer); sl && sd.GetSurface(sl->surface)) src = sd.GetSurface(sl->surface)->tex.get();
    else src = A.source->renderer.EvaluateComplete(sd, *ctx);
    if (!src || src->Width() != ts->tex->Width() || src->Height() != ts->tex->Height()) { Status("Appliquer une image : la source doit avoir la même taille"); return; }
    const u32 W = ts->tex->Width(), H = ts->tex->Height();
    struct Root { u32 w, h, pass, channel, invert, pad[3]; };
    auto prepared = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA16F, "ApplyImageSrc");
    {
        gpu::DispatchDesc dd;
        dd.pipeline = k;
        dd.srv[0] = src;
        dd.uav[0] = prepared.get();
        SetConstants(dd, Root{W, H, 0, (u32)A.channel, A.invert ? 1u : 0u, {0, 0, 0}});
        Dispatch2D(*ctx, dd, W, H);
    }
    // Blended into a copy of the layer (the selection as mask), then the transparency kept if asked.
    auto before = gpu::Dev().CreateTexture(W, H, ts->tex->GetFormat(), "UndoBefore");
    ctx->CopyTexture(before.get(), ts->tex.get());
    auto blended = gpu::Dev().CreateTexture(W, H, ts->tex->GetFormat(), "ApplyImage");
    Surface* sel = d.Selection();
    v.renderer.Composite(d, *ctx, blended.get(), before.get(), prepared.get(), sel ? sel->tex.get() : nullptr, (BlendMode)A.mode, A.opacity, false, nullptr);
    if (A.preserve) {
        gpu::DispatchDesc dd;
        dd.pipeline = k;
        dd.srv[0] = blended.get();
        dd.srv[1] = before.get();
        dd.uav[0] = ts->tex.get();
        SetConstants(dd, Root{W, H, 1, 0, 0, {0, 0, 0}});
        Dispatch2D(*ctx, dd, W, H);
    } else {
        ctx->CopyTexture(ts->tex.get(), blended.get());
    }
    auto after = gpu::Dev().CreateTexture(W, H, ts->tex->GetFormat(), "UndoAfter");
    ctx->CopyTexture(after.get(), ts->tex.get());
    v.history.PushPaint("Appliquer une image", target->surface, {0, 0, (int)W, (int)H}, before, after);
    d.TouchSurface(target->surface);
    d.dirty = true;
    Status(std::string("Image appliquée (") + BlendModeName((BlendMode)A.mode) + ")");
}

}  // namespace sx
