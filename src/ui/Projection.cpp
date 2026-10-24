// Projected materials (Paint 3D, Substance-like triplanar fill): an image (file or
// a pattern of the library) projected on the model along the three axes, written
// into the material's channel layers (color; roughness, height, metal from the
// image's luminance or constant). The material keeps a shared mask, so smart masks
// and painting still decide where it shows. Settings live on the layers (JSON) and
// the layers are regenerated when they change (undo included).
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

#include <imgui_stdlib.h>
#include <stb_image.h>
#include <cstdio>
#include <functional>

namespace sx {

namespace {

const PbrChannel kProjChannels[] = {PbrChannel::BaseColor, PbrChannel::Roughness, PbrChannel::Metallic, PbrChannel::Height};

Json DefaultProjection(const std::string& source) {
    Json p = Json::Object();
    p["source"] = source;
    p["scale"] = 1.5f;
    p["rotation"] = 0.0f;
    p["sharpness"] = 6.0f;
    p["offsetX"] = 0.0f;
    p["offsetY"] = 0.0f;
    p["roughFromImage"] = true;
    p["roughLo"] = 0.35f;
    p["roughHi"] = 0.85f;
    p["rough"] = 0.6f;
    p["roughInvert"] = true;  // dark grooves rough, light polished
    p["heightFromImage"] = true;
    p["heightLo"] = 0.45f;
    p["heightHi"] = 0.55f;
    p["contrast"] = 1.0f;
    p["metal"] = 0.0f;
    return p;
}

}  // namespace

gpu::Texture* App::ProjectionImage(const std::string& source) {
    auto it = projImages.find(source);
    if (it != projImages.end()) return it->second.get();
    gpu::TexturePtr tex;
    if (source.rfind("pattern:", 0) == 0) {
        // A pattern of the library (Motifs panel).
        const std::string name = source.substr(8);
        for (auto& p : patterns)
            if (p.name == name && !p.rgba.empty()) {
                tex = gpu::Dev().CreateTexture((u32)p.w, (u32)p.h, gpu::Format::RGBA8, "ProjectionImage");
                if (tex) ctx->Upload(tex.get(), p.rgba.data(), (u32)p.w * 4);
            }
    } else {
        int w = 0, h = 0, c = 0;
        FILE* f = nullptr;
        stbi_uc* px = nullptr;
        if (_wfopen_s(&f, Widen(source).c_str(), L"rb") == 0 && f) {
            px = stbi_load_from_file(f, &w, &h, &c, 4);
            fclose(f);
        }
        if (px) {
            tex = gpu::Dev().CreateTexture((u32)w, (u32)h, gpu::Format::RGBA8, "ProjectionImage");
            if (tex) ctx->Upload(tex.get(), px, (u32)w * 4);
            stbi_image_free(px);
        }
    }
    projImages[source] = tex;  // null too: not retried every frame
    return tex.get();
}

Id App::AddProjectedMaterial(DocView& v, const std::string& source, const std::string& name) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) return 0;
    for (PbrChannel c : kProjChannels)
        if (!FindChannelGroup(d, c)) {
            d.pbrChannels |= 1u << (int)c;
            CreatePbrChannels(v, true);
        }
    Edit e(d, v.history, "Matériau projeté");
    const Id material = d.NewId();
    const Id mask = d.CreateSurface({1, 1, 1, 1});  // everywhere until masked
    const std::string text = DefaultProjection(source).Dump(0);
    for (PbrChannel c : kProjChannels) {
        Id g = FindChannelGroup(d, c);
        if (!g) continue;
        Layer p;
        p.id = d.NewId();
        p.kind = LayerKind::Pixel;
        p.name = name;
        p.parent = g;
        p.material = material;
        p.mask = mask;
        p.surface = d.CreateSurface({0, 0, 0, 0});
        p.projection = text;
        auto& L = d.S().layers;
        int gi = d.LayerIndex(g);
        L.insert(L.begin() + gi, p);
    }
    if (Layer* l = AnyMaterialLayer(d, material)) d.S().activeLayer = l->id;
    d.S().editMask = true;
    d.SyncGraph();
    e.Commit();
    materialSel = material;
    Status("Matériau projeté « " + name + " » : réglez l'échelle et la rotation (Calques 3D) ; son masque se peint ou se génère");
    return material;
}

void App::UpdateProjections(DocView& v) {
    if (!v.mesh || v.doc->kind != 1 || !v.mesh->posMap) return;
    MeshScene& s = *v.mesh;
    Document& d = *v.doc;
    for (auto& l : d.S().layers) {
        if (l.projection.empty() || !l.surface || l.kind != LayerKind::Pixel) continue;
        // Surface ids are reused (after an undo, in other documents): the key mixes in the document and the
        // signature names the document and the surface's texture, so a new surface under an old id is projected again.
        Surface* sf = d.GetSurface(l.surface);
        if (!sf || !sf->tex) continue;
        const Id key = (Id)(l.surface ^ (Id)(std::hash<const void*>{}(&d) * 2654435761ull));
        char who[64];
        snprintf(who, sizeof(who), "%p|%p|", (const void*)&d, (const void*)sf->tex.get());
        const std::string sig = who + l.projection + "|" + std::to_string(s.revision) + "|" + std::to_string(s.posMap->Width());
        auto it = projectionDone.find(key);
        if (it != projectionDone.end() && it->second == sig) continue;
        Json p;
        if (!Json::Parse(l.projection, p)) continue;
        gpu::Texture* img = ProjectionImage(p["source"].S());
        if (!img) {
            projectionDone[key] = sig;
            Status("Matériau projeté : image introuvable " + p["source"].S());
            continue;
        }
        // Which channel this layer is: its group.
        PbrChannel ch = PbrChannel::BaseColor;
        for (PbrChannel c : kProjChannels)
            if (FindChannelGroup(d, c) == l.parent) ch = c;
        Renderer::MeshProjectParams q;
        q.scale = p["scale"].F(1.5f);
        q.rotation = p["rotation"].F(0) * 0.0174533f;
        q.sharpness = p["sharpness"].F(6);
        q.offsetX = p["offsetX"].F(0);
        q.offsetY = p["offsetY"].F(0);
        q.contrast = p["contrast"].F(1);
        if (ch == PbrChannel::BaseColor) {
            q.mode = 0;
        } else if (ch == PbrChannel::Roughness) {
            q.mode = 1;
            const bool img2 = p["roughFromImage"].B(true);
            q.lo = img2 ? p["roughLo"].F(0.35f) : p["rough"].F(0.6f);
            q.hi = img2 ? p["roughHi"].F(0.85f) : q.lo;
            q.invert = p["roughInvert"].B(true);
        } else if (ch == PbrChannel::Height) {
            q.mode = 1;
            const bool img2 = p["heightFromImage"].B(true);
            q.lo = img2 ? p["heightLo"].F(0.45f) : 0.5f;
            q.hi = img2 ? p["heightHi"].F(0.55f) : 0.5f;
        } else {
            q.mode = 1;
            q.lo = q.hi = p["metal"].F(0);
        }
        if (v.renderer.MeshProject(d, *ctx, s, l.surface, img, q)) projectionDone[key] = sig;
    }
}

void App::DrawProjectionEditor(DocView& v, Id material) {
    Document& d = *v.doc;
    Layer* any = nullptr;
    for (auto& l : d.S().layers)
        if (l.material == material && !l.projection.empty()) { any = &l; break; }
    if (!any) return;
    Json p;
    if (!Json::Parse(any->projection, p)) return;
    const Json& cp = p;
    bool changed = false;
    ImGui::SeparatorText("Projection (triplanaire)");
    std::string src = cp["source"].S();
    const std::string shown = src.rfind("pattern:", 0) == 0 ? "Motif : " + src.substr(8) : Narrow(Utf8Path(src).filename().wstring());
    ImGui::TextDisabled("%s", shown.c_str());
    if (ImGui::SmallButton("Image...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0");
        if (!f.empty()) { p["source"] = f; changed = true; }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Motif...")) ImGui::OpenPopup("##projpattern");
    if (ImGui::BeginPopup("##projpattern")) {
        if (!patternsScanned) ScanPatterns();
        for (auto& pt : patterns)
            if (ImGui::Selectable(pt.name.c_str())) { p["source"] = "pattern:" + pt.name; changed = true; }
        if (patterns.empty()) ImGui::TextDisabled("Aucun motif (panneau Motifs)");
        ImGui::EndPopup();
    }
    auto slider = [&](const char* label, const char* key, float def, float lo, float hi, const char* fmt, ImGuiSliderFlags fl = 0) {
        float x = cp[key].F(def);
        ImGui::SetNextItemWidth(-110 * dpi);
        if (ImGui::SliderFloat(label, &x, lo, hi, fmt, fl)) { p[key] = x; changed = true; }
    };
    slider("Échelle", "scale", 1.5f, 0.1f, 40, "%.2f", ImGuiSliderFlags_Logarithmic);
    slider("Rotation", "rotation", 0, -180, 180, "%.0f°");
    slider("Netteté des raccords", "sharpness", 6, 1, 32, "%.1f");
    slider("Décalage X", "offsetX", 0, 0, 1, "%.2f");
    slider("Décalage Y", "offsetY", 0, 0, 1, "%.2f");
    bool rImg = cp["roughFromImage"].B(true);
    if (ImGui::Checkbox("Rugosité depuis l'image", &rImg)) { p["roughFromImage"] = rImg; changed = true; }
    if (rImg) {
        slider("Rugosité min", "roughLo", 0.35f, 0, 1, "%.2f");
        slider("Rugosité max", "roughHi", 0.85f, 0, 1, "%.2f");
        bool inv = cp["roughInvert"].B(true);
        if (ImGui::Checkbox("Clair = lisse", &inv)) { p["roughInvert"] = inv; changed = true; }
    } else {
        slider("Rugosité", "rough", 0.6f, 0, 1, "%.2f");
    }
    bool hImg = cp["heightFromImage"].B(true);
    if (ImGui::Checkbox("Relief depuis l'image", &hImg)) { p["heightFromImage"] = hImg; changed = true; }
    if (hImg) {
        slider("Creux", "heightLo", 0.45f, 0, 0.5f, "%.2f");
        slider("Bosses", "heightHi", 0.55f, 0.5f, 1, "%.2f");
    }
    slider("Contraste", "contrast", 1, 0.2f, 4, "%.2f");
    slider("Métal", "metal", 0, 0, 1, "%.2f");
    if (changed) {
        const std::string text = cp.Dump(0);
        Edit e(d, v.history, "Projection du matériau");
        for (auto& l : d.S().layers)
            if (l.material == material && !l.projection.empty()) l.projection = text;
        e.Commit();
    }
}

}  // namespace sx
