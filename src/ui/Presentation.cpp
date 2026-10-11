// Presentation of a Paint 3D model (Marmoset Toolbag-like): lighting presets and
// extra lights, depth of field with focus picking, camera bookmarks with smooth
// transitions, and turntable export (the model turns: MP4 video and / or PNG frames).
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <stb_image_write.h>

namespace sx {

namespace {
constexpr float kDeg = 57.29578f;

struct LightingPreset {
    const char* name;
    const char* help;
    float keyIntensity;
    bool keyFollow;
    float keyYaw, keyPitch;  // degrees (fixed key light)
    std::vector<MeshLight> lights;
};

MeshLight L(float yawDeg, float pitchDeg, float3 color, float intensity, bool follow = true) {
    MeshLight l;
    l.yaw = yawDeg / kDeg;
    l.pitch = pitchDeg / kDeg;
    l.color = color;
    l.intensity = intensity;
    l.follow = follow;
    return l;
}

const std::vector<LightingPreset>& LightingPresets() {
    static const std::vector<LightingPreset> p = {
        {"Simple", "La lumière principale seule (suit la caméra).", 1.6f, true, 0, 50, {}},
        {"Studio 3 points", "Principale, débouchage froid à gauche, contre-jour qui détache la silhouette.", 1.5f, true, 0, 50,
         {L(-70, 15, {0.75f, 0.85f, 1.0f}, 0.45f), L(165, 35, {1.0f, 0.95f, 0.9f}, 1.6f)}},
        {"Contre-jour", "Lumière forte derrière le modèle, face sombre : contours lumineux.", 0.35f, true, 0, 50,
         {L(150, 25, {1.0f, 0.9f, 0.8f}, 2.2f), L(-150, 25, {0.8f, 0.9f, 1.0f}, 1.6f)}},
        {"Doux", "Éclairage enveloppant, peu de contraste (présentation de textures).", 0.9f, true, 0, 50,
         {L(-90, 20, {1, 1, 1}, 0.6f), L(90, 20, {1, 1, 1}, 0.6f), L(180, 60, {1, 1, 1}, 0.5f)}},
        {"Dramatique", "Une lumière latérale dure et un liseré chaud : forts contrastes.", 2.2f, false, -80, 25,
         {L(160, 20, {1.0f, 0.65f, 0.35f}, 1.4f)}},
        {"Coucher de soleil", "Lumière rasante orangée et ciel bleuté en débouchage.", 2.0f, false, 60, 10,
         {L(-120, 40, {0.45f, 0.6f, 1.0f}, 0.5f, false)}},
    };
    return p;
}

}  // namespace

int LightingPresetCount() { return (int)LightingPresets().size(); }

void ApplyLightingPreset(MeshScene& s, int index) {
    const LightingPreset& p = LightingPresets()[std::clamp(index, 0, LightingPresetCount() - 1)];
    s.lightIntensity = p.keyIntensity;
    if (s.envAtlas && s.sunIntensity <= 0) s.sunIntensity = p.keyIntensity * 0.6f;
    s.lightFollow = p.keyFollow;
    s.lightYaw = p.keyYaw / kDeg;
    s.lightPitch = p.keyPitch / kDeg;
    s.lights = p.lights;
}

namespace {
float LerpAngle(float a, float b, float t) {
    float d = std::fmod(b - a, 6.2831853f);
    if (d > 3.14159265f) d -= 6.2831853f;
    if (d < -3.14159265f) d += 6.2831853f;
    return a + d * t;
}
}  // namespace

void App::DrawLightingSettings(MeshScene& s) {
    ImGui::SeparatorText("Éclairage");
    const auto& P = LightingPresets();
    if (ImGui::BeginCombo("Préréglage##light", "Choisir...")) {
        for (int i = 0; i < (int)P.size(); ++i) {
            if (ImGui::Selectable(P[i].name)) ApplyLightingPreset(s, i);
            ui::HelpTooltip(P[i].help);
        }
        ImGui::EndCombo();
    }
    int remove = -1;
    for (int i = 0; i < (int)s.lights.size(); ++i) {
        MeshLight& l = s.lights[i];
        ImGui::PushID(i);
        ImGui::Checkbox("##on", &l.on);
        ImGui::SameLine();
        const bool open = ImGui::TreeNodeEx("##l", ImGuiTreeNodeFlags_SpanAvailWidth, "Lumière %d", i + 2);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
        if (ImGui::SmallButton("x")) remove = i;
        if (open) {
            float yaw = l.yaw * kDeg, pitch = l.pitch * kDeg;
            if (ImGui::SliderFloat("Direction", &yaw, -180, 180, "%.0f°")) l.yaw = yaw / kDeg;
            if (ImGui::SliderFloat("Hauteur", &pitch, -60, 89, "%.0f°")) l.pitch = pitch / kDeg;
            ImGui::ColorEdit3("Couleur", &l.color.x, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            ImGui::Checkbox("Suit la caméra", &l.follow);
            ImGui::SliderFloat("Intensité", &l.intensity, 0, 6, "%.2f");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (remove >= 0) s.lights.erase(s.lights.begin() + remove);
    if ((int)s.lights.size() < kMaxMeshLights && ImGui::SmallButton("+ Lumière")) s.lights.push_back(L(-70, 20, {1, 1, 1}, 0.6f));
    ui::HelpTooltip("Lumières directionnelles supplémentaires (débouchage, contre-jour...).\nDirection 0° : depuis la caméra, 180° : derrière le modèle.");
}

void App::DrawCameraBookmarks(MeshScene& s) {
    ImGui::SeparatorText("Caméras");
    int remove = -1;
    for (int i = 0; i < (int)s.cameras.size(); ++i) {
        CameraBookmark& c = s.cameras[i];
        ImGui::PushID(i);
        if (ImGui::Button(c.name.c_str(), ImVec2(140 * dpi, 0))) {
            camAnim.on = true;
            camAnim.t = 0;
            camAnim.from = {"", s.yaw, s.pitch, s.distance, s.fov, s.target};
            camAnim.to = c;
        }
        ui::HelpTooltip("Aller à ce point de vue");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120 * dpi);
        ImGui::InputText("##name", &c.name);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remplacer")) c = {c.name, s.yaw, s.pitch, s.distance, s.fov, s.target};
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) remove = i;
        ImGui::PopID();
    }
    if (remove >= 0) s.cameras.erase(s.cameras.begin() + remove);
    if (ImGui::SmallButton("+ Enregistrer la vue"))
        s.cameras.push_back({"Vue " + std::to_string(s.cameras.size() + 1), s.yaw, s.pitch, s.distance, s.fov, s.target});
    ui::HelpTooltip("Garde l'angle, le cadrage et la focale actuels (enregistrés avec le projet).");
    float fovDeg = s.fov * kDeg;
    ImGui::SetNextItemWidth(220 * dpi);
    if (ImGui::SliderFloat("Focale (champ)", &fovDeg, 10, 100, "%.0f°")) s.fov = fovDeg / kDeg;
    ui::HelpTooltip("Angle de champ vertical : petit = téléobjectif (moins de déformation), grand = grand-angle.");
}

void App::UpdateCameraAnimation(MeshScene& s) {
    if (!camAnim.on) return;
    camAnim.t = std::min(1.0f, camAnim.t + ImGui::GetIO().DeltaTime / 0.45f);
    const float k = camAnim.t * camAnim.t * (3 - 2 * camAnim.t);  // ease in / out
    const CameraBookmark &a = camAnim.from, &b = camAnim.to;
    s.yaw = LerpAngle(a.yaw, b.yaw, k);
    s.pitch = a.pitch + (b.pitch - a.pitch) * k;
    s.distance = a.distance * std::pow(b.distance / std::max(a.distance, 1e-4f), k);
    s.fov = a.fov + (b.fov - a.fov) * k;
    s.target = a.target + (b.target - a.target) * k;
    if (camAnim.t >= 1) camAnim.on = false;
}

bool App::PickFocus(MeshScene& s, float2 local, u32 w, u32 h) {
    if (sim.rc.Empty() || sim.scene != &s || sim.rev != s.revision) {
        sim.rc.Build(s.data);
        sim.scene = &s;
        sim.rev = s.revision;
    }
    float3 o, d, p, n;
    CameraRay(s, local, w, h, o, d);
    float t = 0;
    if (!sim.rc.Ray(o, d, 1000.0f, &p, &n, &t)) return false;
    // The path tracer's focus plane is measured along the view axis.
    s.dofFocus = std::max(t * Dot(d, Normalize(s.target - s.camPos)), 0.01f);
    return true;
}

// ---------------------------------------------------------------- turntable

void App::DrawTurntableDialog() {
    auto& T = turntable;
    if (!T.open) return;
    DocView* v = Active();
    if (!v || !v->mesh) { T.open = false; return; }
    ImGui::SetNextWindowSize(ImVec2(460 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Turntable", &T.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420 * dpi);
    ImGui::TextDisabled("Le modèle fait un tour complet devant la caméra, rendu image par image avec les réglages de la vue 3D "
                        "(ray tracing compris : chaque image attend ses échantillons).");
    ImGui::PopTextWrapPos();
    ImGui::BeginDisabled(T.running);
    ImGui::PushItemWidth(220 * dpi);
    ImGui::SliderInt("Images", &T.frames, 12, 720);
    ImGui::SliderInt("Images / s", &T.fps, 10, 60);
    ImGui::Text("Durée : %.1f s", (float)T.frames / std::max(T.fps, 1));
    ImGui::Checkbox("Taille de la vue", &T.viewSize);
    if (!T.viewSize) {
        static const int sizes[][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {1080, 1080}, {1080, 1920}};
        static const char* names = "1280 x 720\0" "1920 x 1080 (Full HD)\0" "2560 x 1440\0" "3840 x 2160 (4K)\0" "1080 x 1080 (carré)\0" "1080 x 1920 (vertical)\0";
        int cur = -1;
        for (int i = 0; i < 6; ++i) if (sizes[i][0] == T.width && sizes[i][1] == T.height) cur = i;
        if (ImGui::Combo("Taille", &cur, names)) {
            T.width = sizes[cur][0];
            T.height = sizes[cur][1];
        }
    }
    if (v->mesh->quality == 2) ImGui::SliderInt("Échantillons / image", &T.samples, 8, 2048, "%d", ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Éclairage fixe (le modèle tourne)", &T.fixedLighting);
    ui::HelpTooltip("Coché : lumières et HDRI tournent avec la caméra, comme si le modèle tournait sur un plateau.\n"
                    "Sinon la caméra tourne autour du modèle dans un éclairage fixe.");
    ImGui::Checkbox("Vidéo MP4", &T.mp4);
    ImGui::SameLine();
    ImGui::Checkbox("GIF animé", &T.gif);
    ui::HelpTooltip("À côté de la vidéo (même nom, .gif), 720 px de large au plus, en boucle : facile à partager.");
    ImGui::SameLine();
    ImGui::Checkbox("Images PNG", &T.png);
    ImGui::SetNextItemWidth(300 * dpi);
    ImGui::InputTextWithHint("##ttpath", "fichier .mp4", &T.path);
    ImGui::SameLine();
    if (ImGui::Button("...")) {
        std::string f = platform::SaveFileDialog(hwnd, L"Vidéo MP4 (*.mp4)\0*.mp4\0", L"mp4");
        if (!f.empty()) T.path = f;
    }
    ImGui::PopItemWidth();
    ImGui::EndDisabled();
    ImGui::Separator();
    if (!T.running) {
        ImGui::BeginDisabled(T.path.empty() || (!T.mp4 && !T.png && !T.gif) || !T.pending.empty());
        if (ImGui::Button("Lancer", ImVec2(140 * dpi, 0))) StartTurntable(*v);
        ImGui::EndDisabled();
    } else {
        ImGui::ProgressBar((float)T.frame / std::max(T.frames, 1), ImVec2(260 * dpi, 0));
        ImGui::SameLine();
        if (ImGui::Button("Arrêter")) StopTurntable();
    }
    if (!T.error.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", T.error.c_str());
    ImGui::End();
}

void App::StartTurntable(DocView& v) {
    auto& T = turntable;
    MeshScene& s = *v.mesh;
    T.error.clear();
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(T.path).parent_path(), ec);
    T.view = &v;
    T.running = true;
    T.frame = 0;
    T.settle = 0;
    T.written = 0;
    T.yaw0 = s.yaw;
    T.env0 = s.envRotation;
    T.lightYaw0 = s.lightYaw;
    T.samples0 = s.rtSamples;
    T.xYaw0.clear();
    for (auto& l : s.lights) T.xYaw0.push_back(l.yaw);
    T.pending.clear();
    T.video.reset();
    T.gifw.reset();
    camAnim.on = false;
    Status("Turntable : rendu de " + std::to_string(T.frames) + " images...");
}

void App::StopTurntable() {
    auto& T = turntable;
    if (!T.running) return;
    T.running = false;
    bool alive = false;
    for (auto& d : docs) alive |= d.get() == T.view;  // the document may have been closed meanwhile
    if (DocView* v = alive ? T.view : nullptr; v && v->mesh) {
        MeshScene& s = *v->mesh;
        s.yaw = T.yaw0;
        s.envRotation = T.env0;
        s.lightYaw = T.lightYaw0;
        s.rtSamples = T.samples0;
        for (size_t i = 0; i < s.lights.size() && i < T.xYaw0.size(); ++i) s.lights[i].yaw = T.xYaw0[i];
    }
}

void App::TurntableFrame(DocView& v, u32& w, u32& h) {
    auto& T = turntable;
    MeshScene& s = *v.mesh;
    if (!T.viewSize) {
        w = (u32)T.width;
        h = (u32)T.height;
    }
    const float a = 6.2831853f * (float)T.frame / (float)std::max(T.frames, 1);
    s.yaw = T.yaw0 + a;
    if (T.fixedLighting) {
        // Everything fixed to the camera turns with it: the model seems to turn.
        s.envRotation = T.env0 + a;
        if (!s.lightFollow) s.lightYaw = T.lightYaw0 + a;
        for (size_t i = 0; i < s.lights.size() && i < T.xYaw0.size(); ++i)
            if (!s.lights[i].follow) s.lights[i].yaw = T.xYaw0[i] + a;
    }
    if (s.quality == 2) s.rtSamples = std::max(T.samples, 1);
}

void App::TurntableCapture(DocView& v, gpu::Texture* img) {
    auto& T = turntable;
    MeshScene& s = *v.mesh;
    if (!img) return;
    if (s.quality == 2 && s.viewChannel == 0) {
        if (!v.viewRT || RenderSamples(*v.viewRT) < std::max(T.samples, 1)) return;
    } else if (++T.settle < 3) {
        return;  // masks / projections of the new angle settle
    }
    T.settle = 0;
    T.pending.push_back(ctx->ReadbackTexture(img));
    if (++T.frame >= T.frames) {
        StopTurntable();
        Status("Turntable : écriture des images...");
    }
}

void App::PollTurntable() {
    auto& T = turntable;
    if (T.running) {
        bool alive = false;
        for (auto& d : docs) alive |= d.get() == T.view;
        if (!alive) StopTurntable();
    }
    while (!T.pending.empty() && T.pending.front() && T.pending.front()->ready) {
        const gpu::Readback& rb = *T.pending.front();
        const auto base = Utf8Path(T.path);
        if (T.mp4 && !T.video && T.error.empty()) {
            T.video = std::make_unique<VideoWriter>();
            if (!T.video->Open(T.path, rb.width, rb.height, (u32)T.fps, &T.error)) {
                T.video.reset();
                T.error = "Vidéo : " + T.error;
                Status("Turntable : " + T.error);
            }
        }
        if (T.video) T.video->Write(rb.data.data(), rb.width, rb.height);
        if (T.gif && !T.gifw && T.error.empty()) {
            T.gifw = std::make_unique<GifWriter>();
            std::string err;
            if (!T.gifw->Open(Narrow((base.parent_path() / base.stem()).wstring()) + ".gif", (u32)T.fps, 720, &err)) {
                T.gifw.reset();
                T.error = "GIF : " + err;
            }
        }
        if (T.gifw) T.gifw->Write(rb.data.data(), rb.width, rb.height);
        if (T.png) {
            char num[16];
            snprintf(num, sizeof(num), "_%04d.png", T.written + 1);
            const std::string file = Narrow((base.parent_path() / base.stem()).wstring()) + num;
            stbi_write_png(file.c_str(), rb.width, rb.height, 4, rb.data.data(), rb.width * 4);
        }
        ++T.written;
        T.pending.erase(T.pending.begin());
    }
    if (!T.running && T.pending.empty() && T.written > 0) {
        bool ok = !T.video || T.video->Close();
        T.video.reset();
        if (T.gifw) ok = T.gifw->Close() && ok;
        T.gifw.reset();
        Status(ok && T.error.empty() ? "Turntable exporté : " + std::to_string(T.written) + " images" + (T.mp4 ? " -> " + T.path : "")
                                     : "Turntable : " + (T.error.empty() ? std::string("échec de la vidéo") : T.error));
        T.written = 0;
    }
}

}  // namespace sx
