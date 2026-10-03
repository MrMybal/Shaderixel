// Color management in the app: display transform and soft proofing (3D LUT in
// display.slang), assign / convert to profile, CMYK and Lab modes, CMYK TIFF
// export, embedded profiles of opened images, spot color layers.
#include "ui/App.h"
#include "io/ColorMgmt.h"
#include "doc/Effects.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sx {

std::string App::DocProfile(DocView* v) const { return v ? v->doc->iccProfile : std::string(); }

std::string App::CmykProfileFor(DocView* v) const {
    if (v && !v->doc->cmykProfile.empty()) return v->doc->cmykProfile;
    if (!proof.profile.empty()) {
        for (auto& p : color::Profiles()) if (p.path == proof.profile && p.cmyk) return p.path;
    }
    return color::DefaultCmykPath();
}

void App::DisplayColorLut(DocView& v, ViewParams& vp) {
    Document& d = *v.doc;
    // Chain: document RGB -> proofing space (CMYK mode or Proof Colors) -> monitor.
    std::vector<std::string> chain = {d.iccProfile};
    bool proofing = proof.on || d.colorMode == 1;
    std::string proofProfile = d.colorMode == 1 ? CmykProfileFor(&v) : (proof.profile.empty() ? CmykProfileFor(&v) : proof.profile);
    if (proofing && !proofProfile.empty()) chain.push_back(proofProfile);
    chain.push_back(proof.useMonitor ? monitorProfile : std::string());
    bool identity = chain.size() == 2 && chain[0].empty() && chain[1].empty();
    if (identity) return;
    std::string key;
    for (auto& c : chain) key += c + "|";
    key += std::to_string(proof.intent) + (proofing ? "p" : "");
    if (key != v.displayLutKey) {
        v.displayLutKey = key;
        v.displayLut.reset();
        const int n = 33;
        std::vector<float> rgba;
        if (color::BuildLut(chain, proof.intent, n, proofing, rgba)) {
            // Texture layout as the effect LUTs: x = b * n + r, y = g.
            std::vector<float4> px((size_t)n * n * n);
            for (int b = 0; b < n; ++b)
                for (int g = 0; g < n; ++g)
                    for (int r = 0; r < n; ++r) {
                        const float* s = &rgba[(((size_t)b * n + g) * n + r) * 4];
                        px[(size_t)g * n * n + (size_t)b * n + r] = {s[0], s[1], s[2], s[3]};
                    }
            v.displayLut = gpu::Dev().CreateTexture((u32)(n * n), (u32)n, gpu::Format::RGBA32F, "DisplayLUT");
            ctx->Upload(v.displayLut.get(), px.data(), (u32)(n * n * sizeof(float4)));
            v.displayLutN = n;
        } else {
            Status("Gestion des couleurs : transformation impossible (profil illisible ?)");
        }
    }
    if (v.displayLut) {
        vp.lut = v.displayLut.get();
        vp.lutSize = (u32)v.displayLutN;
        vp.gamutWarning = proofing && proof.gamutWarning;
    }
}

bool App::ApplyColorLutToDocument(DocView& v, const std::vector<std::string>& chain, int intent, const std::string& label) {
    // Every pixel surface through color_lookup with a LUT of the chain (sRGB-encoded in / out).
    const int n = 33;
    std::vector<float> rgba;
    if (!color::BuildLut(chain, intent, n, false, rgba)) { Status("Conversion impossible : profil illisible"); return false; }
    std::vector<float> rgb((size_t)n * n * n * 3);
    for (size_t i = 0; i < (size_t)n * n * n; ++i) for (int k = 0; k < 3; ++k) rgb[i * 3 + k] = rgba[i * 4 + k];
    auto def = Effects().Find("color_lookup");
    if (!def) { Status("Effet color_lookup introuvable"); return false; }
    EffectInstance fx;
    fx.effectId = "color_lookup";
    fx.params = def->DefaultParams();
    fx.luts.resize(def->params.size());
    for (size_t i = 0; i < def->params.size(); ++i)
        if (def->params[i].type == ParamType::Lut) fx.luts[i] = MakeLut(label, n, std::move(rgb));
    Document& d = *v.doc;
    std::vector<Id> surfaces;
    if (d.Mode() == DocMode::Layers) {
        for (auto& l : d.S().layers) if (l.kind == LayerKind::Pixel && l.surface) surfaces.push_back(l.surface);
    } else {
        for (auto& nd : d.S().graph.nodes) if (nd.kind == NodeKind::Surface && nd.surface) surfaces.push_back(nd.surface);
    }
    v.history.BeginGroup();
    bool ok = true;
    for (Id s : surfaces) ok &= v.renderer.ApplyEffectToSurface(d, *ctx, v.history, s, fx, label);
    v.history.EndGroup();
    if (!ok) Status("Conversion : effet en cours de compilation, réessayez");
    return ok;
}

void App::SetColorMode(DocView& v, int mode) {
    Document& d = *v.doc;
    if (mode == d.colorMode) return;
    if (mode == 1) {
        // RGB -> CMYK: colors brought into the CMYK gamut, display through the profile from now on.
        std::string cmyk = CmykProfileFor(&v);
        if (cmyk.empty()) { Status("Aucun profil CMJN installé (dossier des profils Windows ou ./profiles)"); return; }
        if (!ApplyColorLutToDocument(v, {d.iccProfile, cmyk, d.iccProfile}, proof.intent, "Mode CMJN")) return;
        d.cmykProfile = cmyk;
        Status("Mode CMJN : " + color::ProfileName(cmyk) + " (couleurs ramenées dans le gamut, affichage simulé)");
    } else if (mode == 2) {
        Status("Mode Lab : sélecteur de couleur en Lab ; l'image garde sa précision (flottant)");
    } else {
        Status("Mode RVB");
    }
    d.colorMode = mode;
    d.dirty = true;
}

void App::OpenedImageProfile(DocView& v, const std::string& path) {
    std::vector<u8> icc = color::ReadEmbeddedIcc(path);
    if (icc.empty()) return;
    std::string stored = color::StoreEmbeddedIcc(icc);
    if (stored.empty()) return;
    v.doc->iccProfile = stored;
    Status("Profil incorporé conservé : " + color::ProfileName(stored) + " (Édition > Convertir en profil pour passer en sRVB)");
}

void App::ExportCmykTiff(DocView& v, const std::string& path) {
    if (path.empty()) return;
    std::string cmyk = CmykProfileFor(&v);
    if (cmyk.empty()) { Status("Aucun profil CMJN installé"); return; }
    gpu::Texture* img = v.renderer.EvaluateComplete(*v.doc, *ctx);
    if (!img) return;
    cmykExports.push_back({path, v.renderer.ExportRGBA8(*ctx, img), v.doc->iccProfile, cmyk, proof.intent});
}

void App::UpdateColorJobs() {
    for (size_t i = 0; i < cmykExports.size();) {
        auto& e = cmykExports[i];
        if (!e.rb || !e.rb->ready) { ++i; continue; }
        const u32 W = e.rb->width, H = e.rb->height;
        std::vector<u8> rgb((size_t)W * H * 3), cmyk((size_t)W * H * 4);
        for (size_t p = 0; p < (size_t)W * H; ++p) {
            // Transparent pixels over white (paper).
            const u8* s = &e.rb->data[p * 4];
            float a = s[3] / 255.0f;
            for (int k = 0; k < 3; ++k) rgb[p * 3 + k] = (u8)std::lround(s[k] * a + 255 * (1 - a));
        }
        bool ok = color::RgbToCmyk(e.rgbProfile, e.cmykProfile, e.intent, rgb.data(), (size_t)W * H, cmyk.data()) &&
                  color::WriteCmykTiff(e.path, cmyk.data(), W, H, e.cmykProfile);
        Status(ok ? "Exporté en CMJN : " + e.path : "Échec de l'export CMJN : " + e.path);
        cmykExports.erase(cmykExports.begin() + i);
    }
}

void App::NewSpotColor(DocView& v, const std::string& name, float4 srgb, float solidity) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) return;
    Edit e(d, v.history, "Ton direct");
    Layer& l = d.AddEffectLayer(LayerKind::Fill, "solid_color");
    l.name = "Ton direct : " + name;
    l.blend = BlendMode::Multiply;
    l.opacity = std::clamp(solidity, 0.0f, 1.0f);
    if (!l.effect.params.empty()) l.effect.params[0] = {SrgbToLinear(srgb.x), SrgbToLinear(srgb.y), SrgbToLinear(srgb.z), 1};
    Id id = l.id;
    MaskFromSelection(v, id, true);
    d.SyncGraph();
    e.Commit();
}

// ------------------------------------------------------------------ menus / dialogs

void App::DrawProofMenu() {
    DocView* v = Active();
    if (!ImGui::BeginMenu("Épreuvage")) return;
    ImGui::MenuItem("Couleurs d'épreuve", "Ctrl+Alt+Y", &proof.on);
    ImGui::MenuItem("Couleurs non imprimables", "Ctrl+Alt+Maj+Y", &proof.gamutWarning);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grise les couleurs hors du gamut du profil d'épreuve (pendant l'épreuvage).");
    ImGui::Separator();
    std::string cur = proof.profile.empty() ? "CMJN du document (" + color::ProfileName(CmykProfileFor(v)) + ")" : color::ProfileName(proof.profile);
    if (ImGui::BeginMenu(("Profil : " + cur).c_str())) {
        if (ImGui::MenuItem("CMJN du document", nullptr, proof.profile.empty())) proof.profile.clear();
        ImGui::SeparatorText("CMJN");
        for (auto& p : color::Profiles()) if (p.cmyk && ImGui::MenuItem(p.name.c_str(), nullptr, proof.profile == p.path)) proof.profile = p.path;
        ImGui::SeparatorText("RVB");
        for (auto& p : color::Profiles()) if (p.rgb && ImGui::MenuItem(p.name.c_str(), nullptr, proof.profile == p.path)) proof.profile = p.path;
        ImGui::EndMenu();
    }
    static const char* intents[] = {"Perceptif", "Colorimétrie relative", "Saturation", "Colorimétrie absolue (simuler le papier)"};
    if (ImGui::BeginMenu("Mode de rendu")) {
        for (int i = 0; i < 4; ++i) if (ImGui::MenuItem(intents[i], nullptr, proof.intent == i)) proof.intent = i;
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Utiliser le profil du moniteur", nullptr, &proof.useMonitor))
        for (auto& dv : docs) dv->displayLutKey.clear();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Profil d'affichage Windows : %s", monitorProfile.empty() ? "sRVB" : color::ProfileName(monitorProfile).c_str());
    ImGui::EndMenu();
}

void App::DrawColorEditMenu() {
    DocView* v = Active();
    if (!v) return;
    if (ImGui::MenuItem("Attribuer un profil...")) { colorDialog = 1; colorDialogProfile = v->doc->iccProfile; }
    if (ImGui::MenuItem("Convertir en profil...")) { colorDialog = 2; colorDialogProfile = ""; }
}

void App::DrawColorModeMenu() {
    DocView* v = Active();
    if (!v || !ImGui::BeginMenu("Mode")) return;
    Document& d = *v->doc;
    if (ImGui::MenuItem("Couleurs RVB", nullptr, d.colorMode == 0)) SetColorMode(*v, 0);
    if (ImGui::MenuItem("Couleurs CMJN", nullptr, d.colorMode == 1)) SetColorMode(*v, 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ramène les couleurs dans le gamut du profil CMJN et affiche le document à travers lui.\nExport : Fichier > Exporter en TIFF CMJN.");
    if (ImGui::MenuItem("Couleurs Lab", nullptr, d.colorMode == 2)) SetColorMode(*v, 2);
    ImGui::Separator();
    if (ImGui::BeginMenu(("Profil CMJN : " + color::ProfileName(CmykProfileFor(v))).c_str())) {
        for (auto& p : color::Profiles())
            if (p.cmyk && ImGui::MenuItem(p.name.c_str(), nullptr, CmykProfileFor(v) == p.path)) { d.cmykProfile = p.path; d.dirty = true; }
        ImGui::EndMenu();
    }
    ImGui::TextDisabled("Profil RVB : %s", color::ProfileName(d.iccProfile).c_str());
    ImGui::EndMenu();
}

void App::DrawColorDialogs() {
    DocView* v = Active();
    if (colorDialog && !ImGui::IsPopupOpen("##colordlg")) ImGui::OpenPopup("##colordlg");
    if (spotDialog && !ImGui::IsPopupOpen("##spotdlg")) ImGui::OpenPopup("##spotdlg");
    if (ImGui::BeginPopupModal("##colordlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        if (!v) { colorDialog = 0; ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        Document& d = *v->doc;
        ImGui::TextUnformatted(colorDialog == 1 ? "Attribuer un profil" : "Convertir en profil");
        ImGui::Separator();
        ImGui::TextDisabled("Profil actuel : %s", color::ProfileName(d.iccProfile).c_str());
        ImGui::SetNextItemWidth(320 * dpi);
        if (ImGui::BeginCombo(colorDialog == 1 ? "Profil" : "Profil de destination", color::ProfileName(colorDialogProfile).c_str())) {
            if (ImGui::Selectable("sRGB IEC61966-2.1", colorDialogProfile.empty())) colorDialogProfile.clear();
            for (auto& p : color::Profiles())
                if (p.rgb && ImGui::Selectable(p.name.c_str(), colorDialogProfile == p.path)) colorDialogProfile = p.path;
            ImGui::EndCombo();
        }
        if (colorDialog == 2) {
            static const char* intents = "Perceptif\0Colorimétrie relative\0Saturation\0Colorimétrie absolue\0";
            ImGui::SetNextItemWidth(320 * dpi);
            ImGui::Combo("Mode de rendu", &proof.intent, intents);
            ImGui::TextDisabled("Les valeurs des pixels changent, l'apparence est conservée.\nCalques de pixels uniquement (remplissages et réglages inchangés).");
        } else {
            ImGui::TextDisabled("Les valeurs des pixels sont conservées, l'apparence change.");
        }
        if (ImGui::Button("OK", ImVec2(100 * dpi, 0))) {
            if (colorDialog == 2) {
                if (colorDialogProfile != d.iccProfile && ApplyColorLutToDocument(*v, {d.iccProfile, colorDialogProfile}, proof.intent, "Convertir en profil"))
                    d.iccProfile = colorDialogProfile;
            } else {
                d.iccProfile = colorDialogProfile;
            }
            d.dirty = true;
            colorDialog = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            colorDialog = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("##spotdlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextUnformatted("Nouveau ton direct");
        ImGui::Separator();
        ImGui::SetNextItemWidth(220 * dpi);
        ImGui::InputText("Nom de l'encre", &spotName);
        ImGui::ColorEdit3("Couleur", &spotColor.x);
        ImGui::SetNextItemWidth(220 * dpi);
        ImGui::SliderFloat("Solidité", &spotSolidity, 0, 1, "%.2f");
        ImGui::TextDisabled("Calque de couleur unie en Produit, masqué par la sélection.\nPeignez son masque pour placer l'encre.");
        if (ImGui::Button("OK", ImVec2(100 * dpi, 0))) {
            if (v) NewSpotColor(*v, spotName, spotColor, spotSolidity);
            spotDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            spotDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace sx
