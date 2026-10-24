// Resources panel: a file explorer for images and meshes (thumbnails made in the background,
// meshes rendered as small shaded previews), pinned folders and files ("gardés"), quick places,
// and a tab of lights, objects and scene presets to compose render scenes. Files are opened
// with a double click or dragged onto the canvas, the graph or the render view (same routes as a
// file dropped from Windows).
#include "ui/App.h"
#include "ui/Theme.h"
#include "io/UAsset.h"
#include "io/RawLoad.h"
#include "io/WicLoad.h"
#include "doc/DocFile.h"
#include "mesh/Mesh.h"
#include "ai/Net.h"

#include <ShlObj.h>
#include <shellapi.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <stb_image.h>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace sx {

namespace fs = std::filesystem;

namespace {

const char* kFilePayload = "SX_FILE";
const char* kLightPayload = "SX_SCENEITEM";

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

bool IsImageExt(const std::string& e) {
    if (IsRawExtension(e)) return true;  // camera RAW: embedded preview
    static const char* x[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".hdr", ".psd", ".tif", ".tiff", ".webp", ".sxl"};
    for (const char* k : x) if (e == k) return true;
    return false;
}
bool IsMeshExt(const std::string& e) {
    static const char* x[] = {".obj", ".fbx", ".gltf", ".glb", ".sx3d"};
    for (const char* k : x) if (e == k) return true;
    return false;
}

std::string KnownFolder(REFKNOWNFOLDERID id) {
    std::string out;
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = Narrow(p);
    if (p) CoTaskMemFree(p);
    return out;
}

// RGBA8 fitted in `size`, box filtered.
std::vector<u8> Fit(const u8* src, int w, int h, int size, int& ow, int& oh) {
    const float k = std::min(1.0f, (float)size / std::max(w, h));
    ow = std::max(1, (int)(w * k));
    oh = std::max(1, (int)(h * k));
    std::vector<u8> out((size_t)ow * oh * 4);
    for (int y = 0; y < oh; ++y)
        for (int x = 0; x < ow; ++x) {
            const int x0 = x * w / ow, x1 = std::max(x0 + 1, (x + 1) * w / ow), y0 = y * h / oh, y1 = std::max(y0 + 1, (y + 1) * h / oh);
            u32 acc[4] = {}, n = 0;
            for (int yy = y0; yy < y1; yy += std::max(1, (y1 - y0) / 4))
                for (int xx = x0; xx < x1; xx += std::max(1, (x1 - x0) / 4)) {
                    const u8* s = &src[((size_t)yy * w + xx) * 4];
                    for (int c = 0; c < 4; ++c) acc[c] += s[c];
                    ++n;
                }
            for (int c = 0; c < 4; ++c) out[((size_t)y * ow + x) * 4 + c] = (u8)(acc[c] / std::max(n, 1u));
        }
    return out;
}

// Small shaded render of a mesh (orthographic, z-buffer, one light).
std::vector<u8> RenderMeshThumb(const MeshData& m, int N) {
    std::vector<u8> px((size_t)N * N * 4, 0);
    if (m.vertices.empty() || m.indices.size() < 3) return px;
    float3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (auto& v : m.vertices) {
        mn = {std::min(mn.x, v.pos.x), std::min(mn.y, v.pos.y), std::min(mn.z, v.pos.z)};
        mx = {std::max(mx.x, v.pos.x), std::max(mx.y, v.pos.y), std::max(mx.z, v.pos.z)};
    }
    const float3 c = (mn + mx) * 0.5f;
    const float ext = std::max({mx.x - mn.x, mx.y - mn.y, mx.z - mn.z, 1e-6f});
    const float cy = std::cos(0.6f), sy = std::sin(0.6f), cp = std::cos(-0.35f), sp = std::sin(-0.35f);
    auto view = [&](float3 p) {
        p = (p - c) * (1.0f / ext);
        float3 r{p.x * cy - p.z * sy, p.y, p.x * sy + p.z * cy};
        return float3{r.x, r.y * cp - r.z * sp, r.y * sp + r.z * cp};
    };
    std::vector<float3> vp(m.vertices.size());
    for (size_t i = 0; i < vp.size(); ++i) vp[i] = view(m.vertices[i].pos);
    std::vector<float> z((size_t)N * N, 1e30f);
    const float3 L = Normalize(float3{-0.4f, 0.7f, -0.6f});
    const float s = N * 0.78f;
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        if (m.indices[t] >= vp.size() || m.indices[t + 1] >= vp.size() || m.indices[t + 2] >= vp.size()) continue;
        const float3 a = vp[m.indices[t]], b = vp[m.indices[t + 1]], d = vp[m.indices[t + 2]];
        float3 n = Cross(b - a, d - a);
        const float len = std::sqrt(Dot(n, n));
        if (len < 1e-12f) continue;
        n = n * (1.0f / len);
        const float shade = 0.28f + 0.72f * std::abs(Dot(n, L));
        auto sx = [&](const float3& p) { return float2{N * 0.5f + p.x * s, N * 0.5f - p.y * s}; };
        const float2 A = sx(a), B = sx(b), C = sx(d);
        const int x0 = std::max(0, (int)std::floor(std::min({A.x, B.x, C.x}))), x1 = std::min(N - 1, (int)std::ceil(std::max({A.x, B.x, C.x})));
        const int y0 = std::max(0, (int)std::floor(std::min({A.y, B.y, C.y}))), y1 = std::min(N - 1, (int)std::ceil(std::max({A.y, B.y, C.y})));
        const float area = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
        if (std::abs(area) < 1e-9f) continue;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float px0 = x + 0.5f, py0 = y + 0.5f;
                const float w0 = ((B.x - px0) * (C.y - py0) - (B.y - py0) * (C.x - px0)) / area;
                const float w1 = ((C.x - px0) * (A.y - py0) - (C.y - py0) * (A.x - px0)) / area;
                const float w2 = 1 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                const float depth = w0 * a.z + w1 * b.z + w2 * d.z;
                float& zz = z[(size_t)y * N + x];
                if (depth >= zz) continue;
                zz = depth;
                u8* o = &px[((size_t)y * N + x) * 4];
                o[0] = (u8)(std::min(1.0f, shade * 0.80f) * 255);
                o[1] = (u8)(std::min(1.0f, shade * 0.84f) * 255);
                o[2] = (u8)(std::min(1.0f, shade * 0.95f) * 255);
                o[3] = 255;
            }
    }
    return px;
}

struct ThumbResult {
    std::vector<u8> rgba;
    int w = 0, h = 0;
    std::string info;  // size of the image, triangles of the mesh
};

ThumbResult MakeThumb(const std::string& path, bool mesh) {
    ThumbResult r;
    const std::string ext = Lower(Narrow(Utf8Path(path).extension().wstring()));
    const int N = 160;
    if (mesh) {
        MeshData m;
        std::string err;
        bool ok = false;
        if (ext == ".uasset") {
            const std::string glb = UAssetMeshCached(path);
            ok = !glb.empty() && LoadMesh(glb, m, &err);
        } else if (ext == ".sx3d") {
            // Only the JSON at the start of the file ("SXL1" | u32 version | u64 size | json), not the pixels.
            Json meta;
            FILE* f = nullptr;
            if (_wfopen_s(&f, Widen(path).c_str(), L"rb") == 0 && f) {
                char magic[4] = {};
                u32 version = 0;
                u64 size = 0;
                if (fread(magic, 1, 4, f) == 4 && memcmp(magic, "SXL1", 4) == 0 && fread(&version, 4, 1, f) == 1 && fread(&size, 8, 1, f) == 1 &&
                    size > 0 && size < (1ull << 31)) {
                    std::string js((size_t)size, '\0');
                    if (fread(js.data(), 1, js.size(), f) == js.size()) ok = Json::Parse(js, meta) && meta.Has("mesh");
                }
                fclose(f);
            }
            if (ok) {
                // Same layout as App::MeshToJson: vertices and indices in base64.
                std::vector<u8> vb = net::Base64Decode(meta["mesh"]["vertices"].S("")), ib = net::Base64Decode(meta["mesh"]["indices"].S(""));
                ok = !vb.empty() && !ib.empty() && vb.size() % sizeof(MeshVertex) == 0 && ib.size() % sizeof(u32) == 0 &&
                     ib.size() / sizeof(u32) % 3 == 0;
                if (ok) {
                    m.vertices.resize(vb.size() / sizeof(MeshVertex));
                    memcpy(m.vertices.data(), vb.data(), vb.size());
                    m.indices.resize(ib.size() / sizeof(u32));
                    memcpy(m.indices.data(), ib.data(), ib.size());
                    // A damaged file must not make the render read outside the vertices.
                    for (u32 i : m.indices)
                        if (i >= m.vertices.size()) { ok = false; break; }
                }
            }
        } else {
            ok = LoadMesh(path, m, &err);
        }
        if (!ok) return r;
        r.rgba = RenderMeshThumb(m, N);
        r.w = r.h = N;
        r.info = std::to_string(m.indices.size() / 3) + " triangles";
        return r;
    }
    int w = 0, h = 0, comp = 0;
    std::vector<u8> src;
    if (ext == ".uasset") {
        u32 uw = 0, uh = 0;
        if (!LoadUAssetImage(path, src, uw, uh, nullptr)) return r;
        w = (int)uw;
        h = (int)uh;
    } else if (ext == ".sxl") {
        return r;  // documents: an icon
    } else if (IsRawExtension(ext)) {
        RawInfo info;
        if (!LoadRawThumb(path, src, w, h, &info)) return r;
        r.rgba = Fit(src.data(), w, h, N, r.w, r.h);
        r.info = std::to_string(info.width) + " x " + std::to_string(info.height) + (info.model.empty() ? "" : ", " + info.model);
        return r;
    } else {
        FILE* f = nullptr;
        if (_wfopen_s(&f, Widen(path).c_str(), L"rb") != 0 || !f) return r;
        if (ext == ".hdr") {
            if (float* hp = stbi_loadf_from_file(f, &w, &h, &comp, 4)) {
                src.resize((size_t)w * h * 4);
                for (size_t i = 0; i < src.size(); ++i) {
                    const float v = (i % 4 == 3) ? 1.0f : hp[i] / (1.0f + hp[i]);
                    src[i] = (u8)(std::pow(std::clamp(v, 0.0f, 1.0f), 1 / 2.2f) * 255);
                }
                stbi_image_free(hp);
            }
        } else if (stbi_uc* p = stbi_load_from_file(f, &w, &h, &comp, 4)) {
            src.assign(p, p + (size_t)w * h * 4);
            stbi_image_free(p);
        }
        fclose(f);
        if (src.empty()) return r;
    }
    r.rgba = Fit(src.data(), w, h, N, r.w, r.h);
    r.info = std::to_string(w) + " x " + std::to_string(h);
    return r;
}

}  // namespace

// ------------------------------------------------------------------ state

void App::LoadExplorerSettings() {
    auto& E = explorer;
    E.loaded = true;
    std::string txt;
    Json j;
    if (ReadTextFile(ExeDir() + "/explorer.json", txt) && Json::Parse(txt, j)) {
        for (auto& p : j["pins"].Elements()) E.pins.push_back(p.S(""));
        E.dirImages = j["images"].S("");
        E.dirMeshes = j["meshes"].S("");
        E.thumb = j["thumb"].F(96);
    }
    if (E.dirImages.empty()) E.dirImages = KnownFolder(FOLDERID_Pictures);
    if (E.dirMeshes.empty()) E.dirMeshes = KnownFolder(FOLDERID_Documents);
}

void App::SaveExplorerSettings() {
    auto& E = explorer;
    Json j = Json::Object();
    Json pins = Json::Array();
    for (auto& p : E.pins) pins.Push(p);
    j["pins"] = pins;
    j["images"] = E.dirImages;
    j["meshes"] = E.dirMeshes;
    j["thumb"] = E.thumb;
    std::ofstream(Utf8Path(ExeDir() + "/explorer.json"), std::ios::binary) << j.Dump(1);
}

gpu::Texture* App::ExplorerThumb(const std::string& path, bool mesh, std::string* info) {
    auto& E = explorer;
    // A finished job becomes the entry's texture (whether or not its path is still listed).
    auto finish = [&](ExplorerThumbEntry& t) {
        if (!t.job || t.job->wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        ExplorerThumbData d = t.job->get();
        t.job.reset();
        --E.running;
        t.info = d.info;
        if (!d.rgba.empty()) {
            t.tex = gpu::Dev().CreateTexture((u32)d.w, (u32)d.h, gpu::Format::RGBA8, "ExplorerThumb");
            ctx->Upload(t.tex.get(), d.rgba.data(), (u32)d.w * 4);
        }
        t.done = true;
    };
    auto it = E.thumbs.find(path);
    if (it == E.thumbs.end()) {
        if (E.running >= 4) {
            // Jobs of paths no longer shown (other folder, filter or tab) are only collected here:
            // poll them all so the slots they hold are freed.
            for (auto& [p, t] : E.thumbs) finish(t);
            if (E.running >= 4) return nullptr;  // a few at a time, the others next frames
        }
        auto& t = E.thumbs[path];
        t.job = std::make_shared<std::future<ExplorerThumbData>>(std::async(std::launch::async, [path, mesh]() {
            ThumbResult r = MakeThumb(path, mesh);
            return ExplorerThumbData{std::move(r.rgba), r.w, r.h, r.info};
        }));
        ++E.running;
        return nullptr;
    }
    auto& t = it->second;
    finish(t);
    if (info) *info = t.info;
    return t.tex.get();
}

// A light / object preset added to the render scene of the active 3D project.
void App::AddScenePreset(int kind) {
    DocView* v = Active();
    if (!v || v->doc->kind != 1 || !v->mesh) { Status("Ouvrez un projet Paint 3D (scène de rendu) pour y ajouter des lumières et objets"); return; }
    PtRenderState& r = RenderStateFor(*v);
    float floorY = 1e30f;
    for (auto& vx : v->mesh->data.vertices) floorY = std::min(floorY, vx.pos.y);
    const float3 target{0, floorY + 0.9f, 0};
    auto aim = [](PtLight& l, float3 to) {
        float3 d = Normalize(to - l.position);
        l.yaw = std::atan2(d.x, d.z) * 57.29578f;
        l.pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f)) * 57.29578f;
    };
    auto light = [&](PtLightType t, const char* name, float3 pos, float intensity, float size, float3 color = {1, 1, 1}, float spot = 40) {
        PtLight l;
        l.type = t;
        l.name = name;
        l.position = {pos.x, floorY + pos.y, pos.z};
        l.intensity = intensity;
        l.size = size;
        l.color = color;
        l.spotAngle = spot;
        aim(l, target);
        r.lights.push_back(l);
        r.selLight = (int)r.lights.size() - 1;
    };
    auto object = [&](PtObjectType t, const char* name, float3 pos, float scale, float3 color, float rough) {
        PtObject o;
        o.type = t;
        o.name = name;
        o.position = {pos.x, floorY + pos.y, pos.z};
        o.scale = scale;
        o.material.baseColor = color;
        o.material.roughness = rough;
        r.objects.push_back(o);
        r.selObject = (int)r.objects.size() - 1;
        r.selLight = -1;
    };
    switch (kind) {
        case 0: light(PtLightType::Area, "Boîte à lumière", {-1.8f, 2.4f, -1.6f}, 60, 1.4f); break;
        case 1: light(PtLightType::Area, "Débouchage doux", {2.2f, 1.6f, -1.8f}, 15, 2.0f, {0.85f, 0.9f, 1.0f}); break;
        case 2: light(PtLightType::Spot, "Contre-jour", {0.4f, 2.6f, 2.6f}, 140, 0.15f, {1, 0.92f, 0.82f}, 35); break;
        case 3: light(PtLightType::Spot, "Projecteur", {-2.0f, 2.4f, -2.0f}, 90, 0.1f, {1, 1, 1}, 25); break;
        case 4: light(PtLightType::Point, "Ampoule chaude", {1.2f, 1.6f, -1.0f}, 30, 0.1f, {1.0f, 0.75f, 0.45f}); break;
        case 5: light(PtLightType::Area, "Lumière du dessus", {0, 3.6f, 0}, 70, 1.6f); break;
        case 6: light(PtLightType::Sun, "Soleil", {3, 4, -2}, 4.5f, 1.0f, {1.0f, 0.93f, 0.82f}); break;
        case 7: light(PtLightType::Area, "Néon bleu", {-2.4f, 1.0f, 0.6f}, 35, 0.5f, {0.3f, 0.5f, 1.0f}); break;
        case 20: object(PtObjectType::Plane, "Sol", {0, 0, 0}, 6.0f, {0.62f, 0.62f, 0.64f}, 0.7f); break;
        case 21: object(PtObjectType::Backdrop, "Fond", {0, 0, 0.4f}, 1.2f, {0.62f, 0.62f, 0.64f}, 0.75f); break;
        case 22: object(PtObjectType::Cube, "Cube", {1.6f, 0.4f, 0.6f}, 0.4f, {0.8f, 0.8f, 0.8f}, 0.4f); break;
        case 23: object(PtObjectType::Sphere, "Sphère", {-1.6f, 0.4f, 0.6f}, 0.4f, {0.8f, 0.8f, 0.8f}, 0.3f); break;
        default: return;
    }
    v->doc->dirty = true;
    Status("Ajouté à la scène de rendu (panneau Scène de rendu pour le régler)");
}

// ------------------------------------------------------------------ panel

void App::DrawResourceBrowser() {
    auto& E = explorer;
    if (!E.loaded) LoadExplorerSettings();
    // First time: in the bottom zone, next to the graph.
    if (ImGuiWindow* g = ImGui::FindWindowByName("Graphe"); g && g->DockId) ImGui::SetNextWindowDockID(g->DockId, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Ressources", PanelFlag("Ressources"))) { ImGui::End(); return; }
    PopOutCorner("Ressources");
    DocView* v = Active();
    const float fs = ImGui::GetFontSize();

    // A file dragged from here and released over another window: routed like a file dropped from Windows.
    if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType(kFilePayload) && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        ImGuiWindow* over = GImGui->HoveredWindow;
        const bool self = over && (over == ImGui::GetCurrentWindow() || over->RootWindow == ImGui::GetCurrentWindow()->RootWindow);
        if (!self) pendingDrops.push_back({std::string((const char*)pl->Data), ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y});
    }
    if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType(kLightPayload) && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        ImGuiWindow* over = GImGui->HoveredWindow;
        if (over && over->RootWindow != ImGui::GetCurrentWindow()->RootWindow) AddScenePreset(*(const int*)pl->Data);
    }

    if (ImGui::BeginTabBar("##restabs")) {
        for (int tab = 0; tab < 3; ++tab) {
            const char* names[] = {"Images", "Maillages", "Lumières et objets"};
            if (!ImGui::BeginTabItem(names[tab], nullptr, explorerTabRequest == tab ? ImGuiTabItemFlags_SetSelected : 0)) continue;
            if (explorerTabRequest == tab) explorerTabRequest = -1;
            E.tab = tab;
            if (tab == 2) {
                // ---------------------------------------------------- lights, objects, scenes
                ImGui::TextDisabled("Clic ou glisser sur la vue Rendu : ajouté à la scène du projet 3D actif.");
                struct Item { int kind; const char* name; ImU32 color; int shape; };  // shape: 0 disc, 1 square, 2 cone, 3 sun
                static const Item lights[] = {{0, "Boîte à lumière", IM_COL32(255, 245, 225, 255), 1}, {1, "Débouchage doux", IM_COL32(210, 225, 255, 255), 1},
                                              {2, "Contre-jour", IM_COL32(255, 230, 200, 255), 2},       {3, "Projecteur", IM_COL32(255, 255, 255, 255), 2},
                                              {4, "Ampoule chaude", IM_COL32(255, 190, 110, 255), 0},  {5, "Lumière du dessus", IM_COL32(255, 255, 240, 255), 1},
                                              {6, "Soleil", IM_COL32(255, 220, 120, 255), 3},           {7, "Néon bleu", IM_COL32(90, 140, 255, 255), 1}};
                static const Item objects[] = {{20, "Sol", IM_COL32(170, 170, 175, 255), 1}, {21, "Fond cyclo", IM_COL32(170, 170, 175, 255), 1},
                                               {22, "Cube", IM_COL32(200, 200, 205, 255), 1}, {23, "Sphère", IM_COL32(200, 200, 205, 255), 0}};
                auto cards = [&](const Item* items, int n) {
                    const float cw = 92 * dpi, ch = 92 * dpi;
                    const int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + 6) / (cw + 6)));
                    for (int i = 0; i < n; ++i) {
                        if (i % cols) ImGui::SameLine(0, 6);
                        ImGui::PushID(items[i].kind);
                        const ImVec2 p = ImGui::GetCursorScreenPos();
                        if (ImGui::InvisibleButton("##it", ImVec2(cw, ch))) AddScenePreset(items[i].kind);
                        if (ImGui::BeginDragDropSource()) {
                            ImGui::SetDragDropPayload(kLightPayload, &items[i].kind, sizeof(int));
                            ImGui::TextUnformatted(items[i].name);
                            ImGui::EndDragDropSource();
                        }
                        const bool hot = ImGui::IsItemHovered();
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        dl->AddRectFilled(p, ImVec2(p.x + cw, p.y + ch), hot ? IM_COL32(48, 50, 62, 255) : IM_COL32(32, 33, 40, 255), 6);
                        const ImVec2 c(p.x + cw * 0.5f, p.y + ch * 0.4f);
                        const float rr = cw * 0.2f;
                        dl->AddCircleFilled(c, rr * 1.6f, (items[i].color & 0x00FFFFFF) | 0x22000000);
                        switch (items[i].shape) {
                            case 0: dl->AddCircleFilled(c, rr * 0.8f, items[i].color); break;
                            case 1: dl->AddRectFilled(ImVec2(c.x - rr, c.y - rr * 0.7f), ImVec2(c.x + rr, c.y + rr * 0.7f), items[i].color, 3); break;
                            case 2: dl->AddTriangleFilled(ImVec2(c.x, c.y - rr), ImVec2(c.x - rr, c.y + rr), ImVec2(c.x + rr, c.y + rr), items[i].color); break;
                            default:
                                dl->AddCircleFilled(c, rr * 0.6f, items[i].color);
                                for (int k = 0; k < 8; ++k) {
                                    const float a = k * 0.785f;
                                    dl->AddLine(ImVec2(c.x + std::cos(a) * rr * 0.8f, c.y + std::sin(a) * rr * 0.8f),
                                                ImVec2(c.x + std::cos(a) * rr * 1.2f, c.y + std::sin(a) * rr * 1.2f), items[i].color, 2);
                                }
                        }
                        const ImVec2 ts = ImGui::CalcTextSize(items[i].name);
                        dl->PushClipRect(p, ImVec2(p.x + cw, p.y + ch), true);
                        dl->AddText(ImVec2(p.x + std::max(4.0f, (cw - ts.x) * 0.5f), p.y + ch - fs * 1.4f), IM_COL32(220, 220, 228, 255), items[i].name);
                        dl->PopClipRect();
                        ImGui::PopID();
                    }
                };
                ImGui::SeparatorText("Lumières");
                cards(lights, (int)std::size(lights));
                ImGui::SeparatorText("Objets");
                cards(objects, (int)std::size(objects));
                ImGui::SeparatorText("Scènes complètes");
                if (scenePresets.empty() || ImGui::IsWindowAppearing()) ScanScenePresets();
                for (int i = 0; i < (int)scenePresets.size(); ++i) {
                    ImGui::PushID(i);
                    if (ImGui::Button(scenePresets[i].name.c_str())) {
                        if (v && v->doc->kind == 1 && v->mesh) LoadScene(*v, scenePresets[i].path, 0);
                        else Status("Ouvrez un projet Paint 3D pour appliquer une scène");
                    }
                    if (ImGui::IsItemHovered() && !scenePresets[i].desc.empty()) ImGui::SetTooltip("%s", scenePresets[i].desc.c_str());
                    ImGui::PopID();
                    if ((i + 1) % 3) ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::EndTabItem();
                continue;
            }
            // -------------------------------------------------------- file explorer (images / meshes)
            const bool meshes = tab == 1;
            std::string& dir = meshes ? E.dirMeshes : E.dirImages;
            const float side = 180 * dpi;
            ImGui::BeginChild("##places", ImVec2(side, 0), ImGuiChildFlags_Borders);
            auto place = [&](const std::string& label, const std::string& path) {
                if (path.empty()) return;
                if (ImGui::Selectable((label + "##" + path).c_str(), Lower(dir) == Lower(path))) dir = path;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
            };
            ImGui::TextDisabled("Gardés");
            bool anyPin = false;
            for (auto& p : E.pins) {
                std::error_code ec;
                const bool isDir = fs::is_directory(Utf8Path(p), ec);
                const std::string ext = Lower(Narrow(Utf8Path(p).extension().wstring()));
                if (!isDir && (meshes ? !IsMeshExt(ext) && ext != ".uasset" : !IsImageExt(ext) && ext != ".uasset")) continue;
                anyPin = true;
                const std::string name = Narrow(Utf8Path(p).filename().wstring());
                if (isDir) {
                    place("\xE2\x98\x85 " + name, p);
                } else if (ImGui::Selectable(("\xE2\x98\x85 " + name + "##pin" + p).c_str())) {
                    dir = Narrow(Utf8Path(p).parent_path().wstring());
                }
            }
            if (!anyPin) ImGui::TextDisabled("(étoile d'un fichier ou\n d'un dossier pour le garder)");
            ImGui::Separator();
            ImGui::TextDisabled("Raccourcis");
            place("Bureau", KnownFolder(FOLDERID_Desktop));
            place("Images", KnownFolder(FOLDERID_Pictures));
            place("Documents", KnownFolder(FOLDERID_Documents));
            place("Téléchargements", KnownFolder(FOLDERID_Downloads));
            {
                std::error_code ec;
                for (const char* rel : {"/Exemples", "/../../examples"})
                    if (fs::is_directory(Utf8Path(ExeDir() + rel), ec)) place("Exemples", Narrow(fs::weakly_canonical(Utf8Path(ExeDir() + rel), ec).wstring()));
            }
            const DWORD drives = GetLogicalDrives();
            for (int k = 0; k < 26; ++k)
                if (drives & (1u << k)) place(std::string(1, (char)('A' + k)) + ":", std::string(1, (char)('A' + k)) + ":\\");
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginGroup();
            // Path bar.
            if (ImGui::Button("\xE2\x86\x91")) {
                const fs::path parent = Utf8Path(dir).parent_path();
                if (!parent.empty() && parent != Utf8Path(dir)) dir = Narrow(parent.wstring());
            }
            ui::HelpTooltip("Dossier parent");
            ImGui::SameLine();
            const bool pinnedDir = std::find(E.pins.begin(), E.pins.end(), dir) != E.pins.end();
            if (ImGui::Button(pinnedDir ? "\xE2\x98\x85" : "\xE2\x98\x86")) {
                if (pinnedDir) E.pins.erase(std::remove(E.pins.begin(), E.pins.end(), dir), E.pins.end());
                else E.pins.push_back(dir);
                SaveExplorerSettings();
            }
            ui::HelpTooltip("Garder ce dossier (colonne de gauche)");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 260 * dpi));
            std::string edit = dir;
            if (ImGui::InputText("##path", &edit, ImGuiInputTextFlags_EnterReturnsTrue)) dir = edit;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130 * dpi);
            ImGui::InputTextWithHint("##filter", "Filtrer...", &E.filter);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x));
            if (ImGui::SliderFloat("##tsize", &E.thumb, 56, 200, "%.0f")) SaveExplorerSettings();
            // Listing (cached per folder, refreshed every 2 s).
            const std::string key = (meshes ? "M|" : "I|") + dir;
            if (key != E.listedKey || ImGui::GetTime() - E.listedTime > 2.0) {
                E.listedKey = key;
                E.listedTime = ImGui::GetTime();
                E.folders.clear();
                E.files.clear();
                std::error_code ec;
                for (auto& e : fs::directory_iterator(Utf8Path(dir), fs::directory_options::skip_permission_denied, ec)) {
                    const std::string name = Narrow(e.path().filename().wstring());
                    if (name.empty() || name[0] == '.' || name[0] == '$') continue;
                    if (e.is_directory(ec)) { E.folders.push_back(Narrow(e.path().wstring())); continue; }
                    const std::string ext = Lower(Narrow(e.path().extension().wstring()));
                    bool ok = meshes ? IsMeshExt(ext) : IsImageExt(ext);
                    if (ext == ".uasset") {
                        UAssetInfo ui;
                        ok = ReadUAssetInfo(Narrow(e.path().wstring()), ui) && (meshes ? ui.mesh : ui.texture);
                    }
                    if (ok) E.files.push_back(Narrow(e.path().wstring()));
                }
                auto byName = [](const std::string& a, const std::string& b) { return Lower(a) < Lower(b); };
                std::sort(E.folders.begin(), E.folders.end(), byName);
                std::sort(E.files.begin(), E.files.end(), byName);
            }
            ImGui::BeginChild("##files", ImVec2(0, 0), ImGuiChildFlags_Borders);
            const float cw = E.thumb * dpi, ch = cw + fs * 2.3f;
            const int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + 6) / (cw + 6)));
            int col = 0;
            const std::string q = Lower(E.filter);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            std::string navigate;
            auto card = [&](const std::string& path, bool folder) {
                const std::string name = Narrow(Utf8Path(path).filename().wstring());
                if (!q.empty() && Lower(name).find(q) == std::string::npos) return;
                if (col) ImGui::SameLine(0, 6);
                col = (col + 1) % cols;
                ImGui::PushID(path.c_str());
                const ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##f", ImVec2(cw, ch));
                const bool hot = ImGui::IsItemHovered();
                const bool dbl = hot && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                if (!folder && ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(kFilePayload, path.c_str(), path.size() + 1);
                    ImGui::TextUnformatted(name.c_str());
                    ImGui::TextDisabled(meshes ? "Vue Rendu : ajouté à la scène" : "Canevas / calques : objet dynamique  |  Graphe : nœud image");
                    ImGui::EndDragDropSource();
                }
                dl->AddRectFilled(p, ImVec2(p.x + cw, p.y + ch), hot ? IM_COL32(48, 50, 62, 255) : IM_COL32(30, 31, 38, 255), 6);
                std::string info;
                if (folder) {
                    // Folder icon.
                    const ImVec2 a(p.x + cw * 0.2f, p.y + cw * 0.3f), b(p.x + cw * 0.8f, p.y + cw * 0.75f);
                    dl->AddRectFilled(ImVec2(a.x, a.y - cw * 0.07f), ImVec2(a.x + cw * 0.25f, a.y + 2), IM_COL32(220, 180, 90, 255), 3);
                    dl->AddRectFilled(a, b, IM_COL32(240, 200, 100, 255), 4);
                    if (dbl) navigate = path;
                } else if (gpu::Texture* t = ExplorerThumb(path, meshes, &info)) {
                    const float k = std::min((cw - 8) / t->Width(), (cw - 8) / t->Height());
                    const float w = t->Width() * k, h = t->Height() * k;
                    const ImVec2 a(p.x + (cw - w) * 0.5f, p.y + 4 + (cw - 8 - h) * 0.5f);
                    // Checkerboard behind transparent images.
                    const float cs = 6 * dpi;
                    dl->PushClipRect(a, ImVec2(a.x + w, a.y + h), true);
                    for (float yy = a.y; yy < a.y + h; yy += cs)
                        for (float xx = a.x; xx < a.x + w; xx += cs)
                            dl->AddRectFilled(ImVec2(xx, yy), ImVec2(xx + cs, yy + cs),
                                              ((int)((xx - a.x) / cs) + (int)((yy - a.y) / cs)) % 2 ? IM_COL32(64, 64, 70, 255) : IM_COL32(48, 48, 54, 255));
                    dl->PopClipRect();
                    dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), a, ImVec2(a.x + w, a.y + h));
                } else {
                    const std::string ext = Narrow(Utf8Path(path).extension().wstring());
                    const ImVec2 ts = ImGui::CalcTextSize(ext.c_str());
                    dl->AddText(ImVec2(p.x + (cw - ts.x) * 0.5f, p.y + cw * 0.45f), ui::col::TextDim, ext.c_str());
                }
                // Keep (pin) star.
                if (!folder) {
                    const bool pinned = std::find(E.pins.begin(), E.pins.end(), path) != E.pins.end();
                    const ImVec2 sc(p.x + cw - fs * 0.9f, p.y + fs * 0.9f);
                    const bool starHot = ImGui::IsMouseHoveringRect(ImVec2(sc.x - fs * 0.7f, sc.y - fs * 0.7f), ImVec2(sc.x + fs * 0.7f, sc.y + fs * 0.7f));
                    dl->AddText(ImVec2(sc.x - fs * 0.45f, sc.y - fs * 0.55f), pinned ? IM_COL32(255, 200, 70, 255) : starHot ? IM_COL32_WHITE : IM_COL32(150, 150, 160, 160),
                                pinned ? "\xE2\x98\x85" : "\xE2\x98\x86");
                    if (starHot && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        if (pinned) E.pins.erase(std::remove(E.pins.begin(), E.pins.end(), path), E.pins.end());
                        else E.pins.push_back(path);
                        SaveExplorerSettings();
                    } else if (dbl) {
                        if (meshes && v && v->doc->kind == 1 && v->mesh) ImportIntoRender(*v, path);
                        else pendingDrop = path;  // opened like a file given to the app
                    }
                }
                const ImVec4 clip(p.x + 4, p.y, p.x + cw - 4, p.y + ch);
                dl->AddText(nullptr, 0, ImVec2(p.x + 5, p.y + cw), IM_COL32(225, 225, 232, 255), name.c_str(), nullptr, 0, &clip);
                if (!info.empty()) dl->AddText(nullptr, 0, ImVec2(p.x + 5, p.y + cw + fs), ui::col::TextDim, info.c_str(), nullptr, 0, &clip);
                if (hot && !folder) ImGui::SetTooltip("%s\n%s", path.c_str(), meshes ? "Double-clic : ajouter à la scène (projet 3D) ou ouvrir\nGlisser sur la vue Rendu"
                                                                                         : "Double-clic : ouvrir\nGlisser sur le canevas, les calques ou le graphe");
                if (!folder && ImGui::BeginPopupContextItem("##ctx")) {
                    if (ImGui::MenuItem("Ouvrir")) pendingDrop = path;
                    if (meshes) {
                        if (ImGui::MenuItem("Ajouter à la scène de rendu", nullptr, false, v && v->doc->kind == 1 && v->mesh)) ImportIntoRender(*v, path);
                    } else if (v) {
                        if (ImGui::MenuItem("Placer comme calque (objet dynamique)", nullptr, false, v->doc->Mode() == DocMode::Layers)) PlaceSmartFile(*v, path, false, true);
                        if (ImGui::MenuItem("Placer lié au fichier", nullptr, false, v->doc->Mode() == DocMode::Layers)) PlaceSmartFile(*v, path, true, true);
                        if (ImGui::MenuItem("Ajouter au graphe (nœud image)", nullptr, false, v->doc->Mode() == DocMode::Nodes)) AddImageNode(*v, path);
                    }
                    if (ImGui::MenuItem("Afficher dans l'Explorateur Windows"))
                        ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + Widen(path) + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            };
            for (auto& f : E.folders) card(f, true);
            for (auto& f : E.files) card(f, false);
            if (E.folders.empty() && E.files.empty()) ImGui::TextDisabled(meshes ? "Aucun maillage ici (.obj, .fbx, .gltf, .glb, .sx3d, .uasset)." : "Aucune image ici.");
            ImGui::EndChild();
            ImGui::EndGroup();
            if (!navigate.empty()) dir = navigate;
            if (dir != E.savedDir[tab]) {
                E.savedDir[tab] = dir;
                SaveExplorerSettings();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

}  // namespace sx
