// Selection tools: Color Range (sampled colors with an eyedropper, or preset
// ranges) and Select and Mask (edge refinement with a guided filter, smoothing,
// feather, contrast, edge shift, live preview on the canvas, output to the
// selection, a layer mask or a new masked layer).
#include "ui/App.h"
#include "ui/Theme.h"

#include <cstring>

namespace sx {

namespace {
// Pixel read back from the image (premultiplied linear, half or float) -> straight linear color.
bool DecodePixel(const gpu::ReadbackPtr& rb, float4& out) {
    if (!rb || !rb->ready) return false;
    auto& d = rb->data;
    float4 c;
    if (rb->format == gpu::Format::RGBA16F && d.size() >= 8) {
        const u16* h = (const u16*)d.data();
        c = {HalfToFloat(h[0]), HalfToFloat(h[1]), HalfToFloat(h[2]), HalfToFloat(h[3])};
    } else if (rb->format == gpu::Format::RGBA32F && d.size() >= 16) {
        memcpy(&c, d.data(), 16);
    } else {
        return false;
    }
    if (c.w <= 1e-5f) return false;
    out = {c.x / c.w, c.y / c.w, c.z / c.w, 1};
    return true;
}

u64 HashBytes(const void* p, size_t n, u64 h = 1469598103934665603ull) {
    const unsigned char* c = (const unsigned char*)p;
    for (size_t i = 0; i < n; ++i) h = (h ^ c[i]) * 1099511628211ull;
    return h;
}
}  // namespace

bool App::SelectToolsCanvasInput(DocView& v, float2 dp, bool click) {
    // Color Range eyedropper: click = sample, Shift = add, Alt = remove the closest.
    if (!colorRange.open || !click || !v.lastImage) return false;
    if (dp.x < 0 || dp.y < 0 || dp.x >= v.doc->Width() || dp.y >= v.doc->Height()) return false;
    ImGuiIO& io = ImGui::GetIO();
    colorRange.pickOp = io.KeyShift ? 1 : io.KeyAlt ? 2 : 0;
    colorRange.pick = v.renderer.ReadPixel(*ctx, v.lastImage, (int)dp.x, (int)dp.y);
    colorRange.preset = 0;
    return true;
}

void App::DrawSelectToolsOverlay(DocView& v, ImVec2 d0, ImVec2 d1, ImDrawList* dl) {
    if (selectMask.open && selectMask.preview) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(selectMask.preview), d0, d1);
}

void App::DrawColorRangeWindow() {
    auto& C = colorRange;
    if (!C.open) return;
    DocView* v = Active();
    if (!v) { C.open = false; return; }
    Document& d = *v->doc;
    // Picked color.
    float4 picked;
    if (C.pick && C.pick->ready) {
        if (DecodePixel(C.pick, picked)) {
            if (C.pickOp == 2 && !C.samples.empty()) {
                size_t best = 0;
                float bd = 1e9f;
                for (size_t i = 0; i < C.samples.size(); ++i) {
                    float4 q = C.samples[i];
                    float dd = std::abs(q.x - picked.x) + std::abs(q.y - picked.y) + std::abs(q.z - picked.z);
                    if (dd < bd) { bd = dd; best = i; }
                }
                C.samples.erase(C.samples.begin() + best);
            } else {
                if (C.pickOp == 0) C.samples.clear();
                if (C.samples.size() < 8) C.samples.push_back(picked);
            }
        }
        C.pick.reset();
    }
    ImGui::SetNextWindowSize(ImVec2(360 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Plage de couleurs", &C.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    static const char* presets = "Couleurs échantillonnées\0Rouges\0Jaunes\0Verts\0Cyans\0Bleus\0Magentas\0Tons clairs\0Tons moyens\0Tons foncés\0Tons chair\0";
    ImGui::Combo("Sélection", &C.preset, presets);
    if (C.preset == 0) {
        ImGui::TextDisabled("Cliquez dans l'image : Maj ajoute, Alt retire.");
        float s = ImGui::GetFrameHeight();
        for (size_t i = 0; i < C.samples.size(); ++i) {
            float4 q = C.samples[i];
            ImVec4 col(LinearToSrgb(q.x), LinearToSrgb(q.y), LinearToSrgb(q.z), 1);
            ImGui::PushID((int)i);
            if (ImGui::ColorButton("##s", col, ImGuiColorEditFlags_NoTooltip, ImVec2(s, s))) {
                C.samples.erase(C.samples.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
            ImGui::SameLine();
        }
        if (ImGui::SmallButton("+ premier plan") && C.samples.size() < 8)
            C.samples.push_back({SrgbToLinear(fg.x), SrgbToLinear(fg.y), SrgbToLinear(fg.z), 1});
        ImGui::SliderFloat("Tolérance", &C.fuzz, 1, 150, "%.0f");
        ui::HelpTooltip("Distance de couleur (Lab) jusqu'où les pixels sont sélectionnés, avec une transition douce.");
    }
    ImGui::Checkbox("Inverser", &C.invert);
    ImGui::Combo("Mode", &C.mode, "Nouvelle sélection\0Ajouter\0Soustraire\0Intersection\0");
    // Preview: recomputed when something changes.
    gpu::Texture* image = v->lastImage;
    u64 h = HashBytes(&C.preset, sizeof(C.preset));
    h = HashBytes(&C.fuzz, sizeof(C.fuzz), h);
    h = HashBytes(&C.invert, sizeof(C.invert), h);
    if (!C.samples.empty()) h = HashBytes(C.samples.data(), C.samples.size() * sizeof(float4), h);
    h ^= (u64)(uintptr_t)image;
    if (image && (h != C.hash || !C.mask)) {
        C.mask = v->renderer.ColorRangeMask(*ctx, image, C.samples, C.preset, C.fuzz, C.invert);
        C.hash = h;
        C.version++;
    }
    if (C.mask) {
        float w = 320 * dpi, hh = w * d.Height() / std::max(1u, d.Width());
        gpu::Texture* t = v->renderer.Thumbnail(*ctx, (1ull << 57) | 1, C.mask, C.version, (u32)w, (u32)hh);
        ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(t), ImVec2(w, hh));
    }
    bool ok = ImGui::Button("OK", ImVec2(100 * dpi, 0));
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0))) C.open = false;
    if (ok && C.mask) {
        static const SelMode modes[] = {SelMode::Replace, SelMode::Add, SelMode::Subtract, SelMode::Intersect};
        v->renderer.SelectFrom(d, *ctx, v->history, C.mask, SelSource::Mask, modes[std::clamp(C.mode, 0, 3)], {}, 0, "Plage de couleurs");
        C.open = false;
    }
    ImGui::End();
}

void App::DrawSelectMaskWindow() {
    auto& M = selectMask;
    if (!M.open) { M.preview = nullptr; return; }
    DocView* v = Active();
    if (!v) { M.open = false; return; }
    Document& d = *v->doc;
    Surface* sel = d.Selection();
    if (!sel) {
        Status("Sélectionner et masquer : faites d'abord une sélection (même approximative)");
        M.open = false;
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(380 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Sélectionner et masquer", &M.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        M.preview = nullptr;
        return;
    }
    ImGui::Combo("Affichage", &M.view, "Incrustation rouge\0Noir et blanc\0Sur noir\0Sur blanc\0Calque seul\0");
    ImGui::SeparatorText("Détection des contours");
    ImGui::SliderFloat("Rayon", &M.params.radius, 0, 120, "%.0f px");
    ui::HelpTooltip("Zone autour du contour où l'image guide le masque (cheveux, poils, feuillage).");
    ImGui::Checkbox("Rayon intelligent", &M.params.smartRadius);
    ui::HelpTooltip("N'affine que près du contour de la sélection ; le reste est conservé.");
    float detail = 1.0f - std::sqrt(std::clamp(M.params.eps / 0.05f, 0.0f, 1.0f));
    if (ImGui::SliderFloat("Précision des détails", &detail, 0, 1, "%.2f")) M.params.eps = std::max(1e-5f, (1.0f - detail) * (1.0f - detail) * 0.05f);
    ui::HelpTooltip("Plus élevé : suit les petits détails du contraste (mèches fines).");
    ImGui::SeparatorText("Contour global");
    ImGui::SliderFloat("Lissage", &M.params.smooth, 0, 50, "%.0f px");
    ImGui::SliderFloat("Contour progressif", &M.params.feather, 0, 60, "%.1f px");
    float contrast = M.params.contrast * 100.0f, shift = M.params.shift * 100.0f;
    if (ImGui::SliderFloat("Contraste", &contrast, 0, 100, "%.0f %%")) M.params.contrast = contrast / 100.0f;
    if (ImGui::SliderFloat("Décaler le contour", &shift, -100, 100, "%.0f %%")) M.params.shift = shift / 100.0f;
    ImGui::SeparatorText("Sortie");
    ImGui::Combo("Vers", &M.output, "Sélection\0Masque de calque\0Nouveau calque avec masque\0");
    // Refined mask + preview, recomputed when a setting changes.
    gpu::Texture* image = v->lastImage;
    u64 h = HashBytes(&M.params, sizeof(M.params));
    h = HashBytes(&M.view, sizeof(M.view), h);
    h ^= (u64)(uintptr_t)image ^ ((u64)(uintptr_t)sel->tex.get() << 1) ^ sel->version * 31;
    if (image && h != M.hash) {
        M.mask = v->renderer.RefineMask(*ctx, image, sel->tex.get(), M.params);
        M.preview = M.mask ? v->renderer.RefinePreview(*ctx, image, M.mask, M.view) : nullptr;
        M.hash = h;
    }
    bool ok = ImGui::Button("OK", ImVec2(100 * dpi, 0));
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0))) M.open = false;
    if (ok && M.mask) {
        Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
        if (M.output == 0 || !l) {
            v->renderer.SelectFrom(d, *ctx, v->history, M.mask, SelSource::Mask, SelMode::Replace, {}, 0, "Sélectionner et masquer");
        } else {
            v->history.BeginGroup();
            if (M.output == 2) {
                Edit e(d, v->history, "Nouveau calque avec masque");
                Layer& dup = d.DuplicateLayer(l->id);
                dup.name = l->name + " (masqué)";
                d.S().activeLayer = dup.id;
                l = &dup;
                e.Commit();
                v->renderer.Prepare(d, *ctx);
            }
            if (!l->mask) {
                Edit e(d, v->history, "Ajouter un masque");
                d.AddMask(l->id);
                e.Commit();
                v->renderer.Prepare(d, *ctx);
            }
            v->renderer.WriteMaskSurface(d, *ctx, v->history, l->mask, M.mask, "Sélectionner et masquer");
            v->renderer.Deselect(d, v->history);
            v->history.EndGroup();
        }
        M.open = false;
    }
    ImGui::End();
    if (!M.open) M.preview = nullptr;
}

}  // namespace sx
