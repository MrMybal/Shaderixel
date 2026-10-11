// Smart masks and smart materials (Paint 3D, Substance-like): mesh maps baked
// on the CPU (AO, convexity, concavity, thickness), masks generated on the GPU
// from them (edge wear, cavity dirt, dust, gradients, noise), and multi-layer
// materials whose masks follow the shape of the model.
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "mesh/MeshBake.h"

#include <imgui_stdlib.h>
#include <stb_image.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sx {

namespace {

const char* kGenTypes = "Aucun (masque peint)\0Usure des arêtes\0Saleté des creux\0Poussière (dessus)\0Dégradé\0Occlusion\0Bruit\0Parties fines\0Plein\0Suit un calque\0";
// Types that read the baked mesh maps.
bool NeedsMaps(int type) { return type == 0 || type == 1 || type == 4 || type == 6; }

float3 DirOf(int k) {
    static const float3 d[] = {{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
    return d[std::clamp(k, 0, 5)];
}

Renderer::MeshGenParams GenFromJson(const Json& g) {
    Renderer::MeshGenParams p;
    p.type = g["type"].I(0);
    p.invert = g["invert"].B();
    p.amount = g["amount"].F(0.5f);
    p.contrast = g["contrast"].F(4);
    p.noise = g["noise"].F(0.3f);
    p.noiseScale = g["scale"].F(6);
    p.seed = g["seed"].F(0);
    { float3 dd = DirOf(g["dir"].I(0)); p.dir = {dd.x, dd.y, dd.z, 0}; }
    p.anchorMode = g["anchor"].I(0);
    p.anchorRadius = g["radius"].F(8);
    return p;
}

Json Gen(int type, float amount, float contrast, float noise, float scale, bool invert = false, int dir = 0) {
    Json g = Json::Object();
    g["type"] = type;
    g["amount"] = amount;
    g["contrast"] = contrast;
    g["noise"] = noise;
    g["scale"] = scale;
    g["invert"] = invert;
    g["dir"] = dir;
    return g;
}

Json Lay(const char* name, float3 color, float rough, float metal, const Json& mask, float height = -1) {
    Json l = Json::Object();
    l["name"] = name;
    Json c = Json::Array();
    c.Push(color.x); c.Push(color.y); c.Push(color.z);
    l["color"] = c;
    l["rough"] = rough;
    l["metal"] = metal;
    if (height >= 0) l["height"] = height;
    l["mask"] = mask;
    return l;
}

Json Smart(const char* name, const char* desc, std::initializer_list<Json> layers) {
    Json s = Json::Object();
    s["name"] = name;
    s["description"] = desc;
    Json a = Json::Array();
    for (auto& l : layers) a.Push(l);
    s["layers"] = a;
    return s;
}

}  // namespace

// ------------------------------------------------------------------ mesh maps

void App::StartMeshMapsBake(DocView& v) {
    if (!v.mesh || mapsBake.running) return;
    auto& B = mapsBake;
    B.view = &v;
    B.progress = std::make_shared<std::atomic<float>>(0.0f);
    B.cancel = std::make_shared<std::atomic<bool>>(false);
    B.revision = v.mesh->revision;
    B.running = true;
    MeshMapsOptions o;
    o.size = std::min<int>(mapsResolution, (int)v.doc->Width());
    MeshData mesh = v.mesh->data;  // copy: the job runs on another thread
    auto progress = B.progress;
    auto cancel = B.cancel;
    B.job = std::async(std::launch::async, [mesh = std::move(mesh), o, progress, cancel] { return BakeMeshMaps(mesh, o, progress.get(), cancel.get()); });
    Status("Cuisson des cartes du maillage (occlusion, courbure, épaisseur)...");
}

void App::PollMeshMapsBake() {
    auto& B = mapsBake;
    if (!B.running || B.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    MeshMaps r = B.job.get();
    B.running = false;
    bool alive = false;
    for (auto& d : docs) alive |= d.get() == B.view;
    if (!alive || !B.view->mesh) return;
    if (!r.ok) { Status("Cartes du maillage : " + r.error); return; }
    MeshScene& s = *B.view->mesh;
    s.maps = gpu::Dev().CreateTexture((u32)r.size, (u32)r.size, gpu::Format::RGBA32F, "MeshMaps");
    if (!s.maps) return;
    ctx->Upload(s.maps.get(), r.rgba.data(), (u32)r.size * 16);
    s.mapsForRevision = B.revision;
    ++s.mapsRev;
    Status("Cartes du maillage cuites (" + std::to_string(r.size) + " px) : les masques intelligents sont à jour");
}

void App::UpdateSmartMasks(DocView& v) {
    if (!v.mesh || v.doc->kind != 1) return;
    MeshScene& s = *v.mesh;
    if (!s.posMap) return;
    if (s.maps && s.mapsForRevision != s.revision) s.maps.reset();  // the geometry or the UVs changed
    Document& d = *v.doc;
    std::set<Id> seen;
    for (auto& l : d.S().layers) {
        if (!l.material || l.generator.empty() || !l.mask || seen.count(l.material)) continue;
        seen.insert(l.material);
        Json g;
        if (!Json::Parse(l.generator, g)) continue;
        Renderer::MeshGenParams p = GenFromJson(g);
        if (!l.maskEdit) {
            // Corrections painted over the smart mask: their own surface, kept when the mask is generated again.
            const Id e = d.CreateSurface({0, 0, 0, 0});
            for (auto& x : d.S().layers) if (x.material == l.material) x.maskEdit = e;
        }
        if (NeedsMaps(p.type) && !s.maps) {
            if (!mapsBake.running) StartMeshMapsBake(v);
            continue;
        }
        std::string sig = l.generator + "|" + std::to_string(s.mapsRev) + "|" + std::to_string(s.revision) + "|" + std::to_string(s.posMap->Width());
        if (p.type == 8) {
            // Follows another 3D layer: regenerated when its pixels change.
            const Id src = (Id)g["source"].I(0);
            Surface* ss = nullptr;
            if (src && src != l.material) {
                if (IsPaintLayer3D(d, src)) {
                    Layer* bc = MaterialChannelLayer(d, src, PbrChannel::BaseColor);
                    if (!bc) bc = AnyMaterialLayer(d, src);
                    if (bc && bc->surface) ss = d.GetSurface(bc->surface);
                    p.sourceAlpha = true;
                } else if (Layer* a = AnyMaterialLayer(d, src); a && a->mask) {
                    ss = d.GetSurface(a->mask);
                }
            }
            if (!ss) continue;
            p.source = ss->tex.get();
            sig += "|" + std::to_string(src) + ":" + std::to_string(ss->version);
        }
        if (Surface* es = l.maskEdit ? d.GetSurface(l.maskEdit) : nullptr) {
            p.edits = es->tex.get();
            sig += "|e" + std::to_string(es->version);
        }
        auto it = smartMaskDone.find(l.mask);
        if (it != smartMaskDone.end() && it->second == sig) continue;
        if (v.renderer.MeshGenerate(d, *ctx, s, l.mask, p)) smartMaskDone[l.mask] = sig;
    }
}

void App::DrawSmartMaskEditor(DocView& v, Id material) {
    Document& d = *v.doc;
    Layer* any = AnyMaterialLayer(d, material);
    if (!any) return;
    Json g;
    if (!any->generator.empty()) Json::Parse(any->generator, g);
    if (!g.IsObject()) g = Json::Object();
    const Json& cg = g;
    ImGui::SeparatorText("Masque intelligent");
    int sel = any->generator.empty() ? 0 : cg["type"].I(0) + 1;
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##gentype", &sel, kGenTypes)) {
        if (sel == 0) g = Json::Object();
        else {
            const int t = sel - 1;
            // Sensible starting values per kind.
            static const float amounts[] = {0.25f, 0.4f, 0.35f, 0.5f, 0.5f, 0.5f, 0.3f, 1.0f, 0.5f};
            if (!g.Has("amount") || cg["type"].I(-1) != t) g = Gen(t, amounts[t], 5, t == 5 ? 0.0f : 0.3f, 6);
            g["type"] = t;
        }
        changed = true;
    }
    ui::HelpTooltip("Le masque du matériau est calculé à partir de la forme du modèle.\nPeindre le masque reste possible : il est recalculé quand un réglage change.");
    if (sel > 0) {
        const int t = sel - 1;
        auto slider = [&](const char* label, const char* key, float def, float lo, float hi, const char* fmt) {
            float x = cg[key].F(def);
            ImGui::SetNextItemWidth(-90 * dpi);
            if (ImGui::SliderFloat(label, &x, lo, hi, fmt)) { g[key] = x; changed = true; }
        };
        slider("Couverture", "amount", 0.5f, 0, 1, "%.2f");
        slider("Contraste", "contrast", 4, 0.5f, 20, "%.1f");
        slider("Bruit", "noise", 0.3f, 0, 1, "%.2f");
        slider("Échelle", "scale", 6, 0.5f, 40, "%.1f");
        slider("Graine", "seed", 0, 0, 100, "%.0f");
        if (t == 8) {
            // Layer followed: another 3D layer (paint or material with a mask).
            const Id src = (Id)cg["source"].I(0);
            std::string cur = "(choisir)";
            if (Layer* a = src ? AnyMaterialLayer(d, src) : nullptr) cur = a->name;
            ImGui::SetNextItemWidth(-90 * dpi);
            if (ImGui::BeginCombo("Calque suivi", cur.c_str())) {
                for (Id m : MaterialIds(d)) {
                    if (m == material) continue;
                    Layer* a = AnyMaterialLayer(d, m);
                    if (!a || (!IsPaintLayer3D(d, m) && !a->mask)) continue;
                    if (ImGui::Selectable(a->name.c_str(), m == src)) { g["source"] = (int)m; changed = true; }
                }
                ImGui::EndCombo();
            }
            ui::HelpTooltip("Le masque suit ce que contient ce calque (sa peinture ou son masque) : il se met à jour quand on y peint.");
            int mode = cg["anchor"].I(0);
            ImGui::SetNextItemWidth(-90 * dpi);
            if (ImGui::Combo("Placement", &mode, "Dessus\0Bord\0Autour\0Ailleurs\0")) { g["anchor"] = mode; changed = true; }
            ui::HelpTooltip("Dessus : sur la peinture du calque suivi.  Bord : une bande à sa limite (usure, écaillage).\nAutour : un halo juste à côté (rouille autour d'une rayure).  Ailleurs : partout sauf dessus.");
            slider("Rayon", "radius", 8, 0, 64, "%.0f px");
        }
        if (t == 2 || t == 3) {
            int dir = cg["dir"].I(0);
            ImGui::SetNextItemWidth(-90 * dpi);
            if (ImGui::Combo("Direction", &dir, "Haut (+Y)\0Bas (-Y)\0+X\0-X\0+Z\0-Z\0")) { g["dir"] = dir; changed = true; }
        }
        bool inv = cg["invert"].B();
        if (ImGui::Checkbox("Inverser", &inv)) { g["invert"] = inv; changed = true; }
        if (NeedsMaps(t)) {
            ImGui::SameLine();
            if (mapsBake.running && mapsBake.view == &v) {
                ImGui::TextDisabled("cuisson %.0f %%", mapsBake.progress->load() * 100);
            } else if (v.mesh && v.mesh->maps) {
                if (ImGui::SmallButton("Recuire les cartes")) StartMeshMapsBake(v);
                ui::HelpTooltip("Occlusion, courbure et épaisseur du modèle, recalculées (après un changement d'UV).");
            } else {
                ImGui::TextDisabled("cartes du maillage à cuire...");
            }
        }
    }
    if (changed) {
        const std::string text = sel == 0 ? "" : cg.Dump(0);
        Edit e(d, v.history, "Masque intelligent");
        for (auto& l : d.S().layers) if (l.material == material) l.generator = text;
        e.Commit();
    }
}

// ------------------------------------------------------------------ smart materials

const std::vector<Json>& App::SmartMaterials() {
    static std::vector<Json> list;
    static bool loaded = false;
    if (loaded) return list;
    loaded = true;
    // Mask types: 0 edge wear, 1 cavity dirt, 2 dust, 3 gradient, 4 occlusion, 5 noise, 6 thin parts, 7 full, 8 follows a layer.
    const Json full = Gen(7, 1, 4, 0, 6);
    auto follow = [](int rel, int mode, float radius, float amount, float noise) {
        Json g = Gen(8, amount, 4, noise, 14);
        g["follow"] = rel;
        g["anchor"] = mode;
        g["radius"] = radius;
        return g;
    };
    list.push_back(Smart("Métal peint usé", "Peinture écaillée sur les arêtes, métal apparent, saleté dans les creux.",
                         {Lay("Métal", {0.56f, 0.57f, 0.58f}, 0.45f, 1, full), Lay("Peinture", {0.62f, 0.09f, 0.07f}, 0.38f, 0, Gen(0, 0.22f, 7, 0.45f, 9, true)),
                          Lay("Saleté", {0.17f, 0.13f, 0.09f}, 0.95f, 0, Gen(1, 0.35f, 3, 0.35f, 7))}));
    list.push_back(Smart("Acier peint écaillé", "Peinture bleue écaillée sur les arêtes, rouille qui s'insinue au bord des éclats, saleté dans les creux.",
                         {Lay("Acier", {0.55f, 0.56f, 0.57f}, 0.4f, 1, full), Lay("Peinture", {0.08f, 0.2f, 0.42f}, 0.42f, 0, Gen(0, 0.25f, 8, 0.5f, 10, true)),
                          Lay("Rouille au bord des éclats", {0.4f, 0.17f, 0.05f}, 0.85f, 0.1f, follow(-1, 1, 6, 0.55f, 0.35f), 0.5f),
                          Lay("Saleté", {0.15f, 0.12f, 0.08f}, 0.95f, 0, Gen(1, 0.3f, 3, 0.35f, 7))}));
    list.push_back(Smart("Cuir patiné", "Cuir brun, assombri et lustré dans les creux, éclairci et mat sur les arêtes usées.",
                         {Lay("Cuir", {0.24f, 0.12f, 0.06f}, 0.6f, 0, Gen(7, 1, 4, 0, 6)), Lay("Patine des creux", {0.1f, 0.05f, 0.025f}, 0.4f, 0, Gen(1, 0.45f, 2, 0.4f, 9), 0.45f),
                          Lay("Usure claire", {0.42f, 0.27f, 0.15f}, 0.8f, 0, Gen(0, 0.3f, 3, 0.5f, 12))}));
    list.push_back(Smart("Béton taché", "Béton gris, coulures sombres vers le bas, mousse claire sur le dessus, salissures dans les creux.",
                         {Lay("Béton", {0.42f, 0.41f, 0.39f}, 0.92f, 0, Gen(7, 1, 4, 0, 6), 0.5f), Lay("Coulures", {0.2f, 0.19f, 0.17f}, 0.85f, 0, Gen(3, 0.4f, 2, 0.6f, 4, false, 1)),
                          Lay("Dépôt clair", {0.6f, 0.6f, 0.55f}, 0.95f, 0, Gen(2, 0.25f, 4, 0.5f, 8)), Lay("Saleté", {0.13f, 0.12f, 0.1f}, 0.95f, 0, Gen(1, 0.35f, 3, 0.3f, 7))}));
    list.push_back(Smart("Céramique ébréchée", "Émail blanc brillant, éclats sur les arêtes qui montrent la terre cuite, bordés d'une fine ligne sombre.",
                         {Lay("Terre cuite", {0.55f, 0.3f, 0.18f}, 0.85f, 0, Gen(7, 1, 4, 0, 6), 0.45f), Lay("Émail", {0.85f, 0.85f, 0.82f}, 0.08f, 0, Gen(0, 0.15f, 12, 0.55f, 14, true), 0.55f),
                          Lay("Bord des éclats", {0.16f, 0.12f, 0.1f}, 0.5f, 0, follow(-1, 1, 3, 0.6f, 0.2f))}));
    list.push_back(Smart("Jouet en plastique peint", "Plastique rouge brillant, peinture jaune écaillée sur les arêtes, poussière dans les creux.",
                         {Lay("Plastique", {0.7f, 0.06f, 0.05f}, 0.25f, 0, Gen(7, 1, 4, 0, 6)), Lay("Peinture jaune", {0.85f, 0.65f, 0.05f}, 0.35f, 0, Gen(0, 0.35f, 10, 0.5f, 8, true)),
                          Lay("Plastique apparent", {0.75f, 0.72f, 0.68f}, 0.5f, 0, follow(-1, 1, 4, 0.5f, 0.2f)),
                          Lay("Poussière", {0.45f, 0.42f, 0.38f}, 1.0f, 0, Gen(1, 0.3f, 3, 0.4f, 8))}));
    list.push_back(Smart("Fer forgé noirci", "Fer presque noir, arêtes polies brillantes par l'usage, rouille autour des parties polies.",
                         {Lay("Fer noirci", {0.06f, 0.06f, 0.065f}, 0.7f, 0.8f, Gen(7, 1, 4, 0, 6)), Lay("Arêtes polies", {0.6f, 0.6f, 0.6f}, 0.25f, 1, Gen(0, 0.2f, 6, 0.3f, 10)),
                          Lay("Rouille", {0.36f, 0.15f, 0.05f}, 0.9f, 0, follow(-1, 2, 18, 0.65f, 0.5f), 0.55f)}));
    list.push_back(Smart("Fer rouillé", "Fer brut, rouille dans les creux et par plaques.",
                         {Lay("Fer", {0.5f, 0.5f, 0.52f}, 0.55f, 1, full), Lay("Rouille des creux", {0.42f, 0.19f, 0.07f}, 0.9f, 0.1f, Gen(1, 0.5f, 3, 0.5f, 6), 0.55f),
                          Lay("Plaques de rouille", {0.5f, 0.24f, 0.09f}, 0.85f, 0.15f, Gen(5, 0.3f, 4, 0, 3), 0.53f)}));
    list.push_back(Smart("Plastique sale", "Plastique, saleté dans les creux et poussière sur le dessus.",
                         {Lay("Plastique", {0.86f, 0.86f, 0.84f}, 0.42f, 0, full), Lay("Saleté", {0.2f, 0.16f, 0.12f}, 0.9f, 0, Gen(1, 0.4f, 3, 0.3f, 8)),
                          Lay("Poussière", {0.62f, 0.6f, 0.56f}, 0.95f, 0, Gen(2, 0.3f, 4, 0.4f, 14))}));
    list.push_back(Smart("Bois verni usé", "Bois verni brillant, vernis parti sur les arêtes.",
                         {Lay("Bois verni", {0.42f, 0.25f, 0.12f}, 0.3f, 0, full), Lay("Bois à nu", {0.55f, 0.38f, 0.22f}, 0.8f, 0, Gen(0, 0.25f, 5, 0.4f, 10))}));
    list.push_back(Smart("Cuivre oxydé", "Cuivre, vert-de-gris dans les creux et par endroits.",
                         {Lay("Cuivre", {0.95f, 0.64f, 0.54f}, 0.3f, 1, full), Lay("Vert-de-gris", {0.3f, 0.58f, 0.48f}, 0.9f, 0, Gen(1, 0.55f, 3, 0.45f, 6))}));
    list.push_back(Smart("Chrome poussiéreux", "Chrome poli, poussière sur les surfaces tournées vers le haut.",
                         {Lay("Chrome", {0.92f, 0.92f, 0.94f}, 0.06f, 1, full), Lay("Poussière", {0.55f, 0.53f, 0.5f}, 0.95f, 0, Gen(2, 0.35f, 3, 0.5f, 16))}));
    list.push_back(Smart("Caoutchouc usé", "Caoutchouc noir, arêtes éclaircies par l'usure.",
                         {Lay("Caoutchouc", {0.05f, 0.05f, 0.05f}, 0.9f, 0, full), Lay("Usure", {0.22f, 0.22f, 0.22f}, 0.75f, 0, Gen(0, 0.18f, 4, 0.3f, 12))}));
    list.push_back(Smart("Pierre moussue", "Pierre, mousse en bas et dans les creux.",
                         {Lay("Pierre", {0.45f, 0.44f, 0.42f}, 0.9f, 0, full), Lay("Mousse (bas)", {0.2f, 0.33f, 0.1f}, 0.95f, 0, Gen(3, 0.3f, 3, 0.5f, 5, false, 1)),
                          Lay("Mousse (creux)", {0.18f, 0.3f, 0.09f}, 0.95f, 0, Gen(1, 0.3f, 3, 0.5f, 8))}));
    list.push_back(Smart("Peinture militaire", "Vert olive mat, métal sur les arêtes, boue en bas.",
                         {Lay("Métal", {0.45f, 0.45f, 0.46f}, 0.5f, 1, full), Lay("Peinture olive", {0.29f, 0.32f, 0.18f}, 0.65f, 0, Gen(0, 0.12f, 6, 0.4f, 10, true)),
                          Lay("Boue", {0.25f, 0.19f, 0.12f}, 0.95f, 0, Gen(3, 0.35f, 2.5f, 0.55f, 5, false, 1))}));
    list.push_back(Smart("Or patiné", "Or, creux assombris par la patine.",
                         {Lay("Or", {1.0f, 0.78f, 0.34f}, 0.22f, 1, full), Lay("Patine", {0.3f, 0.22f, 0.1f}, 0.7f, 0.6f, Gen(4, 0.45f, 3, 0.25f, 8))}));
    // Library: next to the exe (package), the repository's library/smartmaterials (development builds)
    // and the user's %LOCALAPPDATA%\Shaderixel\SmartMaterials; same JSON format as Smart() above.
    const std::vector<std::string> dirs = SmartMaterialDirs();
    std::set<std::string> names;
    for (auto& j : list) names.insert(j["name"].S());
    for (const std::string& dir : dirs) {
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(dir), ec)) {
            if (e.path().extension() != ".json") continue;
            std::ifstream f(e.path(), std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            Json j;
            if (Json::Parse(ss.str(), j) && j.Has("layers") && names.insert(j["name"].S()).second) list.push_back(j);
        }
    }
    return list;
}

std::vector<std::string> App::SmartMaterialDirs() {
    // Library: next to the exe (package), the repository's library/smartmaterials (development builds), the user's folder.
    std::vector<std::string> dirs = {ExeDir() + "/smartmaterials", ExeDir() + "/../../library/smartmaterials"};
    if (const char* la = getenv("LOCALAPPDATA")) dirs.push_back(std::string(la) + "/Shaderixel/SmartMaterials");
    return dirs;
}

gpu::Texture* App::SmartMaterialThumb(DocView&, const std::string& name) {
    // Preview on a sphere: thumbs/<name>.png in a library folder (made by --demo smartthumbs), loaded once.
    if (auto it = smartThumbs.find(name); it != smartThumbs.end()) return it->second.get();
    gpu::TexturePtr tex;
    for (const std::string& dir : SmartMaterialDirs()) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, Utf8Path(dir + "/thumbs/" + name + ".png").c_str(), L"rb") != 0 || !f) continue;
        int w = 0, h = 0, comp = 0;
        stbi_uc* px = stbi_load_from_file(f, &w, &h, &comp, 4);
        fclose(f);
        if (px) {
            // Display values as they are (the UI shows sRGB textures directly).
            tex = gpu::Dev().CreateTexture((u32)w, (u32)h, gpu::Format::RGBA8, "SmartThumb");
            if (tex) ctx->Upload(tex.get(), px, (u32)w * 4);
            stbi_image_free(px);
        }
        break;
    }
    smartThumbs[name] = tex;  // missing: remembered too (name only)
    return tex.get();
}

bool App::DrawSmartMaterialGrid(DocView& v) {
    // Grid of the smart materials with their preview; a click applies one. True when one was applied.
    const auto& list = SmartMaterials();
    const float cell = 96 * dpi;
    const int cols = 4;
    bool applied = false;
    if (ImGui::BeginTable("##smartgrid", cols, ImGuiTableFlags_SizingFixedSame)) {
        for (size_t i = 0; i < list.size(); ++i) {
            const Json& sm = list[i];
            const std::string name = sm["name"].S("");
            ImGui::TableNextColumn();
            ImGui::PushID((int)i);
            ImGui::BeginGroup();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##sm", ImVec2(cell, cell))) {
                ApplySmartMaterial(v, sm);
                applied = true;
            }
            const bool hot = ImGui::IsItemHovered();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, ImVec2(p.x + cell, p.y + cell), IM_COL32(22, 22, 26, 255), 6);
            if (gpu::Texture* t = SmartMaterialThumb(v, name))
                dl->AddImageRounded((ImTextureID)gpu::Dev().UiTextureId(t), p, ImVec2(p.x + cell, p.y + cell), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 6);
            else
                dl->AddText(ImVec2(p.x + 8, p.y + cell * 0.45f), ui::col::TextDim, "(aperçu)");
            dl->AddRect(p, ImVec2(p.x + cell, p.y + cell), hot ? ui::col::Accent : ui::col::Border, 6, 0, hot ? 2.0f : 1.0f);
            if (hot && !sm["description"].S("").empty()) ImGui::SetTooltip("%s\n\n%s", name.c_str(), sm["description"].S("").c_str());
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell);
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    return applied;
}

void App::ApplySmartMaterial(DocView& v, const Json& def) {
    v.history.BeginGroup();
    Id last = 0;
    std::vector<Id> made;
    for (const Json& l : def["layers"].Elements()) {
        float3 col;
        const Json& c = l["color"];
        const bool hasC = c.Size() >= 3;
        if (hasC) col = {c.At(0).F(), c.At(1).F(), c.At(2).F()};
        float r = l["rough"].F(0.5f), m = l["metal"].F(0), h = l["height"].F(0.5f);
        const Json& mask = l["mask"];
        const bool full = mask["type"].I(7) == 7 && mask["noise"].F(0) <= 0;
        Id mat = AddMaterialLayerValues(v, l["name"].S("Couche"), hasC ? &col : nullptr, l.Has("rough") ? &r : nullptr, l.Has("metal") ? &m : nullptr,
                                        l.Has("height") ? &h : nullptr, full);
        if (!mat) continue;
        made.push_back(mat);
        if (!full) {
            Json mk = mask;
            if (mk["type"].I(0) == 8 && mk.Has("follow")) {
                // Follows an earlier layer of this smart material (-1: the one just below).
                const int k = (int)made.size() - 1 + mk["follow"].I(-1);
                mk["source"] = k >= 0 && k + 1 < (int)made.size() ? (int)made[k] : 0;
            }
            const std::string text = mk.Dump(0);
            for (auto& x : v.doc->S().layers) if (x.material == mat) x.generator = text;
        }
        last = mat;
    }
    v.history.EndGroup();
    if (last) SelectMaterial(*v.doc, last);
    Status("Matériau intelligent « " + def["name"].S() + " » : " + std::to_string(def["layers"].Size()) + " couches");
}

}  // namespace sx
