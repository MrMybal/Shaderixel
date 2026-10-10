// Smart masks and smart materials (Paint 3D, Substance-like): mesh maps baked
// on the CPU (AO, convexity, concavity, thickness), masks generated on the GPU
// from them (edge wear, cavity dirt, dust, gradients, noise), and multi-layer
// materials whose masks follow the shape of the model.
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "mesh/MeshBake.h"

#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sx {

namespace {

const char* kGenTypes = "Aucun (masque peint)\0Usure des arêtes\0Saleté des creux\0Poussière (dessus)\0Dégradé\0Occlusion\0Bruit\0Parties fines\0Plein\0";
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
        if (NeedsMaps(p.type) && !s.maps) {
            if (!mapsBake.running) StartMeshMapsBake(v);
            continue;
        }
        const std::string sig = l.generator + "|" + std::to_string(s.mapsRev) + "|" + std::to_string(s.revision) + "|" + std::to_string(s.posMap->Width());
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
            static const float amounts[] = {0.25f, 0.4f, 0.35f, 0.5f, 0.5f, 0.5f, 0.3f, 1.0f};
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
    // Mask types: 0 edge wear, 1 cavity dirt, 2 dust, 3 gradient, 4 occlusion, 5 noise, 6 thin parts, 7 full.
    const Json full = Gen(7, 1, 4, 0, 6);
    list.push_back(Smart("Métal peint usé", "Peinture écaillée sur les arêtes, métal apparent, saleté dans les creux.",
                         {Lay("Métal", {0.56f, 0.57f, 0.58f}, 0.45f, 1, full), Lay("Peinture", {0.62f, 0.09f, 0.07f}, 0.38f, 0, Gen(0, 0.22f, 7, 0.45f, 9, true)),
                          Lay("Saleté", {0.17f, 0.13f, 0.09f}, 0.95f, 0, Gen(1, 0.35f, 3, 0.35f, 7))}));
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
    // Library: library/smartmaterials/*.json and %LOCALAPPDATA%\Shaderixel\SmartMaterials (same format).
    for (const std::string& dir : {ExeDir() + "/library/smartmaterials", ExeDir() + "/../../library/smartmaterials"}) {
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(dir), ec)) {
            if (e.path().extension() != ".json") continue;
            std::ifstream f(e.path(), std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            Json j;
            if (Json::Parse(ss.str(), j) && j.Has("layers")) list.push_back(j);
        }
        if (!list.empty() && list.size() > 10) break;  // the packaged folder was found
    }
    return list;
}

void App::ApplySmartMaterial(DocView& v, const Json& def) {
    v.history.BeginGroup();
    Id last = 0;
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
        if (!full) {
            const std::string text = mask.Dump(0);
            for (auto& x : v.doc->S().layers) if (x.material == mat) x.generator = text;
        }
        last = mat;
    }
    v.history.EndGroup();
    if (last) SelectMaterial(*v.doc, last);
    Status("Matériau intelligent « " + def["name"].S() + " » : " + std::to_string(def["layers"].Size()) + " couches");
}

}  // namespace sx
