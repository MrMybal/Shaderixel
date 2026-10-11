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
#include <fstream>
#include <cstring>

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

float GetParam(const EffectInstance& fx, const char* id) {
    EffectDefPtr def = fx.Def();
    if (!def) return 0;
    for (size_t i = 0; i < def->params.size(); ++i)
        if (def->params[i].id == id) return i < fx.params.size() ? fx.params[i].x : def->params[i].def.x;
    return 0;
}

// Straight colors of a readback (RGBA8 / RGBA16F / RGBA32F, premultiplied surfaces).
bool ReadbackColors(const gpu::ReadbackPtr& rb, std::vector<float4>& out) {
    if (!rb || !rb->ready) return false;
    const size_t n = (size_t)rb->width * rb->height;
    out.clear();
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        float4 c;
        if (rb->format == gpu::Format::RGBA32F && rb->data.size() >= (i + 1) * 16) {
            memcpy(&c, rb->data.data() + i * 16, 16);
        } else if (rb->format == gpu::Format::RGBA16F && rb->data.size() >= (i + 1) * 8) {
            const u16* h = (const u16*)rb->data.data() + i * 4;
            c = {HalfToFloat(h[0]), HalfToFloat(h[1]), HalfToFloat(h[2]), HalfToFloat(h[3])};
        } else if (rb->format == gpu::Format::RGBA8 && rb->data.size() >= (i + 1) * 4) {
            const u8* b = rb->data.data() + i * 4;
            c = {b[0] / 255.0f, b[1] / 255.0f, b[2] / 255.0f, b[3] / 255.0f};
        } else {
            return !out.empty();
        }
        if (c.w > 1e-4f) out.push_back({c.x / c.w, c.y / c.w, c.z / c.w, c.w});
    }
    return !out.empty();
}

// Temperature / tint making the (pre white balance) color neutral, in the camera_raw model:
// rgb *= (1 + .35 t, 1 - .25 g, 1 - .35 t) * (1 + .1 g, 1, 1 + .1 g).
void SetWhiteBalance(EffectInstance& fx, float r, float g, float b) {
    r = std::max(r, 1e-5f);
    g = std::max(g, 1e-5f);
    b = std::max(b, 1e-5f);
    float t = std::clamp((b - r) / (0.35f * (r + b)), -1.0f, 1.0f);
    float a = (1 + 0.35f * t) * r;
    float tn = std::clamp((g - a) / (0.1f * a + 0.25f * g), -1.0f, 1.0f);
    SetParam(fx, "temperature", t * 100);
    SetParam(fx, "tint", tn * 100);
}

struct RawPreset {
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<RawPreset>& BuiltinRawPresets() {
    static const std::vector<RawPreset> p = {
        {"Neutre", {}},
        {"Vif", {{"contrast", 20}, {"clarity", 15}, {"vibrance", 40}, {"saturation", 10}}},
        {"Paysage", {{"highlights", -40}, {"shadows", 30}, {"clarity", 25}, {"dehaze", 15}, {"vibrance", 35}, {"mix_s_blue", 15}, {"mix_s_green", 10}}},
        {"Portrait doux", {{"highlights", -20}, {"shadows", 15}, {"fine_detail", -15}, {"clarity", -20}, {"vibrance", 10}, {"mix_s_orange", -10}, {"mix_l_orange", 10}}},
        {"Noir et blanc", {{"saturation", -100}, {"contrast", 15}, {"clarity", 10}}},
        {"Noir et blanc contrasté", {{"saturation", -100}, {"contrast", 45}, {"clarity", 30}, {"blacks", -20}, {"whites", 15}, {"grain", 15}}},
        {"Film délavé", {{"contrast", -25}, {"blacks", 25}, {"saturation", -15}, {"cg_sh_hue", 200}, {"cg_sh_sat", 15}, {"cg_hl_hue", 40}, {"cg_hl_sat", 15}, {"grain", 20}}},
        {"Tons chauds", {{"temperature", 20}, {"vibrance", 20}, {"cg_hl_hue", 35}, {"cg_hl_sat", 20}, {"cg_sh_hue", 25}, {"cg_sh_sat", 10}}},
        {"Tons froids", {{"temperature", -20}, {"tint", 5}, {"contrast", 10}, {"cg_sh_hue", 215}, {"cg_sh_sat", 20}}},
        {"Teal & orange", {{"contrast", 15}, {"vibrance", 15}, {"cg_sh_hue", 190}, {"cg_sh_sat", 30}, {"cg_hl_hue", 30}, {"cg_hl_sat", 25}, {"mix_s_orange", 10}}},
        {"Haute clé", {{"exposure", 0.7f}, {"contrast", -15}, {"highlights", -30}, {"shadows", 40}, {"whites", 20}}},
        {"Basse clé", {{"exposure", -0.5f}, {"contrast", 25}, {"shadows", -20}, {"blacks", -25}, {"vignette", -30}}},
    };
    return p;
}

std::string RawPresetDir() {
    const char* la = getenv("LOCALAPPDATA");
    return la ? std::string(la) + "\\Shaderixel\\photo_raw_presets" : std::string();
}

// Parameters of a preset are applied over the defaults; white balance and exposure are kept unless the
// preset sets them.
void ApplyRawValues(EffectInstance& fx, const std::vector<std::pair<std::string, float>>& values, const CurvePoints* curve) {
    EffectDefPtr def = fx.Def();
    if (!def) return;
    const float keep[3] = {GetParam(fx, "temperature"), GetParam(fx, "tint"), GetParam(fx, "exposure")};
    fx.params = def->DefaultParams();
    fx.curves.assign(def->params.size(), {});
    SetParam(fx, "temperature", keep[0]);
    SetParam(fx, "tint", keep[1]);
    SetParam(fx, "exposure", keep[2]);
    for (auto& [id, val] : values) SetParam(fx, id.c_str(), val);
    if (curve)
        for (size_t i = 0; i < def->params.size(); ++i)
            if (def->params[i].id == "tone") fx.curves[i] = *curve;
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

bool App::LensPickFromShot(const Document& doc) {
    EnsureLensfun();
    auto& P = lensPick;
    LensfunDb& db = Lensfun();
    const Document::Shot& s = doc.shot;
    if (!db.Loaded() || (s.lens.empty() && s.model.empty())) return false;
    const LensfunLens* l = !s.lens.empty() ? db.FindLens(s.lens, "") : nullptr;
    if (!l && !s.model.empty()) l = db.FindLens(s.model, s.make);  // compact: lens named after the camera
    if (!l) return false;
    P.lens = (int)(l - db.lenses.data());
    P.focal = s.focal > 0 ? s.focal : l->minFocal;
    P.aperture = s.aperture > 0 ? s.aperture : 8;
    const LensfunCamera* cam = db.FindCamera(s.make, s.model);
    P.crop = cam ? cam->crop : s.crop;
    return true;
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
        if (LensPickFromShot(*doc)) apply = true;
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

// ------------------------------------------------------------------ Photo RAW: automatic settings, presets

void App::PhotoRawAuto(DocView& v, Layer& l, int want) {
    // Statistics on a small copy of the layer's pixels (before the filters).
    Surface* s = v.doc->GetSurface(l.surface);
    gpu::Texture* src = s && s->tex ? s->tex.get() : v.lastImage;
    if (!src) return;
    const u32 w = 256, h = std::max(1u, (u32)std::lround(256.0 * src->Height() / std::max(1u, src->Width())));
    camRaw.statsTex = v.renderer.PreviewCopy(*ctx, src, w, h);
    if (!camRaw.statsTex) return;
    camRaw.statsRb = ctx->ReadbackTexture(camRaw.statsTex.get());
    camRaw.statsWant = want;
}

void App::PhotoRawPoll(DocView& v, Layer& l) {
    auto& C = camRaw;
    EffectInstance* fx = FindFilter(&l, "camera_raw");
    std::vector<float4> px;
    if (C.wbRb && C.wbRb->ready) {
        if (fx && ReadbackColors(C.wbRb, px)) {
            float4 m = {0, 0, 0, 0};
            for (auto& p : px) m = {m.x + p.x, m.y + p.y, m.z + p.z, 0};
            SetWhiteBalance(*fx, m.x, m.y, m.z);
            v.doc->SyncGraph();
            Status("Balance des blancs : température " + std::to_string((int)std::lround(GetParam(*fx, "temperature"))) +
                   ", teinte " + std::to_string((int)std::lround(GetParam(*fx, "tint"))));
        }
        C.wbRb.reset();
    }
    if (C.statsRb && C.statsRb->ready) {
        if (fx && ReadbackColors(C.statsRb, px)) {
            if (C.statsWant & 1) {
                // Gray world on the unclipped, not too dark pixels, the most neutral ones weighted more.
                double r = 0, g = 0, b = 0;
                for (auto& p : px) {
                    float mx = std::max(p.x, std::max(p.y, p.z)), mn = std::min(p.x, std::min(p.y, p.z));
                    if (mx > 0.95f || mx < 0.01f) continue;
                    float wgt = 1.0f / (0.05f + (mx - mn) / mx);
                    r += p.x * wgt;
                    g += p.y * wgt;
                    b += p.z * wgt;
                }
                if (g > 0) SetWhiteBalance(*fx, (float)r, (float)g, (float)b);
            }
            if (C.statsWant & 2) {
                std::vector<float> L;
                L.reserve(px.size());
                double logSum = 0;
                for (auto& p : px) {
                    float y = 0.2126f * p.x + 0.7152f * p.y + 0.0722f * p.z;
                    L.push_back(y);
                    logSum += std::log(y + 1e-4);
                }
                const float avg = (float)std::exp(logSum / L.size());
                const float ev = std::clamp(std::log2(0.18f / std::max(avg, 1e-4f)), -3.0f, 4.0f);
                std::sort(L.begin(), L.end());
                auto pct = [&](float q) { return L[std::min(L.size() - 1, (size_t)(q * (L.size() - 1)))] * std::exp2(ev); };
                const float hi = pct(0.995f), lo = pct(0.05f), black = pct(0.005f);
                SetParam(*fx, "exposure", std::round(ev * 100) / 100);
                SetParam(*fx, "highlights", hi > 0.9f ? -std::clamp(50 * std::log2(hi / 0.9f) + 10, 0.0f, 80.0f) : 0.0f);
                SetParam(*fx, "whites", hi < 0.7f ? std::clamp((0.7f - hi) / 0.7f * 60, 0.0f, 40.0f) : 0.0f);
                SetParam(*fx, "shadows", lo < 0.03f ? std::clamp((0.03f - lo) / 0.03f * 45, 0.0f, 50.0f) : 0.0f);
                SetParam(*fx, "blacks", black > 0.02f ? -std::clamp((black - 0.02f) * 400, 0.0f, 35.0f) : 0.0f);
                SetParam(*fx, "contrast", 10);
            }
            v.doc->SyncGraph();
            Status(C.statsWant == 1 ? "Balance des blancs automatique" : "Réglages automatiques");
        }
        C.statsRb.reset();
        C.statsTex.reset();
        C.statsWant = 0;
    }
}

Json App::PhotoRawSettings(const EffectInstance& fx) {
    Json ps = Json::Object();
    EffectDefPtr def = fx.Def();
    for (size_t i = 0; def && i < def->params.size() && i < fx.params.size(); ++i) {
        const ParamDef& p = def->params[i];
        if (p.type == ParamType::Curve) {
            if (i < fx.curves.size() && fx.curves[i].size() >= 2) {
                Json c = Json::Array();
                for (auto& pt : fx.curves[i]) c.Push(ToJson(pt));
                ps["tone"] = c;
            }
            continue;
        }
        if (fx.params[i].x != p.def.x) ps[p.id] = fx.params[i].x;
    }
    return ps;
}

int App::PhotoRawStep(DocView& v, const Json& p, std::string& error, int& retries) {
    Document& d = *v.doc;
    Layer* l = d.ActiveLayer();
    if (!l) { error = "Photo RAW : aucun calque actif"; return 2; }
    if (retries == 0) {
        // Settings over the defaults, lens profile, then (optionally) the automatic white balance / tones.
        Edit e(d, v.history, "Photo RAW");
        EffectInstance* fx = FindFilter(l, "camera_raw");
        if (!fx) {
            EffectInstance n;
            n.effectId = "camera_raw";
            if (auto def = Effects().Find("camera_raw")) n.params = def->DefaultParams();
            l->filters.push_back(n);
            fx = &l->filters.back();
        }
        std::vector<std::pair<std::string, float>> vals;
        CurvePoints curve;
        for (auto& [k, val] : p["settings"].Items()) {
            if (k == "tone") { for (auto& pt : val.Elements()) curve.push_back(Float2FromJson(pt)); continue; }
            vals.push_back({k, val.F()});
        }
        SetParam(*fx, "temperature", 0);
        SetParam(*fx, "tint", 0);
        SetParam(*fx, "exposure", 0);
        ApplyRawValues(*fx, vals, curve.size() >= 2 ? &curve : nullptr);
        if (p["lens"].B() && !FindFilter(l, "lens_profile") && LensPickFromShot(d)) {
            EffectInstance lens;
            lens.effectId = "lens_profile";
            if (auto def = Effects().Find("lens_profile")) lens.params = def->DefaultParams();
            ApplyLensPick(lens);
            l->filters.insert(l->filters.begin(), lens);
        }
        d.SyncGraph();
        e.Commit();
        if (!p["auto"].B()) return 0;
        PhotoRawAuto(v, *l, 3);
        retries = 1;
        return 1;
    }
    PhotoRawPoll(v, *l);
    if (camRaw.statsRb) return ++retries > 600 ? (error = "Photo RAW : statistiques non lues", 2) : 1;
    return 0;
}

void App::PhotoRawToAction(DocView& v, Layer& l, bool openBatch) {
    EffectInstance* fx = FindFilter(&l, "camera_raw");
    if (!fx) return;
    EffectInstance* lens = FindFilter(&l, "lens_profile");
    Json p = Json::Object();
    p["settings"] = PhotoRawSettings(*fx);
    p["auto"] = false;
    p["lens"] = lens && lens->enabled;
    std::string base = "Photo RAW - " + (v.doc->path.empty() ? v.doc->Name() : Narrow(Utf8Path(v.doc->path).stem().wstring()));
    std::string name = base;
    for (int k = 2;; ++k) {
        bool used = false;
        for (auto& a : actions) used |= a.name == name;
        if (!used) break;
        name = base + " (" + std::to_string(k) + ")";
    }
    actions.push_back({name, {ActionStep{"photo_raw", p, true}}, ""});
    SaveActions();
    if (openBatch) {
        auto& B = batch;
        B.open = true;
        B.sourceKind = 0;
        B.chain = {"u:" + name};
        B.format = 2;  // JPG
        B.extFilter.clear();
        B.dest.clear();
        Status("Traitement par lots préparé avec l'action « " + name + " » : choisissez le dossier de RAW puis Démarrer");
    } else {
        Status("Action créée : « " + name + " » (panneau Actions, traitement par lots)");
    }
}

void App::PhotoRawPresetsUI(EffectInstance& fx, DocView& v) {
    auto& C = camRaw;
    namespace fs = std::filesystem;
    // User presets: one JSON per preset ({"params": {id: value}, "tone": [[x, y]...]}).
    std::vector<std::pair<std::string, std::string>> user;  // name, path
    {
        static std::vector<std::pair<std::string, std::string>> cache;
        static double scanned = -10;
        if (ImGui::GetTime() - scanned > 2) {
            scanned = ImGui::GetTime();
            cache.clear();
            std::error_code ec;
            for (auto& e : fs::directory_iterator(Utf8Path(RawPresetDir()), ec))
                if (e.path().extension() == ".json") cache.push_back({Narrow(e.path().stem().wstring()), Narrow(e.path().wstring())});
        }
        user = cache;
    }
    const auto& built = BuiltinRawPresets();
    const int nb = (int)built.size();
    std::string cur = C.preset >= 0 && C.preset < nb ? built[C.preset].name : C.preset >= nb && C.preset - nb < (int)user.size() ? user[C.preset - nb].first : "Choisir...";
    DocState before = v.doc->S();
    bool applied = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##rawpreset", ("Préréglage : " + cur).c_str())) {
        for (int i = 0; i < nb; ++i)
            if (ImGui::Selectable(built[i].name, C.preset == i)) {
                std::vector<std::pair<std::string, float>> vals;
                for (auto& [k, val] : built[i].values) vals.push_back({k, val});
                ApplyRawValues(fx, vals, nullptr);
                C.preset = i;
                applied = true;
            }
        if (!user.empty()) ImGui::SeparatorText("Mes préréglages");
        for (int i = 0; i < (int)user.size(); ++i) {
            if (ImGui::Selectable(user[i].first.c_str(), C.preset == nb + i)) {
                std::string text;
                Json j;
                if (ReadTextFile(user[i].second, text) && Json::Parse(text, j)) {
                    std::vector<std::pair<std::string, float>> vals;
                    for (auto& [k, val] : j["params"].Items()) vals.push_back({k, val.F()});
                    CurvePoints curve;
                    for (auto& p : j["tone"].Elements()) curve.push_back(Float2FromJson(p));
                    ApplyRawValues(fx, vals, curve.size() >= 2 ? &curve : nullptr);
                    C.preset = nb + i;
                    applied = true;
                }
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Supprimer ce préréglage")) {
                    std::error_code ec;
                    fs::remove(Utf8Path(user[i].second), ec);
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Enregistrer...")) ImGui::OpenPopup("##savepreset");
    ImGui::SameLine();
    if (ImGui::Button("Copier")) {
        rawClipboard = fx;
        Status("Réglages Photo RAW copiés");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!rawClipboard);
    if (ImGui::Button("Coller") && rawClipboard) {
        fx.params = rawClipboard->params;
        fx.curves = rawClipboard->curves;
        applied = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Copiez les réglages d'une photo puis collez-les sur une autre (Filtre > Photo RAW...).");
    ImGui::SameLine();
    if (ImGui::Button("Lot...")) ImGui::OpenPopup("##rawbatch");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Appliquer ces réglages à d'autres photos (action, traitement par lots)");
    if (ImGui::BeginPopup("##rawbatch")) {
        if (Layer* l = v.doc->FindLayer(camRaw.layer)) {
            if (ImGui::MenuItem("Traiter un dossier de RAW avec ces réglages...")) PhotoRawToAction(v, *l, true);
            if (ImGui::MenuItem("Enregistrer comme action")) PhotoRawToAction(v, *l, false);
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##savepreset")) {
        ImGui::SetNextItemWidth(240 * dpi);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##pn", "Nom du préréglage", C.presetName, sizeof(C.presetName));
        ImGui::Checkbox("Inclure la balance des blancs et l'exposition", &C.presetWB);
        if (ImGui::Button("Enregistrer", ImVec2(120 * dpi, 0)) && C.presetName[0]) {
            EffectDefPtr def = fx.Def();
            Json j = Json::Object(), ps = Json::Object();
            for (size_t i = 0; def && i < def->params.size() && i < fx.params.size(); ++i) {
                const ParamDef& p = def->params[i];
                if (p.type == ParamType::Curve) {
                    if (i < fx.curves.size() && fx.curves[i].size() >= 2) {
                        Json c = Json::Array();
                        for (auto& pt : fx.curves[i]) c.Push(ToJson(pt));
                        j["tone"] = c;
                    }
                    continue;
                }
                if (!C.presetWB && (p.id == "temperature" || p.id == "tint" || p.id == "exposure")) continue;
                if (fx.params[i].x != p.def.x) ps[p.id] = fx.params[i].x;
            }
            j["params"] = ps;
            std::error_code ec;
            fs::create_directories(Utf8Path(RawPresetDir()), ec);
            std::string name = C.presetName;
            for (auto& ch : name)
                if (strchr("\\/:*?\"<>|", ch)) ch = '_';
            std::ofstream(Utf8Path(RawPresetDir() + "\\" + name + ".json"), std::ios::binary) << j.Dump(1);
            Status("Préréglage enregistré : " + name);
            C.presetName[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (applied) {
        v.doc->SyncGraph();
        v.history.PushState("Préréglage Photo RAW", before, v.doc->S());
    }
}

// ------------------------------------------------------------------ Camera Raw

void App::OpenCameraRaw(DocView& v) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) {
        Status("Photo RAW : uniquement pour les documents en calques");
        return;
    }
    Layer* l = d.ActiveLayer();
    if (!l) {
        Status("Photo RAW : aucun calque actif");
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
    PhotoRawPoll(*v, *l);
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
                v->history.PushState("Photo RAW", *C.before, d.S());
                d.dirty = true;
            }
        } else if (C.before) {
            d.S() = *C.before;
            d.InvalidateSurfaces();
        }
        d.SyncGraph();
        C.open = false;
        C.tex.reset();
        C.pickWB = false;
    };

    ImGuiViewport* vp0 = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp0->WorkPos.x + vp0->WorkSize.x * 0.5f, vp0->WorkPos.y + vp0->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(vp0->WorkSize.x * 0.92f, vp0->WorkSize.y * 0.92f), ImGuiCond_Appearing);
    if (C.firstFrame) ImGui::SetNextWindowFocus();
    bool keep = true;
    std::string title = "Photo RAW - " + d.Name() + "###cameraraw";
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
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_W, false)) C.pickWB = !C.pickWB;

        ImVec2 avail = ImGui::GetContentRegionAvail();
        u32 vw = (u32)std::max(16.0f, avail.x), vh = (u32)std::max(16.0f, avail.y);
        if (!C.tex || C.tex->Width() != vw || C.tex->Height() != vh) C.tex = gpu::Dev().CreateTexture(vw, vh, gpu::Format::RGBA8, "CameraRawView");
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##crimage", ImVec2((float)vw, (float)vh));
        const float docW = (float)d.Width(), docH = (float)d.Height();
        float inv = C.zoom == 0 ? std::max(docW / vw, docH / vh) : 1.0f / zv[std::clamp(C.zoom, 0, 4)];
        const bool picking = C.pickWB;
        if (picking && ImGui::IsItemHovered()) {
            // Eyedropper: a crosshair; the click reads 5 x 5 pixels of the layer under it.
            ImVec2 m = ImGui::GetIO().MousePos;
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            dl->AddCircle(m, 7 * dpi, IM_COL32(255, 255, 255, 230), 0, 1.5f);
            dl->AddLine(ImVec2(m.x - 12 * dpi, m.y), ImVec2(m.x + 12 * dpi, m.y), IM_COL32(0, 0, 0, 200));
            dl->AddLine(ImVec2(m.x, m.y - 12 * dpi), ImVec2(m.x, m.y + 12 * dpi), IM_COL32(0, 0, 0, 200));
            ImGui::SetTooltip("Cliquez sur une zone qui doit être neutre (gris, blanc)");
            if (ImGui::IsItemClicked(0)) {
                const float ox = C.zoom == 0 ? (docW - vw * inv) * 0.5f : C.pan.x * docW - vw * inv * 0.5f;
                const float oy = C.zoom == 0 ? (docH - vh * inv) * 0.5f : C.pan.y * docH - vh * inv * 0.5f;
                const int x = (int)(ox + (m.x - p0.x) * inv), y = (int)(oy + (m.y - p0.y) * inv);
                Surface* s = d.GetSurface(l->surface);
                gpu::Texture* src = s && s->tex ? s->tex.get() : nullptr;
                if (src && x >= 0 && y >= 0 && x < (int)src->Width() && y < (int)src->Height()) {
                    Rect r{std::max(0, x - 2), std::max(0, y - 2), std::min((int)src->Width(), x + 3), std::min((int)src->Height(), y + 3)};
                    C.wbRb = ctx->ReadbackTexture(src, &r);
                }
                C.pickWB = false;
            }
        } else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
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
        if (!picking && C.zoom != 0 && ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0)) {
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
        if (cr) PhotoRawPresetsUI(*cr, *v);
        cr = FindFilter(l, "camera_raw");
        if (cr) {
            ImGui::PushItemWidth(-150 * dpi);
            if (header("Base", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button(C.pickWB ? "Pipette... (W)" : "Pipette (W)")) C.pickWB = !C.pickWB;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Balance des blancs : cliquez dans l'image sur une zone neutre");
                ImGui::SameLine();
                if (ImGui::Button("Balance auto")) PhotoRawAuto(*v, *l, 1);
                ImGui::SameLine();
                if (ImGui::Button("Tel quel")) {
                    DocState before = d.S();
                    SetParam(*cr, "temperature", 0);
                    SetParam(*cr, "tint", 0);
                    d.SyncGraph();
                    v->history.PushState("Balance des blancs", before, d.S());
                }
                ImGui::SameLine();
                if (ImGui::Button("Auto")) PhotoRawAuto(*v, *l, 3);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Balance des blancs, exposition, hautes lumières, ombres, blancs et noirs d'après l'image");
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
                v->history.PushState("Réinitialiser Photo RAW", before, d.S());
            }
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 210 * dpi);
        bool cancel = ImGui::Button("Annuler", ImVec2(100 * dpi, 0));
        ImGui::SameLine();
        bool ok = ImGui::Button("OK", ImVec2(100 * dpi, 0));
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            if (C.pickWB) C.pickWB = false;
            else cancel = true;
        }
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
