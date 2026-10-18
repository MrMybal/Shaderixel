// Baker (Paint 3D, Marmoset / Substance style): mesh maps from the project's model
// or from a high poly model projected on it (cage distances) - normal, ambient
// occlusion, curvature, thickness, height, position, world normal. The results go
// into the project's channels (Normal, Occlusion, Height), feed the smart masks and
// can be written as PNG files.
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

#include <imgui_stdlib.h>
#include <filesystem>
#include <stb_image_write.h>

namespace sx {

namespace {

u16 ToHalf(float v) {
    if (v <= 0) return 0;
    u32 b;
    memcpy(&b, &v, 4);
    int e = (int)((b >> 23) & 255) - 127 + 15;
    if (e <= 0) return 0;
    if (e >= 31) return 0x7BFF;
    return (u16)((e << 10) | ((b >> 13) & 0x3FF));
}

}  // namespace

void App::WriteLayerPixels(Document& d, Id surface, const std::vector<float>& src, int channels, int sw, int sh, bool dataChannel) {
    // Straight values (0..1) -> the layer (document size, bilinear): data channels keep visual gray levels.
    Surface* s = d.GetSurface(surface);
    if (!s || src.size() < (size_t)sw * sh * channels) return;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto at = [&](int x, int y, int k) { return src[((size_t)std::clamp(y, 0, sh - 1) * sw + std::clamp(x, 0, sw - 1)) * channels + k]; };
    std::vector<float> rgba((size_t)W * H * 4, 1.0f);
    for (u32 y = 0; y < H; ++y)
        for (u32 x = 0; x < W; ++x) {
            const float fx = (x + 0.5f) * sw / W - 0.5f, fy = (y + 0.5f) * sh / H - 0.5f;
            const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
            const float ax = fx - x0, ay = fy - y0;
            for (int k = 0; k < 3; ++k) {
                const int c = std::min(k, channels - 1);
                float v = (at(x0, y0, c) * (1 - ax) + at(x0 + 1, y0, c) * ax) * (1 - ay) + (at(x0, y0 + 1, c) * (1 - ax) + at(x0 + 1, y0 + 1, c) * ax) * ay;
                v = std::clamp(v, 0.0f, 1.0f);
                rgba[((size_t)y * W + x) * 4 + k] = dataChannel ? SrgbToLinear(v) : v;
            }
        }
    if (s->tex->GetFormat() == gpu::Format::RGBA32F) {
        ctx->Upload(s->tex.get(), rgba.data(), W * 16);
    } else {
        std::vector<u16> px(rgba.size());
        for (size_t i = 0; i < rgba.size(); ++i) px[i] = ToHalf(rgba[i]);
        ctx->Upload(s->tex.get(), px.data(), W * 8);
    }
    d.CancelPendingClear(surface);
    d.TouchSurface(surface);
}

Id App::AddBakedLayer(DocView& v, PbrChannel c, const std::string& name, const std::vector<float>& src, int channels, int size) {
    Document& d = *v.doc;
    if (!(d.pbrChannels & (1u << (int)c)) || !FindChannelGroup(d, c)) {
        d.pbrChannels |= 1u << (int)c;
        CreatePbrChannels(v, true);
    }
    Id g = FindChannelGroup(d, c);
    if (!g) return 0;
    Edit e(d, v.history, "Carte cuite");
    // Replaces the previous bake of this channel.
    for (auto it = d.S().layers.begin(); it != d.S().layers.end();)
        if (it->parent == g && it->name == name) it = d.S().layers.erase(it);
        else ++it;
    Layer p;
    p.id = d.NewId();
    p.kind = LayerKind::Pixel;
    p.name = name;
    p.parent = g;
    p.surface = d.CreateSurface({0, 0, 0, 0});
    auto& L = d.S().layers;
    // Just above the channel's default value: the painted layers stay on top.
    int at = d.LayerIndex(g);
    for (int i = 0; i < (int)L.size(); ++i)
        if (L[i].parent == g && L[i].kind == LayerKind::Fill && L[i].name.find("(valeur)") != std::string::npos) { at = i + 1; break; }
    L.insert(L.begin() + std::min(at, (int)L.size()), p);
    d.SyncGraph();
    e.Commit();
    v.renderer.Prepare(d, *ctx);
    WriteLayerPixels(d, p.surface, src, channels, size, size, true);
    return p.id;
}

void App::StartBake(DocView& v) {
    auto& B = baker;
    if (B.running || !v.mesh) return;
    MeshData low = v.mesh->data;
    std::shared_ptr<MeshData> high;
    if (B.useHigh && !B.highPath.empty()) {
        high = std::make_shared<MeshData>();
        std::string err;
        if (!LoadMesh(B.highPath, *high, &err)) { Status("Modèle haute définition : " + err); return; }
        TransformToMeshSpace(*high, low);
    }
    B.view = &v;
    B.progress = std::make_shared<std::atomic<float>>(0.0f);
    B.cancel = std::make_shared<std::atomic<bool>>(false);
    B.revision = v.mesh->revision;
    B.running = true;
    B.startTime = ImGui::GetTime();
    BakeOptions o = B.opt;
    static const int sizes[] = {512, 1024, 2048, 4096};
    o.size = sizes[std::clamp(B.sizeIndex, 0, 3)];
    auto progress = B.progress;
    auto cancel = B.cancel;
    B.job = std::async(std::launch::async, [low = std::move(low), high, o, progress, cancel] {
        return BakeMeshes(low, high.get(), o, progress.get(), cancel.get());
    });
    Status(high ? "Cuisson depuis le modèle haute définition..." : "Cuisson des cartes du maillage...");
}

void App::PollBake() {
    auto& B = baker;
    if (!B.running || B.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    BakeResult r = B.job.get();
    B.running = false;
    bool alive = false;
    for (auto& d : docs) alive |= d.get() == B.view;
    if (!alive || !B.view->mesh) return;
    if (!r.ok) { Status("Cuisson : " + r.error); return; }
    DocView& v = *B.view;
    MeshScene& s = *v.mesh;
    const double secs = ImGui::GetTime() - B.startTime;
    // Smart masks follow the baked detail.
    s.maps = gpu::Dev().CreateTexture((u32)r.size, (u32)r.size, gpu::Format::RGBA32F, "MeshMaps");
    if (s.maps) {
        ctx->Upload(s.maps.get(), r.maps.data(), (u32)r.size * 16);
        s.mapsForRevision = B.revision;
        ++s.mapsRev;
    }
    int layers = 0;
    if (B.toChannels) {
        v.history.BeginGroup();
        if (!r.normal.empty() && B.useHigh && !B.highPath.empty() && AddBakedLayer(v, PbrChannel::Normal, "Normale cuite", r.normal, 3, r.size)) ++layers;
        if (!r.ao.empty() && AddBakedLayer(v, PbrChannel::Occlusion, "Occlusion cuite", r.ao, 1, r.size)) ++layers;
        if (B.heightToChannel && !r.height.empty() && AddBakedLayer(v, PbrChannel::Height, "Hauteur cuite", r.height, 1, r.size)) ++layers;
        if (!r.id.empty()) {
            // The ID map as a hidden reference layer (pick its colors with the magic wand / select color range).
            Document& d = *v.doc;
            Edit e(d, v.history, "ID cuit");
            for (auto it = d.S().layers.begin(); it != d.S().layers.end();)
                if (it->name == "ID cuit" && it->parent == 0) it = d.S().layers.erase(it);
                else ++it;
            Layer& l = d.AddPixelLayer("ID cuit");
            l.visible = false;
            const Id surf = l.surface;
            d.SyncGraph();
            e.Commit();
            v.renderer.Prepare(d, *ctx);
            WriteLayerPixels(d, surf, r.id, 3, r.size, r.size, false);
            ++layers;
        }
        v.history.EndGroup();
    }
    int files = 0;
    if (B.exportPng && !B.exportDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(Utf8Path(B.exportDir), ec);
        std::string stem = v.doc->path.empty() ? std::string("Bake") : Narrow(Utf8Path(v.doc->path).stem().wstring());
        auto write = [&](const std::vector<float>& img, int ch, const char* suffix) {
            if (img.empty()) return;
            std::vector<u8> px((size_t)r.size * r.size * 4, 255);
            for (size_t i = 0; i < (size_t)r.size * r.size; ++i)
                for (int k = 0; k < 3; ++k) px[i * 4 + k] = (u8)std::lround(std::clamp(img[i * ch + std::min(k, ch - 1)], 0.0f, 1.0f) * 255);
            std::string f = B.exportDir + "/" + stem + suffix;
            if (stbi_write_png(f.c_str(), r.size, r.size, 4, px.data(), r.size * 4)) ++files;
        };
        write(r.normal, 3, "_Normal.png");
        write(r.ao, 1, "_AO.png");
        write(r.curvature, 1, "_Curvature.png");
        write(r.thickness, 1, "_Thickness.png");
        write(r.height, 1, "_Height.png");
        write(r.position, 3, "_Position.png");
        write(r.worldNormal, 3, "_WorldNormal.png");
        write(r.id, 3, "_ID.png");
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "Cuisson terminée en %.1f s (%d px) : %d calque(s), %d fichier(s)%s", secs, r.size, layers, files,
             r.misses ? "" : "");
    std::string m = msg;
    if (r.misses) m += " - " + std::to_string((int)std::lround(100.0 * r.misses / std::max<size_t>(r.texels, 1))) + " % de texels sans modèle haute définition (augmentez la cage)";
    Status(m);
    Log("%s", m.c_str());
}

void App::DrawBakerWindow() {
    auto& B = baker;
    if (!B.open) return;
    DocView* v = Active();
    ImGui::SetNextWindowSize(ImVec2(520 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Cuisson des cartes (baker)", &B.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ui::FilterBox("Filtrer (Ctrl+F)...", 320.0f * dpi);
    if (!v || !v->mesh || v->doc->kind != 1) {
        ImGui::TextWrapped("Ouvrez un projet Paint 3D : le modèle du projet est le modèle basse définition (ses UV reçoivent les cartes).");
        ImGui::End();
        return;
    }
    const float R = Length(v->mesh->data.boundsMax - v->mesh->data.boundsMin) * 0.5f;
    ImGui::BeginDisabled(B.running);
    ImGui::SeparatorText("Modèles");
    ImGui::TextDisabled("Basse définition : %s (%zu triangles)", v->mesh->data.name.c_str(), v->mesh->data.indices.size() / 3);
    ImGui::Checkbox("Depuis un modèle haute définition", &B.useHigh);
    ui::HelpTooltip("Le détail du modèle haute définition (sculpture, biseaux...) est projeté sur les UV du modèle du projet :\nnormale, occlusion, courbure, hauteur. Les deux modèles doivent être superposés (mêmes unités et position).");
    if (B.useHigh) {
        ImGui::SetNextItemWidth(-60 * dpi);
        ImGui::InputTextWithHint("##high", "fichier OBJ / FBX / glTF", &B.highPath);
        ImGui::SameLine();
        if (ImGui::Button("...##high")) {
            std::string f = platform::OpenFileDialog(hwnd, L"Maillages (*.obj, *.fbx, *.gltf, *.glb)\0*.obj;*.fbx;*.gltf;*.glb\0");
            if (!f.empty()) B.highPath = f;
        }
        float front = B.opt.front / std::max(R, 1e-6f) * 100, back = B.opt.back / std::max(R, 1e-6f) * 100;
        ImGui::SetNextItemWidth(200 * dpi);
        if (ImGui::SliderFloat("Cage : au-dessus", &front, 0.1f, 30, "%.1f %% du modèle", ImGuiSliderFlags_Logarithmic)) B.opt.front = front / 100 * R;
        ImGui::SetNextItemWidth(200 * dpi);
        if (ImGui::SliderFloat("Cage : en dessous", &back, 0.1f, 30, "%.1f %% du modèle", ImGuiSliderFlags_Logarithmic)) B.opt.back = back / 100 * R;
        ui::HelpTooltip("Distance cherchée de part et d'autre de la surface basse définition (en % de la taille du modèle).\nTrop petite : des zones manquent ; trop grande : le détail d'autres parties peut être pris.");
    }
    ImGui::SeparatorText("Cartes");
    ImGui::BeginDisabled(!B.useHigh);
    ImGui::Checkbox("Normale (espace tangent)", &B.opt.normal);
    ImGui::EndDisabled();
    ImGui::SameLine(260 * dpi);
    ImGui::Checkbox("Occlusion ambiante", &B.opt.ao);
    ImGui::Checkbox("Courbure", &B.opt.curvature);
    ImGui::SameLine(260 * dpi);
    ImGui::Checkbox("Épaisseur", &B.opt.thickness);
    ImGui::Checkbox("Hauteur", &B.opt.height);
    ImGui::SameLine(260 * dpi);
    ImGui::Checkbox("Position", &B.opt.position);
    ImGui::Checkbox("Normale monde", &B.opt.worldNormal);
    ImGui::SameLine(260 * dpi);
    {
        bool idOn = B.opt.id >= 0;
        if (ImGui::Checkbox("ID couleur", &idOn)) B.opt.id = idOn ? std::max(B.idMode, 0) : -1;
        if (idOn) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130 * dpi);
            if (ImGui::Combo("##idmode", &B.idMode, "Pièces\0Matériaux\0Îles UV\0")) B.opt.id = B.idMode;
        }
        ui::HelpTooltip("Une couleur unie par pièce du modèle (haute définition s'il y en a un), par matériau ou par île UV :\npour sélectionner ou masquer ces zones ensuite (calque « ID cuit », export PNG).");
    }
    ImGui::SeparatorText("Qualité");
    ImGui::SetNextItemWidth(200 * dpi);
    ImGui::Combo("Résolution", &B.sizeIndex, "512\0" "1024\0" "2048\0" "4096\0");
    ImGui::SetNextItemWidth(200 * dpi);
    ImGui::SliderInt("Rayons d'occlusion", &B.opt.aoRays, 8, 256);
    float aod = B.opt.aoDistance / std::max(R, 1e-6f) * 100;
    ImGui::SetNextItemWidth(200 * dpi);
    if (ImGui::SliderFloat("Portée de l'occlusion", &aod, 1, 100, "%.0f %% du modèle")) B.opt.aoDistance = aod / 100 * R;
    float ew = B.opt.edgeWidth / std::max(R, 1e-6f) * 100;
    ImGui::SetNextItemWidth(200 * dpi);
    if (ImGui::SliderFloat("Largeur des arêtes", &ew, 0.5f, 20, "%.1f %% du modèle")) B.opt.edgeWidth = ew / 100 * R;
    ui::HelpTooltip("Portée de la courbure autour des arêtes vives (usure des arêtes des masques intelligents).");
    ImGui::SetNextItemWidth(200 * dpi);
    ImGui::SliderInt("Marge (texels)", &B.opt.padding, 2, 64);
    ui::HelpTooltip("Les cartes débordent des îles UV de ce nombre de texels (pas de joints visibles avec les mipmaps).");
    ImGui::SeparatorText("Résultat");
    ImGui::Checkbox("Dans les canaux du projet", &B.toChannels);
    ui::HelpTooltip("Calques « Normale cuite » (canal Normale, avec un modèle haute définition) et « Occlusion cuite » (canal Occlusion).\nLes masques intelligents utilisent toujours les cartes cuites.");
    if (B.toChannels) {
        ImGui::SameLine();
        ImGui::Checkbox("Hauteur dans le canal Hauteur", &B.heightToChannel);
    }
    ImGui::Checkbox("Exporter en PNG", &B.exportPng);
    if (B.exportPng) {
        ImGui::SetNextItemWidth(-60 * dpi);
        ImGui::InputTextWithHint("##bakedir", "dossier", &B.exportDir);
        ImGui::SameLine();
        if (ImGui::Button("...##bakedir")) {
            std::string f = platform::SaveFileDialog(hwnd, L"Dossier des cartes (nom au choix)\0*.png\0", L"png");
            if (!f.empty()) B.exportDir = Narrow(Utf8Path(f).parent_path().wstring());
        }
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    if (B.running) {
        const float p = B.progress ? B.progress->load() : 0;
        char ov[64];
        snprintf(ov, sizeof(ov), "%.0f %%  (%.0f s)", p * 100, ImGui::GetTime() - B.startTime);
        ImGui::ProgressBar(p, ImVec2(-90 * dpi, 0), ov);
        ImGui::SameLine();
        if (ImGui::Button("Annuler") && B.cancel) B.cancel->store(true);
    } else {
        ImGui::BeginDisabled(B.useHigh && B.highPath.empty());
        if (ImGui::Button("Cuire", ImVec2(140 * dpi, 0))) StartBake(*v);
        ImGui::EndDisabled();
    }
    ImGui::End();
}

}  // namespace sx
