// Image > Calculations (like Photoshop's): two channels (gray, red, green, blue or transparency) of layers or merged
// images of open documents of the same size, each inverted or not, blended with a mode and an opacity (source 1
// over source 2); the result becomes a selection, a new gray layer or a new document.
#include "ui/App.h"
#include "render/Dispatch.h"

namespace sx {

namespace {
const char* kCalcChannels = "Gris\0Rouge\0Vert\0Bleu\0Transparence\0";
// Dialog channel -> apply_image.slang channel (5 gray, 1 red, 2 green, 3 blue, 4 alpha).
u32 ShaderChannel(int c) { return c == 0 ? 5u : (u32)c; }
std::string DocName(const DocView* v) {
    return v->doc->path.empty() ? "(document sans nom)" : Narrow(Utf8Path(v->doc->path).filename().wstring());
}
}  // namespace

void App::DrawCalculationsDialog() {
    auto& C = calculations;
    if (!C.open) return;
    DocView* v = Active();
    if (!v || v->doc->Mode() != DocMode::Layers) { C.open = false; return; }  // (a node document: no layers to add)
    Document& d = *v->doc;
    ImGui::SetNextWindowSize(ImVec2(420 * dpi, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Calculs", &C.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    std::vector<DocView*> same;
    for (auto& dv : docs)
        if (dv->doc->Width() == d.Width() && dv->doc->Height() == d.Height() && dv->doc->Mode() == DocMode::Layers) same.push_back(dv.get());
    for (int k = 0; k < 2; ++k) {
        auto& S = C.src[k];
        ImGui::PushID(k);
        ImGui::SeparatorText(k == 0 ? "Source 1 (dessus)" : "Source 2 (dessous)");
        bool found = false;
        for (DocView* s : same) found |= s == S.view;
        if (!found) S.view = v, S.layer = 0;
        ImGui::SetNextItemWidth(260 * dpi);
        if (ImGui::BeginCombo("Document", DocName(S.view).c_str())) {
            for (DocView* s : same) {
                ImGui::PushID(s);
                if (ImGui::Selectable(DocName(s).c_str(), s == S.view)) { S.view = s; S.layer = 0; }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        Document& sd = *S.view->doc;
        const Layer* sl = sd.FindLayer(S.layer);
        ImGui::SetNextItemWidth(260 * dpi);
        if (ImGui::BeginCombo("Calque", sl ? sl->name.c_str() : "Fusionné")) {
            if (ImGui::Selectable("Fusionné", S.layer == 0)) S.layer = 0;
            for (auto it = sd.S().layers.rbegin(); it != sd.S().layers.rend(); ++it)
                if (it->kind == LayerKind::Pixel && ImGui::Selectable((it->name + "##" + std::to_string(it->id)).c_str(), S.layer == it->id)) S.layer = it->id;
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(260 * dpi);
        ImGui::Combo("Couche", &S.channel, kCalcChannels);
        ImGui::Checkbox("Inverser", &S.invert);
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::SetNextItemWidth(260 * dpi);
    if (ImGui::BeginCombo("Mode", BlendModeName((BlendMode)C.mode))) {
        for (int m = 0; m < (int)BlendMode::Count; ++m)
            if (ImGui::Selectable(BlendModeName((BlendMode)m), m == C.mode)) C.mode = m;
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::SliderFloat("Opacité", &C.opacity, 0, 1, "%.2f");
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::Combo("Résultat", &C.result, "Sélection\0Nouveau calque\0Nouveau document\0");
    if (ImGui::Button("OK", ImVec2(120 * dpi, 0))) {
        CalculationsRun(*v);
        C.open = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0))) C.open = false;
    ImGui::End();
}

void App::CalculationsRun(DocView& v) {
    auto& C = calculations;
    if (xf.active && xf.view) TransformCommit(*xf.view);
    auto* k = Kernel("apply_image.slang");
    Document& d = *v.doc;
    const u32 W = d.Width(), H = d.Height();
    if (!k || !C.src[0].view || !C.src[1].view) return;
    struct Root { u32 w, h, pass, channel, invert, pad[3]; };
    // Each source prepared as a gray image (the channel, inverted if asked).
    gpu::TexturePtr prepared[2];
    for (int i = 0; i < 2; ++i) {
        auto& S = C.src[i];
        Document& sd = *S.view->doc;
        Prepare(*S.view);
        gpu::Texture* src = nullptr;
        if (S.layer) {
            const Layer* sl = sd.FindLayer(S.layer);
            if (!sl || !sd.GetSurface(sl->surface)) { Status("Calculs : un calque source n'existe plus"); return; }
            src = sd.GetSurface(sl->surface)->tex.get();
        } else {
            src = S.view->renderer.EvaluateComplete(sd, *ctx);
        }
        if (!src || src->Width() != W || src->Height() != H) { Status("Calculs : les sources doivent avoir la taille du document"); return; }
        prepared[i] = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA16F, "CalcSrc");
        if (!prepared[i]) { Status("Calculs : mémoire graphique insuffisante"); return; }
        gpu::DispatchDesc dd;
        dd.pipeline = k;
        dd.srv[0] = src;
        dd.uav[0] = prepared[i].get();
        SetConstants(dd, Root{W, H, 0, ShaderChannel(S.channel), S.invert ? 1u : 0u, {1, 0, 0}});  // (an opaque 0..1 channel)
        Dispatch2D(*ctx, dd, W, H);
    }
    auto result = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA16F, "Calculations");
    if (!result) { Status("Calculs : mémoire graphique insuffisante"); return; }
    // Source 1 blended over source 2.
    v.renderer.Composite(d, *ctx, result.get(), prepared[1].get(), prepared[0].get(), nullptr, (BlendMode)C.mode, C.opacity, false, nullptr);
    const std::string what = std::string("Calculs (") + BlendModeName((BlendMode)C.mode) + ")";
    if (C.result == 0) {
        v.renderer.SelectFrom(d, *ctx, v.history, result.get(), SelSource::Luminance, SelMode::Replace, {}, 0, "Calculs");
        Status(what + " : sélection");
        return;
    }
    // A gray layer: in this document, or alone in a new one.
    DocView* target = &v;
    if (C.result == 2) {
        NewDocument(W, H, d.GetPrecision(), DocMode::Layers, 3, "Calculs");  // (no background: the result is its layer)
        target = docs.back().get();
    }
    Document& td = *target->doc;
    Edit e(td, target->history, "Calculs");
    Layer* l = &td.AddPixelLayer("Calculs", (int)td.S().layers.size());  // (on top: the result seen as computed)
    td.S().activeLayer = l->id;
    const Id surf = l->surface, lid = l->id;
    td.SyncGraph();
    Prepare(*target);  // (the new surface created)
    Surface* s = td.GetSurface(surf);
    if (!s || !s->tex) { td.RemoveLayer(lid); Status("Calculs : calque impossible à créer"); return; }  // (no step)
    if (s->tex->GetFormat() == result->GetFormat()) {
        ctx->CopyTexture(s->tex.get(), result.get());
    } else {
        // (another surface format: written through the blend, normal at full opacity over nothing)
        v.renderer.Composite(td, *ctx, s->tex.get(), result.get(), result.get(), nullptr, BlendMode::Normal, 1, false, nullptr);
    }
    td.TouchSurface(surf);
    e.Commit();
    td.dirty = true;
    Status(what + (C.result == 2 ? " : nouveau document" : " : nouveau calque"));
}

}  // namespace sx
