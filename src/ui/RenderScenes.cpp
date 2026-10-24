// Render scenes on their own: .sxscene files (objects, lights, environment, settings, camera),
// presets (shipped in scenes/ next to the exe, library/scenes in the repository, and the user's in
// %LOCALAPPDATA%\Shaderixel\Scenes), applied whole, lighting only, or objects added; and "gather
// the files": imported models and the HDRI copied next to the project, so it stays complete.
// Paths are stored relative to the file when they are in its folder.
#include "ui/App.h"
#include "ui/Theme.h"
#include "io/UAsset.h"

#include <ShlObj.h>
#include <imgui_stdlib.h>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <regex>

namespace sx {

namespace fs = std::filesystem;

void SceneFromJson(const Json& j, const std::string& baseDir, PtRenderState& r);  // RenderMode.cpp

std::string SceneRelPath(const std::string& path, const std::string& baseDir) {
    if (path.empty() || baseDir.empty() || path[0] == '*') return path;
    std::error_code ec;
    const fs::path p = Utf8Path(path), b = Utf8Path(baseDir);
    if (!p.is_absolute()) return path;
    fs::path rel = fs::relative(p, b, ec);
    if (ec || rel.empty() || rel.native().rfind(L"..", 0) == 0) return path;  // outside the folder: absolute
    return Narrow(rel.generic_wstring());
}

std::string SceneAbsPath(const std::string& path, const std::string& baseDir) {
    if (path.empty() || baseDir.empty() || path[0] == '*') return path;
    const fs::path p = Utf8Path(path);
    if (p.is_absolute()) return path;
    return Narrow((Utf8Path(baseDir) / p).lexically_normal().wstring());
}

namespace {

std::string UserSceneDir() {
    std::string out;
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) out = Narrow(p) + "\\Shaderixel\\Scenes";
    if (p) CoTaskMemFree(p);
    return out;
}

std::vector<std::string> SceneDirs() {
    return {UserSceneDir(), ExeDir() + "\\scenes", Narrow((fs::path(Utf8Path(ExeDir())).parent_path().parent_path() / L"library" / L"scenes").wstring())};
}

std::string SafeName(std::string s) {
    for (auto& c : s)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    return s.empty() ? "Scène" : s;
}

float FloorOf(DocView& v) {
    if (!v.mesh || v.mesh->data.vertices.empty()) return -1.0f;
    float y = 1e30f;
    for (auto& vx : v.mesh->data.vertices) y = std::min(y, vx.pos.y);
    return y;
}

// Lines starting with `key` (after spaces): the rest of the line, options (-x a b) skipped for maps.
std::vector<std::string> LinesWith(const std::string& txt, std::initializer_list<const char*> keys, bool skipOptions) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < txt.size()) {
        size_t eol = txt.find('\n', pos);
        if (eol == std::string::npos) eol = txt.size();
        std::string line = txt.substr(pos, eol - pos);
        pos = eol + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        line = line.substr(a);
        for (const char* k : keys) {
            const size_t n = strlen(k);
            if (line.compare(0, n, k) != 0 || line.size() <= n || (line[n] != ' ' && line[n] != '\t' && line[n - 1] != '_')) continue;
            size_t b = line.find_first_of(" \t", n);
            if (b == std::string::npos) continue;
            std::string rest = line.substr(line.find_first_not_of(" \t", b));
            // Map options ("-bm 1.0 file.png"): the file name is the last word.
            if (skipOptions && !rest.empty() && rest[0] == '-') {
                const size_t sp = rest.find_last_of(" \t");
                if (sp != std::string::npos) rest = rest.substr(sp + 1);
            }
            out.push_back(rest);
            break;
        }
    }
    return out;
}

void AimAt(PtLight& l, float3 to) {
    float3 d = Normalize(to - l.position);
    l.yaw = std::atan2(d.x, d.z) * 57.29578f;
    l.pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f)) * 57.29578f;
}

}  // namespace

bool App::SaveScene(DocView& v, const std::string& path, const std::string& name, const std::string& desc) {
    if (!v.render) return false;
    const std::string dir = Narrow(Utf8Path(path).parent_path().wstring());
    Json j = Json::Object();
    j["format"] = "shaderixel-scene";
    j["name"] = name;
    j["description"] = desc;
    j["floorY"] = FloorOf(v);  // positions are moved to the floor of the project the scene is applied to
    j["scene"] = RenderToJson(*v.render, dir);
    std::ofstream f(Utf8Path(path), std::ios::binary);
    if (!f) return false;
    f << j.Dump(1);
    return true;
}

bool App::LoadScene(DocView& v, const std::string& path, int mode) {
    std::string txt;
    Json j;
    if (!ReadTextFile(path, txt) || !Json::Parse(txt, j) || !j.Has("scene")) {
        Status("Scène illisible : " + path);
        return false;
    }
    PtRenderState loaded;
    SceneFromJson(j["scene"], Narrow(Utf8Path(path).parent_path().wstring()), loaded);
    // Moved to this project's floor.
    const float dy = FloorOf(v) - j["floorY"].F(FloorOf(v));
    for (auto& o : loaded.objects)
        if (o.type != PtObjectType::Project) o.position.y += dy;
    for (auto& l : loaded.lights) l.position.y += dy;
    loaded.target.y += dy;
    PtRenderState& r = RenderStateFor(v);
    PtSettings& s = r.settings;
    const PtSettings& n = loaded.settings;
    auto lighting = [&] {
        r.lights = loaded.lights;
        s.hdri = n.hdri;
        s.envIntensity = n.envIntensity;
        s.envRotation = n.envRotation;
        s.envVisible = n.envVisible;
        s.background = n.background;
        s.exposure = n.exposure;
        s.tonemap = n.tonemap;
    };
    const std::string name = j["name"].S(Narrow(Utf8Path(path).stem().wstring()));
    if (mode == 1) {
        lighting();
        Status("Scène « " + name + " » : éclairage et environnement appliqués");
    } else if (mode == 2) {
        int added = 0;
        for (auto& o : loaded.objects)
            if (o.type != PtObjectType::Project) { r.objects.push_back(o); ++added; }
        Status("Scène « " + name + " » : " + std::to_string(added) + " objet(s) ajouté(s)");
    } else {
        // Everything, but the render quality of this machine (device, samples, size) is kept.
        const PtSettings keep = s;
        r.objects = loaded.objects;
        lighting();
        s.aperture = n.aperture;
        s.focusDistance = n.focusDistance;
        s.fov = n.fov;
        s.clampIndirect = n.clampIndirect;
        s.maxBounces = n.maxBounces;
        s.device = keep.device;
        s.maxSamples = keep.maxSamples;
        if (j["scene"].Has("camera")) {
            r.yaw = loaded.yaw;
            r.pitch = loaded.pitch;
            r.distance = loaded.distance;
            r.target = loaded.target;
        }
        bool hasModel = false;
        for (auto& o : r.objects) hasModel |= o.type == PtObjectType::Project;
        if (!hasModel) {
            PtObject model;
            model.name = "Modèle";
            model.type = PtObjectType::Project;
            model.material.useProject = true;
            r.objects.insert(r.objects.begin(), model);
        }
        Status("Scène « " + name + " » appliquée");
    }
    r.selObject = 0;
    r.selLight = -1;
    v.doc->dirty = true;
    return true;
}

void App::ScanScenePresets() {
    scenePresets.clear();
    const std::string user = UserSceneDir();
    for (const std::string& dir : SceneDirs()) {
        std::error_code ec;
        if (dir.empty() || !fs::is_directory(Utf8Path(dir), ec)) continue;
        for (auto& e : fs::directory_iterator(Utf8Path(dir), ec)) {
            if (e.path().extension() != L".sxscene") continue;
            ScenePreset p;
            p.path = Narrow(e.path().wstring());
            p.name = Narrow(e.path().stem().wstring());
            p.user = dir == user;
            std::string txt;
            Json j;
            if (ReadTextFile(p.path, txt) && Json::Parse(txt, j)) {
                p.name = j["name"].S(p.name);
                p.desc = j["description"].S("");
            }
            bool dup = false;
            for (auto& o : scenePresets) dup |= o.name == p.name;
            if (!dup) scenePresets.push_back(std::move(p));
        }
    }
    std::stable_sort(scenePresets.begin(), scenePresets.end(), [](const ScenePreset& a, const ScenePreset& b) {
        return a.user != b.user ? !a.user : a.name < b.name;
    });
}

// Imported models (and what they need: .mtl, textures, .bin) and the HDRI copied into
// "<project>_fichiers" next to the project; the scene then points at the copies.
void App::GatherSceneFiles(DocView& v) {
    if (!v.render) return;
    if (v.doc->path.empty()) { Status("Rassembler : enregistrez d'abord le projet (les fichiers sont copiés à côté)"); return; }
    const fs::path project = Utf8Path(v.doc->path);
    const fs::path dir = project.parent_path() / (project.stem().wstring() + L"_fichiers");
    std::error_code ec;
    fs::create_directories(dir, ec);
    int copied = 0, missing = 0, outside = 0;
    auto copyOne = [&](const fs::path& src, const fs::path& into) -> fs::path {
        if (!fs::exists(src, ec)) { ++missing; return {}; }
        const fs::path dst = into / src.filename();
        if (fs::equivalent(src, dst, ec)) return dst;
        if (fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec)) ++copied;
        return dst;
    };
    // Already inside the folder (gathered before): left where it is.
    auto inside = [&](const fs::path& p) {
        for (fs::path q = p.parent_path(); !q.empty() && q != q.root_path(); q = q.parent_path())
            if (fs::equivalent(q, dir, ec)) return true;
        return false;
    };
    // A reference kept relative to its model: no absolute path, no "..", so the copy stays inside the folder.
    auto safeRef = [](const fs::path& r) {
        if (r.empty() || r.is_absolute() || r.has_root_name() || r.has_root_directory()) return false;
        for (auto& part : r)
            if (part.wstring() == L"..") return false;
        return true;
    };
    // Files a model refers to (OBJ: mtllib + map_*, glTF: uri), copied beside it (in its own sub-folder).
    auto copyDeps = [&](const fs::path& model, const fs::path& into) {
        std::string ext = Narrow(model.extension().wstring());
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        std::string txt;
        if (ext != ".obj" && ext != ".gltf") return;
        if (!ReadTextFile(Narrow(model.wstring()), txt)) return;
        std::vector<std::string> refs;
        if (ext == ".gltf") {
            static const std::regex uri("\"uri\"\\s*:\\s*\"([^\"]+)\"");
            for (std::sregex_iterator it(txt.begin(), txt.end(), uri), end; it != end; ++it)
                if ((*it)[1].str().rfind("data:", 0) != 0) refs.push_back((*it)[1].str());
        } else {
            for (const std::string& m : LinesWith(txt, {"mtllib"}, false)) {
                refs.push_back(m);
                std::string mt;
                if (ReadTextFile(Narrow((model.parent_path() / Utf8Path(m)).wstring()), mt))
                    for (const std::string& t : LinesWith(mt, {"map_Kd", "map_Ks", "map_Ka", "map_Ke", "map_Ns", "map_d", "map_Bump", "map_bump", "map_Pr", "map_Pm", "map_Rough", "bump", "disp", "norm"}, true))
                        refs.push_back(t);
            }
        }
        for (auto& r : refs) {
            const fs::path rel = Utf8Path(r).lexically_normal();
            if (!safeRef(rel)) { ++outside; continue; }  // would land outside the folder: not copied (the model keeps pointing at the original)
            const fs::path src = model.parent_path() / rel;
            const fs::path dst = into / rel;
            fs::create_directories(dst.parent_path(), ec);
            if (fs::exists(src, ec) && !fs::equivalent(src, dst, ec) && fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec)) ++copied;
        }
    };
    // One sub-folder per source model (with its sidecar files): two models of the same name never overwrite each other.
    std::map<std::wstring, fs::path> modelDirs;  // source model -> its sub-folder
    auto modelDir = [&](const fs::path& src, const fs::path& named) -> fs::path {
        const std::wstring key = src.lexically_normal().wstring();
        if (auto it = modelDirs.find(key); it != modelDirs.end()) return it->second;
        std::wstring stem = named.stem().wstring();
        if (stem.empty()) stem = L"modele";
        fs::path sub = dir / stem;
        for (int n = 2; fs::exists(sub, ec); ++n) sub = dir / (stem + L"_" + std::to_wstring(n));
        fs::create_directories(sub, ec);
        return modelDirs[key] = sub;
    };
    for (auto& o : v.render->objects) {
        if (o.path.empty() || (o.type != PtObjectType::File && o.type != PtObjectType::Sx3d)) continue;
        fs::path src = Utf8Path(o.path);
        if (inside(src)) continue;
        std::string ext = Narrow(src.extension().wstring());
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        fs::path mat;
        if (ext == ".uasset") {
            // Unreal mesh: its converted copy (no Unreal needed any more).
            const std::string glb = UAssetMeshCached(o.path);
            if (glb.empty()) { ++missing; continue; }
            src = Utf8Path(glb);
            // Material table written by the conversion as X.materials.json (older copies: X.glb.materials.json).
            mat = Utf8Path(glb).replace_extension(".materials.json");
            if (!fs::exists(mat, ec)) mat = Utf8Path(glb + ".materials.json");
            if (!fs::exists(mat, ec)) mat.clear();
        }
        if (!fs::exists(src, ec)) { ++missing; continue; }
        const fs::path into = modelDir(src, Utf8Path(o.path));  // an Unreal mesh's folder is named after its asset
        const fs::path dst = copyOne(src, into);
        if (dst.empty()) continue;
        if (!mat.empty() && fs::copy_file(mat, fs::path(dst).replace_extension(".materials.json"), fs::copy_options::overwrite_existing, ec)) ++copied;
        copyDeps(src, into);
        o.path = Narrow(dst.wstring());
    }
    PtSettings& s = v.render->settings;
    if (!s.hdri.empty() && s.hdri[0] != '*' && !inside(Utf8Path(s.hdri))) {
        const fs::path dst = copyOne(Utf8Path(s.hdri), dir);
        if (!dst.empty()) s.hdri = Narrow(dst.wstring());
    }
    v.doc->dirty = true;
    Status("Fichiers rassemblés dans " + Narrow(dir.filename().wstring()) + " : " + std::to_string(copied) + " copié(s)" +
           (missing ? ", " + std::to_string(missing) + " introuvable(s)" : "") +
           (outside ? ", " + std::to_string(outside) + " référence(s) hors du dossier du modèle non copiée(s)" : "") + " ; enregistrez le projet");
}

void App::DrawScenesUI(DocView& v) {
    if (!ImGui::CollapsingHeader("Scène (fichiers, préréglages)")) return;
    if (scenePresets.empty() || ImGui::IsWindowAppearing()) ScanScenePresets();
    sceneUi.preset = std::clamp(sceneUi.preset, 0, std::max(0, (int)scenePresets.size() - 1));
    if (ImGui::BeginCombo("Préréglage", scenePresets.empty() ? "(aucun)" : scenePresets[sceneUi.preset].name.c_str())) {
        for (int i = 0; i < (int)scenePresets.size(); ++i) {
            if (ImGui::Selectable((scenePresets[i].name + (scenePresets[i].user ? "  (perso)" : "")).c_str(), i == sceneUi.preset)) sceneUi.preset = i;
            if (ImGui::IsItemHovered() && !scenePresets[i].desc.empty()) ImGui::SetTooltip("%s", scenePresets[i].desc.c_str());
        }
        ImGui::EndCombo();
    }
    ImGui::Combo("Appliquer", &sceneUi.mode, "Toute la scène\0Éclairage et environnement\0Ajouter ses objets\0");
    ImGui::BeginDisabled(scenePresets.empty());
    if (ImGui::Button("Appliquer le préréglage")) LoadScene(v, scenePresets[sceneUi.preset].path, sceneUi.mode);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Charger une scène...")) {
        const std::string p = platform::OpenFileDialog(hwnd, L"Scène Shaderixel (*.sxscene)\0*.sxscene\0");
        if (!p.empty()) LoadScene(v, p, sceneUi.mode);
    }
    if (ImGui::Button("Enregistrer la scène...")) {
        const std::string p = platform::SaveFileDialog(hwnd, L"Scène Shaderixel (*.sxscene)\0*.sxscene\0", L"sxscene");
        if (!p.empty()) Status(SaveScene(v, p, Narrow(Utf8Path(p).stem().wstring()), "") ? "Scène enregistrée : " + p : "Impossible d'écrire " + p);
    }
    ui::HelpTooltip("Objets, lumières, environnement, réglages et caméra dans un fichier .sxscene, réutilisable dans d'autres projets.");
    ImGui::SameLine();
    if (ImGui::Button("Rassembler les fichiers")) GatherSceneFiles(v);
    ui::HelpTooltip("Copie les modèles importés (avec .mtl, textures, .bin) et le HDRI dans un dossier à côté du projet : le projet reste complet si les originaux bougent ou si vous l'envoyez.");
    ImGui::InputText("Nom du préréglage", &sceneUi.saveName);
    ImGui::InputText("Description", &sceneUi.saveDesc);
    ImGui::BeginDisabled(sceneUi.saveName.empty());
    if (ImGui::Button("Enregistrer comme préréglage")) {
        std::error_code ec;
        fs::create_directories(Utf8Path(UserSceneDir()), ec);
        const std::string p = UserSceneDir() + "\\" + SafeName(sceneUi.saveName) + ".sxscene";
        if (SaveScene(v, p, sceneUi.saveName, sceneUi.saveDesc)) {
            Status("Préréglage de scène enregistré : " + sceneUi.saveName);
            ScanScenePresets();
        }
    }
    ImGui::EndDisabled();
    // Files the scene needs that are not found.
    if (v.render) {
        std::error_code ec;
        for (auto& o : v.render->objects)
            if (!o.path.empty() && (o.type == PtObjectType::File || o.type == PtObjectType::Sx3d) && !fs::exists(Utf8Path(o.path), ec))
                ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "Introuvable : %s (%s)", o.name.c_str(), o.path.c_str());
    }
}

// Shipped presets, built here (--demo makescenes, SX_SCENEDIR = output folder), floor at y = 0.
void App::SaveBuiltinScenes(DocView& v, const std::string& dir) {
    std::error_code ec;
    fs::create_directories(Utf8Path(dir), ec);
    auto model = [] {
        PtObject m;
        m.name = "Modèle";
        m.type = PtObjectType::Project;
        m.material.useProject = true;
        return m;
    };
    auto obj = [](PtObjectType t, const char* name, float3 pos, float scale, float3 color, float rough, float metal = 0) {
        PtObject o;
        o.type = t;
        o.name = name;
        o.position = pos;
        o.scale = scale;
        o.material.baseColor = color;
        o.material.roughness = rough;
        o.material.metallic = metal;
        return o;
    };
    auto light = [](PtLightType t, const char* name, float3 pos, float3 to, float intensity, float size, float3 color = {1, 1, 1}) {
        PtLight l;
        l.type = t;
        l.name = name;
        l.position = pos;
        l.intensity = intensity;
        l.size = size;
        l.color = color;
        AimAt(l, to);
        return l;
    };
    struct SceneDef {
        std::vector<PtObject> objects;
        std::vector<PtLight> lights;
        PtSettings settings;
        float yaw = 0, pitch = 0, distance = 3.4f;
        float3 target;
    };
    struct Def { const char* name; const char* desc; SceneDef s; };
    std::deque<Def> defs;
    auto add = [&](const char* name, const char* desc) -> SceneDef& {
        defs.push_back({name, desc, {}});
        SceneDef& s = defs.back().s;
        s.objects.push_back(model());
        s.yaw = 0.35f;
        s.pitch = 0.2f;
        s.distance = 3.4f;
        s.target = {0, 0.9f, 0};
        return s;
    };
    const float3 gray = {0.62f, 0.62f, 0.64f};
    {
        SceneDef& s = add("Studio doux", "Fond cyclo gris, une grande boîte à lumière, environnement studio : la scène par défaut.");
        s.objects.push_back(obj(PtObjectType::Backdrop, "Fond", {0, 0, 0.4f}, 1.2f, gray, 0.75f));
        s.lights.push_back(light(PtLightType::Area, "Boîte à lumière", {-1.8f, 3.4f, -1.6f}, {0, 1, 0}, 60, 1.4f));
    }
    {
        SceneDef& s = add("Studio trois lumières", "Principale, débouchage et contre-jour : modelé classique de photo produit.");
        s.objects.push_back(obj(PtObjectType::Backdrop, "Fond", {0, 0, 0.4f}, 1.2f, {0.5f, 0.5f, 0.52f}, 0.8f));
        s.lights.push_back(light(PtLightType::Area, "Principale", {-2.0f, 3.2f, -1.8f}, {0, 1, 0}, 70, 1.2f, {1.0f, 0.96f, 0.9f}));
        s.lights.push_back(light(PtLightType::Area, "Débouchage", {2.2f, 1.8f, -1.6f}, {0, 1, 0}, 18, 1.6f, {0.85f, 0.9f, 1.0f}));
        PtLight rim = light(PtLightType::Spot, "Contre-jour", {0.6f, 3.0f, 2.4f}, {0, 1.2f, 0}, 120, 0.15f);
        rim.spotAngle = 35;
        s.lights.push_back(rim);
        s.settings.envIntensity = 0.3f;
    }
    {
        SceneDef& s = add("Produit sur fond noir", "Fond noir, deux bandes lumineuses latérales et un contre-jour : reflets nets sur les matières brillantes.");
        s.objects.push_back(obj(PtObjectType::Backdrop, "Fond noir", {0, 0, 0.4f}, 1.2f, {0.015f, 0.015f, 0.018f}, 0.6f));
        s.lights.push_back(light(PtLightType::Area, "Bande gauche", {-2.2f, 1.6f, 0.2f}, {0, 1, 0}, 45, 0.6f));
        s.lights.push_back(light(PtLightType::Area, "Bande droite", {2.2f, 1.6f, 0.2f}, {0, 1, 0}, 45, 0.6f));
        s.lights.push_back(light(PtLightType::Area, "Dessus", {0, 3.6f, 1.2f}, {0, 1, 0}, 25, 1.0f));
        s.settings.hdri = "";
        s.settings.background = {0.0f, 0.0f, 0.0f};
        s.settings.envVisible = false;
    }
    {
        SceneDef& s = add("Extérieur ensoleillé", "Soleil rasant, sol clair et ciel bleuté : lumière naturelle et ombres dures.");
        s.objects.push_back(obj(PtObjectType::Plane, "Sol", {0, 0, 0}, 60.0f, {0.55f, 0.52f, 0.48f}, 0.85f));
        PtLight sun = light(PtLightType::Sun, "Soleil", {3, 4, -2}, {0, 0, 0}, 4.5f, 1.0f, {1.0f, 0.93f, 0.82f});
        s.lights.push_back(sun);
        // The studio environment still lights the shadows (sky light), the background shows a blue sky.
        s.settings.envIntensity = 0.6f;
        s.settings.envVisible = false;
        s.settings.background = {0.45f, 0.62f, 0.85f};
    }
    {
        SceneDef& s = add("Contre-jour dramatique", "Spot derrière le modèle sur fond sombre, peu d'ambiance : silhouette et liseré lumineux.");
        s.objects.push_back(obj(PtObjectType::Backdrop, "Fond", {0, 0, 0.4f}, 1.2f, {0.08f, 0.08f, 0.09f}, 0.7f));
        PtLight back = light(PtLightType::Spot, "Contre-jour", {0, 2.2f, 2.6f}, {0, 1, 0}, 220, 0.2f, {1.0f, 0.75f, 0.5f});
        back.spotAngle = 45;
        s.lights.push_back(back);
        s.lights.push_back(light(PtLightType::Area, "Faible face", {-1.5f, 1.5f, -2.5f}, {0, 1, 0}, 6, 1.0f, {0.6f, 0.7f, 1.0f}));
        s.settings.envIntensity = 0.1f;
        s.settings.background = {0.01f, 0.01f, 0.015f};
    }
    {
        SceneDef& s = add("Sol réfléchissant", "Sol sombre et brillant, lumière en douche : le modèle se reflète (vitrine, présentation).");
        s.objects.push_back(obj(PtObjectType::Plane, "Sol brillant", {0, 0, 0}, 8.0f, {0.03f, 0.03f, 0.035f}, 0.12f));
        s.lights.push_back(light(PtLightType::Area, "Douche", {0, 3.8f, 0}, {0, 0, 0}, 80, 1.6f));
        s.settings.envIntensity = 0.25f;
        s.settings.background = {0.02f, 0.02f, 0.025f};
    }
    // Saved through a temporary render state of the view (positions relative to a floor at 0).
    std::unique_ptr<PtRenderState> keep = std::move(v.render);
    for (auto& def : defs) {
        v.render = std::make_unique<PtRenderState>();
        v.render->objects = def.s.objects;
        v.render->lights = def.s.lights;
        v.render->settings = def.s.settings;
        v.render->yaw = def.s.yaw;
        v.render->pitch = def.s.pitch;
        v.render->distance = def.s.distance;
        v.render->target = def.s.target;
        const std::string p = dir + "\\" + SafeName(def.name) + ".sxscene";
        std::string txt;
        SaveScene(v, p, def.name, def.desc);
        // Written with this project's floor: rewritten at 0 (presets are floor-relative).
        Json j;
        if (ReadTextFile(p, txt) && Json::Parse(txt, j)) {
            j["floorY"] = 0.0f;
            std::ofstream(Utf8Path(p), std::ios::binary) << j.Dump(1);
        }
    }
    v.render = std::move(keep);
    ScanScenePresets();
}

}  // namespace sx
