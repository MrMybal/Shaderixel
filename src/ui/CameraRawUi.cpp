// Camera Raw workspace: development of the active layer (usually the photo of a camera RAW opened with
// LibRaw) with the "camera_raw" filter (white balance, tones, presence, tone curve, color mixer, color
// grading, detail, noise reduction, effects) and the "lens_profile" filter (lens corrections from the
// lensfun database). Big preview, histogram, before / after, OK (one history step) / Annuler.
// The lens picker is shared with the properties of the "Correction d'objectif" effect, which also
// simulates a lens on any layer.

#include "ui/App.h"
#include "ui/Theme.h"

#include <shellapi.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace sx {

namespace {

EffectInstance* FindFilter(Layer* l, const char* id) {
    if (!l) return nullptr;
    for (auto& f : l->filters)
        if (f.effectId == id) return &f;
    return nullptr;
}

void SetParam(EffectInstance& fx, const char* id, float v) {
    EffectDefPtr def = fx.Def();
    if (!def) return;
    if (fx.params.size() < def->params.size())
        for (size_t i = fx.params.size(); i < def->params.size(); ++i) fx.params.push_back(def->params[i].def);
    for (size_t i = 0; i < def->params.size(); ++i)
        if (def->params[i].id == id) fx.params[i].x = v;
}

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::string LensfunUserDir() {
    const char* la = getenv("LOCALAPPDATA");
    return la ? std::string(la) + "\\Shaderixel\\lensfun" : std::string();
}

}  // namespace

// ------------------------------------------------------------------ lensfun

void App::EnsureLensfun(bool reload) {
    static bool tried = false;
    if (tried && !reload) return;
    tried = true;
    std::string dir = FindLensfunDir(lensfunDir);
    Lensfun() = LensfunDb();
    if (dir.empty()) {
        lensPick.status = "base lensfun introuvable";
        return;
    }
    std::string err;
    if (!Lensfun().Load(dir, &err)) lensPick.status = "lecture impossible : " + err;
    else lensPick.status.clear();
    lensPick.lens = -1;
    lensPick.autoFor = nullptr;
    Log("lensfun: %zu lenses, %zu cameras from %s", Lensfun().lenses.size(), Lensfun().cameras.size(), dir.c_str());
}

void App::ApplyLensPick(EffectInstance& fx) {
    auto& P = lensPick;
    LensfunDb& db = Lensfun();
    if (P.lens < 0 || P.lens >= (int)db.lenses.size()) return;
    const LensfunLens* L = &db.lenses[P.lens];
    LensProfile pr = LensfunDb::Profile(*L, P.focal, P.aperture, P.distance, P.crop);
    SetParam(fx, "dist_model", P.applyDist ? (float)pr.distModel : 0.0f);
    SetParam(fx, "d_a", pr.a);
    SetParam(fx, "d_b", pr.b);
    SetParam(fx, "d_c", pr.c);
    SetParam(fx, "tca_model", P.applyTca ? (float)pr.tcaModel : 0.0f);
    SetParam(fx, "tca_vr", pr.vr);
    SetParam(fx, "tca_vb", pr.vb);
    SetParam(fx, "tca_cr", pr.cr);
    SetParam(fx, "tca_cb", pr.cb);
    SetParam(fx, "tca_br", pr.br);
    SetParam(fx, "tca_bb", pr.bb);
    SetParam(fx, "vig", P.applyVig && pr.vignetting ? 1.0f : 0.0f);
    SetParam(fx, "v_k1", pr.k1);
    SetParam(fx, "v_k2", pr.k2);
    SetParam(fx, "v_k3", pr.k3);
    SetParam(fx, "crop_ratio", pr.cropRatio);
    char buf[96];
    snprintf(buf, sizeof(buf), ", %.0f mm f/%.1f", P.focal, P.aperture);
    fx.note = L->Name() + buf;
    std::string what;
    if (pr.distModel) what += "distorsion";
    if (pr.tcaModel) what += std::string(what.empty() ? "" : ", ") + "aberrations chromatiques";
    if (pr.vignetting) what += std::string(what.empty() ? "" : ", ") + "vignetage";
    P.status = what.empty() ? "aucune calibration dans ce profil" : "calibré : " + what;
}

bool App::LensfunPickerUI(EffectInstance& fx, const Document* doc) {
    EnsureLensfun();
    auto& P = lensPick;
    LensfunDb& db = Lensfun();
    bool changed = false, apply = false;
    ImGui::PushID("lensfun");
    if (!db.Loaded()) {
        ImGui::TextWrapped("Base de profils lensfun introuvable. Téléchargez les fichiers XML du dossier data/db du projet "
                           "lensfun (licence CC BY-SA) et placez-les dans le dossier ci-dessous, puis « Recharger ».");
        ImGui::TextDisabled("%s", LensfunUserDir().c_str());
        if (ImGui::Button("Page de téléchargement"))
            ShellExecuteW(nullptr, L"open", L"https://github.com/lensfun/lensfun/tree/master/data/db", nullptr, nullptr, SW_SHOWNORMAL);
        ImGui::SameLine();
        if (ImGui::Button("Ouvrir le dossier")) {
            std::error_code ec;
            std::filesystem::create_directories(Utf8Path(LensfunUserDir()), ec);
            ShellExecuteW(nullptr, L"open", Utf8Path(LensfunUserDir()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button("Recharger")) EnsureLensfun(true);
        if (ImGui::Button("Choisir un autre dossier...")) {
            std::string f = platform::OpenFileDialog(hwnd, L"Un fichier de la base lensfun (*.xml)\0*.xml\0");
            if (!f.empty()) {
                lensfunDir = Narrow(Utf8Path(f).parent_path().wstring());
                SaveAiSettings();
                EnsureLensfun(true);
            }
        }
        ImGui::PopID();
        return false;
    }
    ImGui::TextDisabled("%zu objectifs, %zu boîtiers", db.lenses.size(), db.cameras.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Dossier...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Un fichier de la base lensfun (*.xml)\0*.xml\0");
        if (!f.empty()) {
            lensfunDir = Narrow(Utf8Path(f).parent_path().wstring());
            SaveAiSettings();
            EnsureLensfun(true);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", db.Dir().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Recharger")) EnsureLensfun(true);

    // Automatic choice from the photo's metadata (once per document).
    if (doc && doc != P.autoFor) {
        P.autoFor = doc;
        const Document::Shot& s = doc->shot;
        if (!s.lens.empty() || !s.model.empty()) {
            const LensfunLens* l = !s.lens.empty() ? db.FindLens(s.lens, "") : nullptr;
            if (!l && !s.model.empty()) l = db.FindLens(s.model, s.make);  // compact: lens named after the camera
            if (l) {
                P.lens = (int)(l - db.lenses.data());
                P.focal = s.focal > 0 ? s.focal : l->minFocal;
                P.aperture = s.aperture > 0 ? s.aperture : 8;
                const LensfunCamera* cam = db.FindCamera(s.make, s.model);
                P.crop = cam ? cam->crop : s.crop;
                apply = true;
            }
        }
    }
    if (doc && !doc->shot.summary.empty()) ImGui::TextWrapped("Photo : %s", doc->shot.summary.c_str());

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##lfilter", "Rechercher un objectif (marque, modèle, focale)...", P.filter, sizeof(P.filter));
    std::vector<std::string> tokens;
    {
        std::string f = Lower(P.filter), t;
        for (char c : f + " ") {
            if (c == ' ') { if (!t.empty()) tokens.push_back(t); t.clear(); }
            else t += c;
        }
    }
    if (ImGui::BeginListBox("##lenses", ImVec2(-FLT_MIN, 150 * dpi))) {
        int shown = 0;
        for (int i = 0; i < (int)db.lenses.size() && shown < 400; ++i) {
            const LensfunLens& L = db.lenses[i];
            std::string name = L.Name();
            if (!tokens.empty()) {
                std::string ln = Lower(name + " " + L.mounts);
                bool ok = true;
                for (auto& t : tokens) if (ln.find(t) == std::string::npos) { ok = false; break; }
                if (!ok) continue;
            }
            ++shown;
            ImGui::PushID(i);
            char label[300];
            snprintf(label, sizeof(label), "%s  (crop %.2g)", name.c_str(), L.crop);
            if (ImGui::Selectable(label, P.lens == i)) {
                P.lens = i;
                if (P.focal < L.minFocal || P.focal > std::max(L.maxFocal, L.minFocal) || P.focal <= 0) P.focal = L.minFocal;
                if (P.aperture <= 0) P.aperture = 8;
                apply = true;
            }
            if (P.lens == i && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
            ImGui::PopID();
        }
        if (shown == 0) ImGui::TextDisabled("Aucun objectif ne correspond.");
        ImGui::EndListBox();
    }
    const LensfunLens* L = P.lens >= 0 && P.lens < (int)db.lenses.size() ? &db.lenses[P.lens] : nullptr;
    if (L) {
        ImGui::Text("%s", L->Name().c_str());
        if (!L->mounts.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", L->mounts.c_str());
        }
        float fmin = L->minFocal > 0 ? L->minFocal : 1, fmax = std::max(L->maxFocal, fmin);
        if (fmax > fmin) {
            if (ImGui::SliderFloat("Focale (mm)", &P.focal, fmin, fmax, "%.0f")) apply = true;
        } else {
            P.focal = fmin;
            ImGui::TextDisabled("Focale fixe : %.0f mm", fmin);
        }
        if (ImGui::SliderFloat("Ouverture (f/)", &P.aperture, 1.0f, 32.0f, "%.1f", ImGuiSliderFlags_Logarithmic)) apply = true;
        if (ImGui::SliderFloat("Distance (m)", &P.distance, 0.25f, 1000.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) apply = true;
        float crop = P.crop > 0 ? P.crop : L->crop;
        if (ImGui::SliderFloat("Crop du boîtier", &crop, 0.5f, 8.0f, "%.2f")) { P.crop = crop; apply = true; }
        ui::HelpTooltip("Facteur de recadrage du capteur qui a pris la photo (1 = plein format, 1.5 / 1.6 = APS-C). "
                        "Il fixe l'échelle du profil, calibré au crop de l'objectif.");
        if (ImGui::Checkbox("Distorsion", &P.applyDist)) apply = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Aberrations chromatiques", &P.applyTca)) apply = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Vignetage", &P.applyVig)) apply = true;
        if (ImGui::Button("Appliquer le profil")) apply = true;
    }
    if (apply && L) {
        ApplyLensPick(fx);
        changed = true;
        // One history step per discrete choice; sliders merge while dragged.
        if (DocView* v = Active(); v && frameBeforeValid && !ImGui::IsAnyItemActive())
            v->history.PushState("Profil d'objectif", frameBefore, v->doc->S());
    }
    if (!fx.note.empty()) ImGui::TextDisabled("Profil : %s", fx.note.c_str());
    if (!P.status.empty()) ImGui::TextDisabled("%s", P.status.c_str());
    ImGui::PopID();
    return changed;
}

// ------------------------------------------------------------------ Camera Raw

void App::OpenCameraRaw(DocView& v) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) {
        Status("Camera Raw : uniquement pour les documents en calques");
        return;
    }
    Layer* l = d.ActiveLayer();
    if (!l) {
        Status("Camera Raw : aucun calque actif");
        return;
    }
    auto& C = camRaw;
    C.before = d.S();
    C.histStart = v.history.Cursor();
    if (!FindFilter(l, "camera_raw")) {
        EffectInstance fx;
        fx.effectId = "camera_raw";
        if (auto def = Effects().Find("camera_raw")) fx.params = def->DefaultParams();
        l->filters.push_back(fx);
    }
    FindFilter(l, "camera_raw")->enabled = true;
    d.SyncGraph();
    C.open = true;
    C.view = &v;
    C.layer = l->id;
    C.compare = false;
    C.zoom = 0;
    C.pan = {0.5f, 0.5f};
    C.firstFrame = true;
    C.changeFrame = gpu::Dev().FrameNumber();
}

void App::DrawCameraRaw() {
    auto& C = camRaw;
    if (!C.open) return;
    DocView* v = nullptr;
    for (auto& dv : docs)
        if (dv.get() == C.view) v = dv.get();
    Layer* l = v ? v->doc->FindLayer(C.layer) : nullptr;
    if (!v || !l) {
        C.open = false;
        C.tex.reset();
        return;
    }
    Document& d = *v->doc;
    auto finish = [&](bool ok) {
        // Restore the comparison and merge the session's edits into one history step (or revert them).
        if (C.compare) {
            if (EffectInstance* f = FindFilter(d.FindLayer(C.layer), "camera_raw")) f->enabled = C.savedCr;
            if (EffectInstance* f = FindFilter(d.FindLayer(C.layer), "lens_profile")) f->enabled = C.savedLens;
            C.compare = false;
        }
        v->history.TruncateTo(C.histStart);
        if (ok) {
            if (C.before) {
                v->history.PushState("Camera Raw", *C.before, d.S());
                d.dirty = true;
            }
        } else if (C.before) {
            d.S() = *C.before;
            d.InvalidateSurfaces();
        }
        d.SyncGraph();
        C.open = false;
        C.tex.reset();
    };

    ImGuiViewport* vp0 = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp0->WorkPos.x + vp0->WorkSize.x * 0.5f, vp0->WorkPos.y + vp0->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(vp0->WorkSize.x * 0.92f, vp0->WorkSize.y * 0.92f), ImGuiCond_Appearing);
    if (C.firstFrame) ImGui::SetNextWindowFocus();
    bool keep = true;
    std::string title = "Camera Raw - " + d.Name() + "###cameraraw";
    if (!ImGui::Begin(title.c_str(), &keep, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!keep) finish(false);
        return;
    }
    C.firstFrame = false;
    const float panelW = 400 * dpi;
    bool changed = false;
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // ---- preview
    ImGui::BeginChild("##crpreview", ImVec2(-panelW - ImGui::GetStyle().ItemSpacing.x, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        if (ImGui::Checkbox("Avant (P)", &C.compare) || (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_P, false) && (C.compare = !C.compare, true))) {
            EffectInstance* cr = FindFilter(l, "camera_raw");
            EffectInstance* lens = FindFilter(l, "lens_profile");
            if (C.compare) {
                C.savedCr = cr ? cr->enabled : true;
                C.savedLens = lens ? lens->enabled : true;
                if (cr) cr->enabled = false;
                if (lens) lens->enabled = false;
            } else {
                if (cr) cr->enabled = C.savedCr;
                if (lens) lens->enabled = C.savedLens;
            }
            d.SyncGraph();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        static const char* zooms[] = {"Adapter", "25 %", "50 %", "100 %", "200 %"};
        static const float zv[] = {0, 0.25f, 0.5f, 1, 2};
        ImGui::SetNextItemWidth(110 * dpi);
        ImGui::Combo("Zoom", &C.zoom, zooms, IM_ARRAYSIZE(zooms));
        ImGui::SameLine();
        ImGui::TextDisabled("%u x %u  -  double-clic : 100 %% / adapter, glisser : déplacer", d.Width(), d.Height());
        if (!d.shot.summary.empty()) ImGui::TextDisabled("%s", d.shot.summary.c_str());

        ImVec2 avail = ImGui::GetContentRegionAvail();
        u32 vw = (u32)std::max(16.0f, avail.x), vh = (u32)std::max(16.0f, avail.y);
        if (!C.tex || C.tex->Width() != vw || C.tex->Height() != vh) C.tex = gpu::Dev().CreateTexture(vw, vh, gpu::Format::RGBA8, "CameraRawView");
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##crimage", ImVec2((float)vw, (float)vh));
        const float docW = (float)d.Width(), docH = (float)d.Height();
        float inv = C.zoom == 0 ? std::max(docW / vw, docH / vh) : 1.0f / zv[std::clamp(C.zoom, 0, 4)];
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (C.zoom == 0) {
                // Zoom on the clicked point.
                float fitInv = inv;
                float ox = (docW - vw * fitInv) * 0.5f, oy = (docH - vh * fitInv) * 0.5f;
                ImVec2 m = ImGui::GetIO().MousePos;
                C.pan = {std::clamp((ox + (m.x - p0.x) * fitInv) / docW, 0.0f, 1.0f), std::clamp((oy + (m.y - p0.y) * fitInv) / docH, 0.0f, 1.0f)};
                C.zoom = 3;
            } else {
                C.zoom = 0;
            }
            inv = C.zoom == 0 ? std::max(docW / vw, docH / vh) : 1.0f / zv[C.zoom];
        }
        if (C.zoom != 0 && ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0)) {
            ImVec2 dd = ImGui::GetIO().MouseDelta;
            C.pan.x = std::clamp(C.pan.x - dd.x * inv / docW, 0.0f, 1.0f);
            C.pan.y = std::clamp(C.pan.y - dd.y * inv / docH, 0.0f, 1.0f);
        }
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0) {
            C.zoom = std::clamp(C.zoom + (ImGui::GetIO().MouseWheel > 0 ? 1 : -1), 0, 4);
            inv = C.zoom == 0 ? std::max(docW / vw, docH / vh) : 1.0f / zv[C.zoom];
        }
        ViewParams vp;
        vp.invZoom = inv;
        vp.viewW = vw;
        vp.viewH = vh;
        vp.origin = C.zoom == 0 ? float2{(docW - vw * inv) * 0.5f, (docH - vh * inv) * 0.5f}
                                : float2{C.pan.x * docW - vw * inv * 0.5f, C.pan.y * docH - vh * inv * 0.5f};
        vp.nearest = inv < 0.5f;
        vp.checker = 8;
        vp.outside = {0.12f, 0.12f, 0.13f, 1};
        DisplayColorLut(*v, vp);
        gpu::Texture* image = v->lastImage ? v->lastImage : v->renderer.Evaluate(d, *ctx);
        if (image) v->renderer.Display(*ctx, v->renderer.DisplaySource(*ctx, image, inv), d.Width(), d.Height(), vp, C.tex.get());
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)gpu::Dev().UiTextureId(C.tex.get()), p0, ImVec2(p0.x + vw, p0.y + vh));
        if (C.compare) ImGui::GetWindowDrawList()->AddText(ImVec2(p0.x + 10 * dpi, p0.y + 8 * dpi), IM_COL32(255, 220, 120, 255), "Avant");
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- settings
    ImGui::BeginChild("##crpanel", ImVec2(panelW, 0), false);
    {
        ImGui::BeginChild("##crscroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.4f), false);
        // Histogram (RGB overlay).
        if (histo.valid) {
            float w = ImGui::GetContentRegionAvail().x, h = 90 * dpi;
            ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
            ImGui::Dummy(ImVec2(w, h));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(a, b, IM_COL32(18, 18, 22, 255), 3);
            dl->PushClipRect(a, b, true);
            const ImU32 cols[3] = {IM_COL32(230, 60, 60, 110), IM_COL32(60, 210, 80, 110), IM_COL32(70, 110, 240, 110)};
            for (int c = 0; c < 3; ++c)
                for (int i = 0; i < 256; ++i) {
                    float x0 = a.x + w * i / 256.0f, x1 = a.x + w * (i + 1) / 256.0f;
                    dl->AddRectFilled(ImVec2(x0, b.y - histo.norm[c + 1][i] * (h - 2)), ImVec2(std::max(x1, x0 + 1), b.y), cols[c]);
                }
            dl->PopClipRect();
        }
        static const bool expandAll = getenv("SX_CREXPAND") != nullptr;  // demo captures: every section open
        auto header = [&](const char* name, ImGuiTreeNodeFlags fl = 0) {
            if (expandAll) ImGui::SetNextItemOpen(strcmp(name, "Base") != 0 && strcmp(name, "Optique") != 0, ImGuiCond_Appearing);
            return ImGui::CollapsingHeader(name, fl);
        };
        EffectInstance* cr = FindFilter(l, "camera_raw");
        if (cr) {
            ImGui::PushItemWidth(-150 * dpi);
            if (header("Base", ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= EffectParamsUI(*cr, "crbase", "Base");
                ImGui::Spacing();
                changed |= EffectParamsUI(*cr, "crpres", "Présence");
            }
            if (header("Courbe des tonalités")) changed |= EffectParamsUI(*cr, "crcurve", "Courbe des tonalités");
            if (header("Détail")) {
                changed |= EffectParamsUI(*cr, "crdet", "Détail");
                ImGui::SeparatorText("Réduction du bruit");
                changed |= EffectParamsUI(*cr, "crnr", "Réduction du bruit");
            }
            if (header("Mélangeur de couleurs")) {
                if (ImGui::BeginTabBar("##mix")) {
                    if (ImGui::BeginTabItem("Teinte")) { changed |= EffectParamsUI(*cr, "crmh", "Mélangeur : Teinte"); ImGui::EndTabItem(); }
                    if (ImGui::BeginTabItem("Saturation")) { changed |= EffectParamsUI(*cr, "crms", "Mélangeur : Saturation"); ImGui::EndTabItem(); }
                    if (ImGui::BeginTabItem("Luminance")) { changed |= EffectParamsUI(*cr, "crml", "Mélangeur : Luminance"); ImGui::EndTabItem(); }
                    ImGui::EndTabBar();
                }
            }
            if (header("Étalonnage des couleurs")) changed |= EffectParamsUI(*cr, "crcg", "Étalonnage des couleurs");
            ImGui::PopItemWidth();
        }
        if (header("Optique", ImGuiTreeNodeFlags_DefaultOpen)) {
            EffectInstance* lens = FindFilter(l, "lens_profile");
            bool on = lens && lens->enabled && !C.compare;
            if (ImGui::Checkbox("Activer les corrections de profil (lensfun)", &on)) {
                DocState before = d.S();
                if (on && !lens) {
                    EffectInstance fx;
                    fx.effectId = "lens_profile";
                    if (auto def = Effects().Find("lens_profile")) fx.params = def->DefaultParams();
                    l->filters.insert(l->filters.begin(), fx);  // geometry and vignetting first, on the linear photo
                    lensPick.autoFor = nullptr;                   // preselect from the shot
                } else if (lens) {
                    lens->enabled = on;
                }
                d.SyncGraph();
                v->history.PushState("Corrections de profil", before, d.S());
            }
            lens = FindFilter(l, "lens_profile");
            if (lens && lens->enabled) {
                ImGui::PushItemWidth(-150 * dpi);
                changed |= LensfunPickerUI(*lens, &d);
                if (ImGui::TreeNode("Réglages du profil")) {
                    changed |= EffectParamsUI(*lens, "lensmode", "");
                    for (const char* g : {"Distorsion", "Aberrations chromatiques", "Vignetage", "Cadrage"}) {
                        ImGui::SeparatorText(g);
                        changed |= EffectParamsUI(*lens, g, g);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopItemWidth();
            }
            cr = FindFilter(l, "camera_raw");  // the insertion above may have moved it
        }
        if (cr && header("Effets")) {
            ImGui::PushItemWidth(-150 * dpi);
            changed |= EffectParamsUI(*cr, "creff", "Effets");
            ImGui::PopItemWidth();
        }
        ImGui::EndChild();
        ImGui::Separator();
        if (ImGui::Button("Réinitialiser")) {
            if (EffectInstance* f = FindFilter(l, "camera_raw")) {
                DocState before = d.S();
                if (auto def = Effects().Find("camera_raw")) f->params = def->DefaultParams();
                f->curves.clear();
                d.SyncGraph();
                v->history.PushState("Réinitialiser Camera Raw", before, d.S());
            }
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 210 * dpi);
        bool cancel = ImGui::Button("Annuler", ImVec2(100 * dpi, 0));
        ImGui::SameLine();
        bool ok = ImGui::Button("OK", ImVec2(100 * dpi, 0));
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) cancel = true;
        if (changed) {
            d.SyncGraph();
            C.changeFrame = gpu::Dev().FrameNumber();
        }
        ImGui::EndChild();
        ImGui::End();
        if (ok) finish(true);
        else if (cancel || !keep) finish(false);
        return;
    }
}

}  // namespace sx
