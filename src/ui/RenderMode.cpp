// Render mode of 3D projects: a "Rendu" tab that path traces the painted model in a
// small scene (backdrop, primitives, imported meshes, lights, HDRI) on the GPU (any
// vendor: plain compute shader) or on the CPU, and the "Scène de rendu" panel.
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/PathTracer.h"
#include "io/UAsset.h"

#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <chrono>

namespace sx {

// Scene file paths: relative to the file's folder when inside it (RenderScenes.cpp).
std::string SceneRelPath(const std::string& path, const std::string& baseDir);
std::string SceneAbsPath(const std::string& path, const std::string& baseDir);

namespace {

const char* const kObjectTypes[] = {"Modèle du projet", "Plan", "Fond (cyclo)", "Cube", "Sphère", "Maillage importé", "Projet .sx3d"};

// Part material from an imported model's material.
PtMaterial PartMaterial(const MeshMaterialInfo& mi) {
    PtMaterial m;
    m.baseColor = {mi.baseColor.x, mi.baseColor.y, mi.baseColor.z};
    m.roughness = std::clamp(mi.roughness, 0.02f, 1.0f);
    m.metallic = std::clamp(mi.metallic, 0.0f, 1.0f);
    m.emission = mi.emissive;
    m.emissionStrength = (mi.emissive.x + mi.emissive.y + mi.emissive.z) > 0 ? 1.0f : 0.0f;
    m.useProject = !mi.baseColorPath.empty() || !mi.baseColorData.empty();
    return m;
}

Json MaterialJson(const PtMaterial& m) {
    Json oj = Json::Object();
    oj["color"] = Json::Array();
    for (float f : {m.baseColor.x, m.baseColor.y, m.baseColor.z}) oj["color"].Push(f);
    oj["rough"] = m.roughness;
    oj["metal"] = m.metallic;
    oj["emission"] = Json::Array();
    for (float f : {m.emission.x, m.emission.y, m.emission.z}) oj["emission"].Push(f);
    oj["emissionStrength"] = m.emissionStrength;
    oj["transmission"] = m.transmission;
    oj["ior"] = m.ior;
    oj["project"] = m.useProject;
    oj["coat"] = m.coat;
    oj["coatRough"] = m.coatRoughness;
    return oj;
}
const char* const kLightTypes[] = {"Ponctuelle", "Spot", "Surfacique", "Soleil"};

double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

void Key(std::string& k, float f) {
    char b[24];
    snprintf(b, sizeof(b), "%.5g,", f);
    k += b;
}
void Key(std::string& k, float3 v) {
    Key(k, v.x);
    Key(k, v.y);
    Key(k, v.z);
}

Json J3(float3 v) {
    Json a = Json::Array();
    a.Push(v.x);
    a.Push(v.y);
    a.Push(v.z);
    return a;
}
float3 F3(const Json& j, float3 def) { return j.Size() == 3 ? float3{j.At(0).F(), j.At(1).F(), j.At(2).F()} : def; }

// Linear color edited as sRGB.
bool ColorLinear(const char* label, float3& c) {
    float s[3] = {LinearToSrgb(c.x), LinearToSrgb(c.y), LinearToSrgb(c.z)};
    if (!ImGui::ColorEdit3(label, s, ImGuiColorEditFlags_Float)) return false;
    c = {SrgbToLinear(s[0]), SrgbToLinear(s[1]), SrgbToLinear(s[2])};
    return true;
}

float MeshMinY(const MeshData& m) {
    float y = 1e30f;
    for (auto& v : m.vertices) y = std::min(y, v.pos.y);
    return y < 1e29f ? y : 0.0f;
}

// Yaw / pitch (degrees) of the direction from `from` to `to`.
void Aim(PtLight& l, float3 to) {
    float3 d = Normalize(to - l.position);
    l.yaw = std::atan2(d.x, d.z) * 57.29578f;
    l.pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f)) * 57.29578f;
}

struct MaterialPreset {
    const char* name;
    float3 color;
    float rough, metal, transmission, ior, emission, coat;
};
const MaterialPreset kPresets[] = {
    {"Plastique blanc", {0.8f, 0.8f, 0.8f}, 0.35f, 0, 0, 1.45f, 0, 0},     {"Fond studio", {0.62f, 0.62f, 0.64f}, 0.75f, 0, 0, 1.45f, 0, 0},
    {"Céramique émaillée", {0.85f, 0.83f, 0.8f}, 0.45f, 0, 0, 1.5f, 0, 1}, {"Caoutchouc noir", {0.03f, 0.03f, 0.03f}, 0.85f, 0, 0, 1.45f, 0, 0},
    {"Or", {1.0f, 0.71f, 0.29f}, 0.22f, 1, 0, 1.45f, 0, 0},               {"Chrome", {0.95f, 0.95f, 0.95f}, 0.05f, 1, 0, 1.45f, 0, 0},
    {"Aluminium brossé", {0.91f, 0.92f, 0.92f}, 0.4f, 1, 0, 1.45f, 0, 0},   {"Verre", {1, 1, 1}, 0.02f, 0, 1, 1.5f, 0, 0},
    {"Verre dépoli", {0.95f, 0.97f, 1.0f}, 0.3f, 0, 1, 1.5f, 0, 0},       {"Laque rouge", {0.55f, 0.03f, 0.03f}, 0.4f, 0, 0, 1.45f, 0, 1},
    {"Bois verni", {0.35f, 0.18f, 0.08f}, 0.6f, 0, 0, 1.45f, 0, 0.8f},     {"Émissif", {1, 1, 1}, 0.5f, 0, 0, 1.45f, 8, 0},
};

}  // namespace

std::shared_ptr<MeshData> App::RenderObjectMesh(DocView& v, PtObject& o) {
    MeshData m;
    switch (o.type) {
        case PtObjectType::Project:
            if (!v.mesh) return nullptr;
            return std::make_shared<MeshData>(v.mesh->data);
        case PtObjectType::Plane: m = MakeFloorPlane(1.0f); break;
        case PtObjectType::Backdrop: m = MakeBackdrop(); break;
        case PtObjectType::Cube: m = MakeCube(); break;
        case PtObjectType::Sphere: m = MakeSphere(64, 32); break;
        case PtObjectType::Sx3d: {
            DocView* hv = v.render ? RenderHiddenProject(*v.render, o.path) : nullptr;
            if (!hv || !hv->mesh) {
                if (v.render) v.render->message = "Projet " + o.path + " : illisible ou sans maillage";
                return nullptr;
            }
            return std::make_shared<MeshData>(hv->mesh->data);
        }
        case PtObjectType::File: {
            std::string err, file = o.path;
            if (Narrow(Utf8Path(o.path).extension().wstring()) == ".uasset") {
                // Unreal mesh: its cached glTF, or a conversion in the background (the object appears then).
                file = UAssetMeshCached(o.path);
                if (file.empty()) {
                    if (!uassetFailed.count(o.path)) ConvertUAssetAsync(o.path, nullptr);
                    return nullptr;
                }
            }
            const bool fromUnreal = file != o.path;
            if (!LoadMesh(file, m, &err)) {
                if (v.render) v.render->message = "Maillage " + o.path + " : " + err;
                return nullptr;
            }
            if (fromUnreal) ApplyUAssetMaterials(file, m);
            NormalizeMesh(m);
            break;
        }
    }
    return std::make_shared<MeshData>(std::move(m));
}

DocView* App::RenderHiddenProject(PtRenderState& r, const std::string& path) {
    auto it = r.hiddenProjects.find(path);
    if (it != r.hiddenProjects.end()) return (DocView*)it->second.get();
    // Loaded like a document, then taken out of the tabs: its channels feed the render atlas.
    const int prevActive = activeDoc;
    const size_t before = docs.size();
    if (!LoadSxl(path) || docs.size() != before + 1) {
        r.hiddenProjects[path] = nullptr;
        return nullptr;
    }
    std::shared_ptr<DocView> hidden(std::move(docs.back()));
    docs.pop_back();
    activeDoc = prevActive;
    focusDocRequest = prevActive;
    hidden->render.reset();
    r.hiddenProjects[path] = hidden;
    return hidden.get();
}

bool App::ImportIntoRender(DocView& v, const std::string& path) {
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    const bool sx3d = ext == ".sx3d";
    if (!sx3d && ext != ".obj" && ext != ".gltf" && ext != ".glb" && ext != ".fbx" && ext != ".uasset") return false;
    if (ext == ".uasset" && UAssetMeshCached(path).empty()) {
        // Converted by Unreal first; the object is added when the mesh is ready (if the project is still open).
        DocView* target = &v;
        ConvertUAssetAsync(path, [this, target, path](const std::string&) {
            for (auto& d : docs)
                if (d.get() == target) ImportIntoRender(*target, path);
        });
        return true;
    }
    PtRenderState& r = RenderStateFor(v);
    PtObject o;
    o.type = sx3d ? PtObjectType::Sx3d : PtObjectType::File;
    o.path = path;
    o.name = Narrow(Utf8Path(path).stem().wstring());
    o.material.useProject = sx3d;  // painted textures of the imported project
    o.mesh = RenderObjectMesh(v, o);
    if (!o.mesh) {
        Status("Import dans la scène : " + (r.message.empty() ? path : r.message));
        return false;
    }
    for (auto& mi : o.mesh->materials) o.parts.push_back(PartMaterial(mi));
    // Next to the model, alternately right and left, resting on the floor.
    const float floorY = v.mesh ? MeshMinY(v.mesh->data) : -1.0f;
    int imported = 0;
    for (auto& e : r.objects) imported += e.type == PtObjectType::File || e.type == PtObjectType::Sx3d;
    o.scale = 0.6f;
    o.position = {(imported % 2 ? -1.0f : 1.0f) * (1.6f + 1.3f * (imported / 2)), floorY - MeshMinY(*o.mesh) * o.scale, 0.3f};
    r.objects.push_back(o);
    r.selObject = (int)r.objects.size() - 1;
    r.selLight = -1;
    r.selPart = 0;
    Status("Ajouté à la scène de rendu : " + o.name + (o.parts.size() > 1 ? " (" + std::to_string(o.parts.size()) + " matériaux)" : ""));
    return true;
}

PtRenderState& App::RenderStateFor(DocView& v) {
    if (v.render) return *v.render;
    v.render = std::make_unique<PtRenderState>();
    PtRenderState& r = *v.render;
    const float floor = v.mesh ? MeshMinY(v.mesh->data) : -1.0f;
    PtObject model;
    model.name = "Modèle";
    model.type = PtObjectType::Project;
    model.material.useProject = true;
    r.objects.push_back(model);
    PtObject back;
    back.name = "Fond";
    back.type = PtObjectType::Backdrop;
    back.position = {0, floor, 0.4f};
    back.scale = 1.2f;
    back.material.baseColor = kPresets[1].color;
    back.material.roughness = kPresets[1].rough;
    r.objects.push_back(back);
    PtLight key;
    key.name = "Boîte à lumière";
    key.type = PtLightType::Area;
    key.position = {-1.8f, 2.4f, -1.6f};
    key.size = 1.4f;
    key.intensity = 60.0f;
    Aim(key, {0, 0, 0});
    r.lights.push_back(key);
    r.settings.envIntensity = 1.0f;
    // Facing the backdrop (behind the model along +z), slightly from the side and above.
    r.yaw = 0.35f;
    r.pitch = 0.2f;
    r.distance = v.mesh ? std::max(v.mesh->distance * 1.15f, 2.5f) : 3.4f;
    r.settings.focusDistance = r.distance;
    return r;
}

Json App::RenderToJson(const PtRenderState& r, const std::string& baseDir) {
    Json j = Json::Object();
    Json objs = Json::Array();
    for (auto& o : r.objects) {
        Json oj = Json::Object();
        oj["name"] = o.name;
        oj["type"] = (int)o.type;
        if (!o.path.empty()) oj["path"] = SceneRelPath(o.path, baseDir);
        oj["pos"] = J3(o.position);
        oj["rot"] = J3(o.rotation);
        oj["scale"] = o.scale;
        oj["visible"] = o.visible;
        const PtMaterial& m = o.material;
        oj["color"] = J3(m.baseColor);
        oj["rough"] = m.roughness;
        oj["metal"] = m.metallic;
        oj["emission"] = J3(m.emission);
        oj["emissionStrength"] = m.emissionStrength;
        oj["transmission"] = m.transmission;
        oj["ior"] = m.ior;
        oj["project"] = m.useProject;
        oj["coat"] = m.coat;
        oj["coatRough"] = m.coatRoughness;
        if (!o.parts.empty()) {
            Json parts = Json::Array();
            for (auto& pm : o.parts) parts.Push(MaterialJson(pm));
            oj["parts"] = parts;
        }
        objs.Push(oj);
    }
    j["objects"] = objs;
    Json lights = Json::Array();
    for (auto& l : r.lights) {
        Json lj = Json::Object();
        lj["name"] = l.name;
        lj["type"] = (int)l.type;
        lj["pos"] = J3(l.position);
        lj["yaw"] = l.yaw;
        lj["pitch"] = l.pitch;
        lj["color"] = J3(l.color);
        lj["intensity"] = l.intensity;
        lj["size"] = l.size;
        lj["spot"] = l.spotAngle;
        lj["visible"] = l.visible;
        lights.Push(lj);
    }
    j["lights"] = lights;
    const PtSettings& s = r.settings;
    Json sj = Json::Object();
    sj["device"] = s.device;
    sj["samples"] = s.maxSamples;
    sj["bounces"] = s.maxBounces;
    sj["viewSize"] = s.viewSize;
    sj["width"] = s.width;
    sj["height"] = s.height;
    sj["exposure"] = s.exposure;
    sj["tonemap"] = s.tonemap;
    sj["post"] = PostToJson(s.post);
    sj["denoise"] = s.denoise;
    sj["aperture"] = s.aperture;
    sj["focus"] = s.focusDistance;
    sj["fov"] = s.fov;
    sj["ortho"] = s.ortho;
    sj["clamp"] = s.clampIndirect;
    sj["hdri"] = SceneRelPath(s.hdri, baseDir);
    sj["envIntensity"] = s.envIntensity;
    sj["envRotation"] = s.envRotation;
    sj["envVisible"] = s.envVisible;
    sj["background"] = J3(s.background);
    j["settings"] = sj;
    Json cam = Json::Array();
    for (float f : {r.yaw, r.pitch, r.distance, r.target.x, r.target.y, r.target.z}) cam.Push(f);
    j["camera"] = cam;
    return j;
}

// Scene JSON (project or .sxscene) into a state's objects, lights, settings and camera.
void SceneFromJson(const Json& j, const std::string& baseDir, PtRenderState& r) {
    r.objects.clear();
    const Json& objs = j["objects"];
    for (size_t i = 0; i < objs.Size(); ++i) {
        const Json& oj = objs.At(i);
        PtObject o;
        o.name = oj["name"].S("Objet");
        o.type = (PtObjectType)std::clamp(oj["type"].I(1), 0, 6);
        o.path = SceneAbsPath(oj["path"].S(""), baseDir);
        o.position = F3(oj["pos"], {});
        o.rotation = F3(oj["rot"], {});
        o.scale = oj["scale"].F(1);
        o.visible = oj["visible"].B(true);
        PtMaterial& m = o.material;
        m.baseColor = F3(oj["color"], m.baseColor);
        m.roughness = oj["rough"].F(0.5f);
        m.metallic = oj["metal"].F(0);
        m.emission = F3(oj["emission"], {});
        m.emissionStrength = oj["emissionStrength"].F(0);
        m.transmission = oj["transmission"].F(0);
        m.ior = oj["ior"].F(1.45f);
        m.useProject = oj["project"].B(false);
        m.coat = oj["coat"].F(0);
        m.coatRoughness = oj["coatRough"].F(0.05f);
        const Json& parts = oj["parts"];
        for (size_t k = 0; k < parts.Size(); ++k) {
            const Json& pj = parts.At(k);
            PtMaterial pm;
            pm.baseColor = F3(pj["color"], pm.baseColor);
            pm.roughness = pj["rough"].F(0.5f);
            pm.metallic = pj["metal"].F(0);
            pm.emission = F3(pj["emission"], {});
            pm.emissionStrength = pj["emissionStrength"].F(0);
            pm.transmission = pj["transmission"].F(0);
            pm.ior = pj["ior"].F(1.45f);
            pm.useProject = pj["project"].B(false);
            pm.coat = pj["coat"].F(0);
            pm.coatRoughness = pj["coatRough"].F(0.05f);
            o.parts.push_back(pm);
        }
        r.objects.push_back(o);
    }
    r.lights.clear();
    const Json& lights = j["lights"];
    for (size_t i = 0; i < lights.Size(); ++i) {
        const Json& lj = lights.At(i);
        PtLight l;
        l.name = lj["name"].S("Lumière");
        l.type = (PtLightType)std::clamp(lj["type"].I(2), 0, 3);
        l.position = F3(lj["pos"], l.position);
        l.yaw = lj["yaw"].F(l.yaw);
        l.pitch = lj["pitch"].F(l.pitch);
        l.color = F3(lj["color"], {1, 1, 1});
        l.intensity = lj["intensity"].F(l.intensity);
        l.size = lj["size"].F(l.size);
        l.spotAngle = lj["spot"].F(l.spotAngle);
        l.visible = lj["visible"].B(true);
        r.lights.push_back(l);
    }
    const Json& sj = j["settings"];
    PtSettings& s = r.settings;
    s.device = std::clamp(sj["device"].I(0), 0, 1);
    s.maxSamples = sj["samples"].I(s.maxSamples);
    s.maxBounces = sj["bounces"].I(s.maxBounces);
    s.viewSize = sj["viewSize"].B(true);
    s.width = sj["width"].I(s.width);
    s.height = sj["height"].I(s.height);
    s.exposure = sj["exposure"].F(0);
    s.tonemap = sj["tonemap"].I(3);
    s.post = PostFromJson(sj["post"]);
    s.denoise = sj["denoise"].F(0.6f);
    s.aperture = sj["aperture"].F(0);
    s.focusDistance = sj["focus"].F(s.focusDistance);
    s.fov = sj["fov"].F(s.fov);
    s.ortho = sj["ortho"].B(false);
    s.clampIndirect = sj["clamp"].F(s.clampIndirect);
    s.hdri = SceneAbsPath(sj["hdri"].S(s.hdri), baseDir);
    s.envIntensity = sj["envIntensity"].F(1);
    s.envRotation = sj["envRotation"].F(0);
    s.envVisible = sj["envVisible"].B(true);
    s.background = F3(sj["background"], s.background);
    const Json& cam = j["camera"];
    if (cam.Size() == 6) {
        r.yaw = cam.At(0).F();
        r.pitch = cam.At(1).F();
        r.distance = cam.At(2).F(3.4f);
        r.target = {cam.At(3).F(), cam.At(4).F(), cam.At(5).F()};
    }
}

void App::RenderFromJson(DocView& v, const Json& j, const std::string& baseDir) {
    v.render.reset();
    PtRenderState& r = RenderStateFor(v);
    if (!j.Has("objects")) return;
    PtRenderState loaded;
    SceneFromJson(j, baseDir, loaded);
    r.objects = std::move(loaded.objects);
    r.lights = std::move(loaded.lights);
    r.settings = loaded.settings;
    r.yaw = loaded.yaw;
    r.pitch = loaded.pitch;
    r.distance = loaded.distance;
    r.target = loaded.target;
}

// Scene build, accumulation resets, GPU passes / CPU thread, tone mapping. Returns the display image.
gpu::Texture* App::UpdateRender(DocView& v, u32 w, u32 h) { return UpdateRenderState(v, RenderStateFor(v), w, h); }

gpu::Texture* App::UpdateViewRayTrace(DocView& v, u32 w, u32 h) {
    // Ray-traced 3D view (Marmoset-like viewport): its own scene (the model, an optional floor),
    // the view's camera, HDRI (or the studio) and key light (as a sun), refined while nothing moves.
    if (!v.mesh) return nullptr;
    MeshScene& m = *v.mesh;
    if (!v.viewRT) {
        v.viewRT = std::make_unique<PtRenderState>();
        PtObject model;
        model.name = "Modèle";
        model.type = PtObjectType::Project;
        model.material.useProject = true;
        v.viewRT->objects.push_back(model);
    }
    PtRenderState& r = *v.viewRT;
    // Floor (shadow catcher look): under the model.
    const bool hasFloor = r.objects.size() > 1;
    if (m.rtFloor != hasFloor) {
        if (m.rtFloor) {
            PtObject f;
            f.name = "Sol";
            f.type = PtObjectType::Plane;
            f.position = {0, MeshMinY(m.data), 0};
            f.scale = 6.0f;
            f.material.baseColor = {0.06f, 0.06f, 0.065f};
            f.material.roughness = 0.9f;
            r.objects.push_back(f);
        } else {
            r.objects.resize(1);
        }
    }
    PtSettings& s = r.settings;
    s.device = 0;
    s.maxSamples = m.rtSamples;
    s.maxBounces = m.rtBounces;
    s.viewSize = true;
    s.fov = m.fov;
    s.ortho = m.ortho;
    s.hdri = m.envPath.empty() || !m.envAtlas ? std::string("*studio") : m.envPath;
    s.envIntensity = m.envAtlas ? m.envIntensity : 1.0f;
    s.envRotation = m.envRotation * 57.29578f;
    s.envVisible = m.envAtlas && m.envBackground;
    s.background = {0.05f, 0.052f, 0.062f};  // about the raster view's gray once tone mapped
    s.exposure = m.post.exposure;
    s.tonemap = m.post.tonemap;
    s.post = m.post;
    s.denoise = m.rtDenoise;
    r.yaw = m.yaw;
    r.pitch = m.pitch;
    r.distance = m.distance;
    r.target = m.target;
    // Key light as a sun (direction toward the scene).
    r.lights.clear();
    const float li = m.envAtlas ? m.sunIntensity : m.lightIntensity * 0.6f;
    if (li > 0) {
        float3 l = m.KeyLightDir();
        PtLight sun;
        sun.name = "Soleil";
        sun.type = PtLightType::Sun;
        sun.intensity = li * 2.0f;
        sun.size = 2.0f;
        sun.pitch = std::asin(std::clamp(-l.y, -1.0f, 1.0f)) * 57.29578f;
        sun.yaw = std::atan2(-l.x, -l.z) * 57.29578f;
        r.lights.push_back(sun);
    }
    for (const MeshLight& x : m.lights) {
        if (!x.on || x.intensity <= 0) continue;
        const float3 l = m.LightDir(x);
        PtLight sun;
        sun.name = "Lumière";
        sun.type = PtLightType::Sun;
        sun.color = x.color;
        sun.intensity = x.intensity * 1.2f;
        sun.size = 4.0f;
        sun.pitch = std::asin(std::clamp(-l.y, -1.0f, 1.0f)) * 57.29578f;
        sun.yaw = std::atan2(-l.x, -l.z) * 57.29578f;
        r.lights.push_back(sun);
    }
    s.aperture = m.dofAperture;
    s.focusDistance = m.dofFocus;
    // Paint changes: the textures are packed again.
    const u64 key = v.doc->ContentVersion() * 1315423911ull ^ (u64)v.history.Count() * 2654435761ull ^ (u64)v.history.Cursor();
    if (key != r.paintKey) {
        r.paintKey = key;
        r.texDirty = true;
    }
    r.lastFrame = ImGui::GetFrameCount();  // shown every frame: no repack for being shown again
    r.denoiseDepth = m.depth.get();
    return UpdateRenderState(v, r, w, h);
}

gpu::Texture* App::UpdateRenderState(DocView& v, PtRenderState& r, u32 w, u32 h) {
    PtSettings& s = r.settings;
    if (ImGui::GetFrameCount() - r.lastFrame > 1) r.texDirty = true;  // the tab is shown again: the paint may have changed
    r.lastFrame = ImGui::GetFrameCount();
    bool restart = false;
    // Meshes: project copy (follows UV changes), primitives and files on demand.
    const u32 rev = v.mesh ? v.mesh->revision : 0;
    if (rev != r.projectRevision) {
        for (auto& o : r.objects)
            if (o.type == PtObjectType::Project) o.mesh.reset();
        r.projectRevision = rev;
        r.texDirty = true;
    }
    for (auto& o : r.objects)
        if (!o.mesh) {
            o.mesh = RenderObjectMesh(v, o);
            // One material per part of the model (kept from the saved scene when they match).
            if (o.mesh && o.parts.size() != o.mesh->materials.size()) {
                std::vector<PtMaterial> parts;
                for (size_t i = 0; i < o.mesh->materials.size(); ++i) parts.push_back(i < o.parts.size() ? o.parts[i] : PartMaterial(o.mesh->materials[i]));
                o.parts = std::move(parts);
            }
        }
    // Texture sets (atlas tiles): the open project, imported .sx3d projects, model part textures.
    struct SetSrc { DocView* doc; const MeshMaterialInfo* info; };
    std::vector<SetSrc> sets = {{&v, nullptr}};
    std::string ak = "P";
    for (auto& o : r.objects) {
        if (o.type == PtObjectType::Project) o.material.texSet = 0;
        if (o.type == PtObjectType::Sx3d) {
            DocView* hv = RenderHiddenProject(r, o.path);
            o.material.texSet = hv ? (int)sets.size() : -1;
            if (hv) {
                sets.push_back({hv, nullptr});
                ak += "|S" + o.path;
            }
        }
        if (!o.UsesParts()) continue;
        for (size_t i = 0; i < o.parts.size() && i < o.mesh->materials.size(); ++i) {
            const MeshMaterialInfo& mi = o.mesh->materials[i];
            if (mi.baseColorPath.empty() && mi.baseColorData.empty()) {
                o.parts[i].texSet = -1;
                o.parts[i].useProject = false;
                continue;
            }
            o.parts[i].texSet = (int)sets.size();
            sets.push_back({nullptr, &mi});
            ak += "|I" + o.path + "#" + std::to_string(i) + mi.baseColorPath + ":" + std::to_string(mi.baseColorData.size());
        }
    }
    for (auto& o : r.objects)
        if (o.material.texSet < 0) o.material.useProject = false;
    if (ak != r.atlasKey || r.texDirty) {
        // One tile per set: the project alone keeps its resolution; several share up to 4096 x 4096.
        Document& d = *v.doc;
        const u32 n = (u32)sets.size();
        const u32 g = (u32)std::ceil(std::sqrt((float)n));
        const u32 tile = n == 1 ? std::min(d.Width(), 4096u) : std::max(512u, std::min(2048u, 4096u / g));
        const u32 size = tile * g;
        r.gpu.BeginAtlas(size);
        r.tiles.assign(n, float4{0, 0, 1, 1});
        std::vector<gpu::TexturePtr> keep;
        for (u32 i = 0; i < n; ++i) {
            const u32 x = (i % g) * tile, y = (i / g) * tile;
            r.tiles[i] = {(float)x / size, (float)y / size, (float)tile / size, (float)tile / size};
            gpu::Texture* ch[(int)PbrChannel::Count] = {};
            float defRough = 0.5f, defMetal = 0.0f;
            if (DocView* sv = sets[i].doc) {
                sv->lastImage = sv->renderer.Evaluate(*sv->doc, *ctx);
                MeshMaterial m = GatherMaterial(*sv);
                for (int c = 0; c < (int)PbrChannel::Count; ++c) ch[c] = m.tex[c];
            } else if (const MeshMaterialInfo* mi = sets[i].info) {
                int iw = 0, ih = 0, comp = 0;
                stbi_uc* px = nullptr;
                if (!mi->baseColorData.empty()) px = stbi_load_from_memory(mi->baseColorData.data(), (int)mi->baseColorData.size(), &iw, &ih, &comp, 4);
                else {
                    FILE* f = nullptr;
                    if (_wfopen_s(&f, Widen(mi->baseColorPath).c_str(), L"rb") == 0 && f) {
                        px = stbi_load_from_file(f, &iw, &ih, &comp, 4);
                        fclose(f);
                    }
                }
                if (px) {
                    if (auto t = v.renderer.ImportTexture(*ctx, px, (u32)iw, (u32)ih, false, gpu::Format::RGBA16F)) {
                        ch[(int)PbrChannel::BaseColor] = t.get();
                        keep.push_back(t);
                    }
                    stbi_image_free(px);
                } else r.message = "Texture introuvable : " + (mi->baseColorPath.empty() ? std::string("(intégrée)") : mi->baseColorPath);
                defRough = mi->roughness;
                defMetal = mi->metallic;
            }
            r.gpu.PackTile(*ctx, ch, x, y, tile, defRough, defMetal);
        }
        for (auto& t : r.setTextures) keep.push_back(t);  // the previous ones may still be in flight this frame
        r.setTextures = std::move(keep);
        if (r.setTextures.size() > 64) r.setTextures.erase(r.setTextures.begin() + 32, r.setTextures.end());
        // Imported projects no longer in the scene are released.
        for (auto it = r.hiddenProjects.begin(); it != r.hiddenProjects.end();) {
            bool used = false;
            for (auto& o : r.objects) used |= o.type == PtObjectType::Sx3d && o.path == it->first;
            it = used ? std::next(it) : r.hiddenProjects.erase(it);
        }
        r.atlasKey = ak;
        r.texDirty = false;
        r.cpuTex.reset();
        r.texReadbacks.clear();
        r.geometryKey.clear();  // UVs go to the new tiles
        restart = true;
    }
    // Environment.
    if (r.envPath != s.hdri) {
        r.envPath = s.hdri;
        r.env.reset();
        if (!s.hdri.empty() && s.hdri[0] == '*') r.env = MakeProceduralEnvironment(s.hdri);
        else if (!s.hdri.empty()) {
            std::string err;
            r.env = LoadPtEnvironment(s.hdri, &err);
            if (!r.env) r.message = "HDRI : " + err;
        }
        r.geometryKey.clear();
    }
    // Geometry: rebuilt (BVH) when objects move or change.
    std::string gk;
    for (auto& o : r.objects) {
        gk += std::to_string((int)o.type) + o.path + (o.visible ? "v" : "h") + std::to_string((uintptr_t)o.mesh.get());
        Key(gk, o.position);
        Key(gk, o.rotation);
        Key(gk, o.scale);
    }
    if (gk != r.geometryKey || !r.scene) {
        r.geometryKey = gk;
        r.scene = std::make_shared<PtScene>();
        BuildPtScene(r.objects, r.lights, *r.scene, &r.tiles);
        r.scene->env = r.env;
        r.gpu.SetScene(r.scene);
        restart = true;
    }
    if (s.device == 1 && !r.cpuTex && r.texReadbacks.empty() && r.gpu.packed[0])
        for (auto& t : r.gpu.packed) r.texReadbacks.push_back(ctx->ReadbackTexture(t.get()));
    if (!r.texReadbacks.empty() && std::all_of(r.texReadbacks.begin(), r.texReadbacks.end(), [](auto& rb) { return rb && rb->ready; })) {
        auto tex = std::make_shared<PtProjectTextures>();
        PtImage* imgs[4] = {&tex->base, &tex->orm, &tex->emissive, &tex->normal};
        for (int i = 0; i < 4; ++i) {
            const gpu::Readback& rb = *r.texReadbacks[i];
            imgs[i]->width = (int)rb.width;
            imgs[i]->height = (int)rb.height;
            imgs[i]->pixels.resize((size_t)rb.width * rb.height);
            const u16* src = (const u16*)rb.data.data();
            for (size_t p = 0; p < imgs[i]->pixels.size(); ++p)
                imgs[i]->pixels[p] = {HalfToFloat(src[p * 4]), HalfToFloat(src[p * 4 + 1]), HalfToFloat(src[p * 4 + 2]), HalfToFloat(src[p * 4 + 3])};
        }
        r.cpuTex = tex;
        r.texReadbacks.clear();
        restart = true;
    }
    // Everything else that changes the image (not exposure / tone curve / sample count).
    std::string sk;
    for (const PtMaterial& m : SceneMaterials(r.objects)) {
        Key(sk, m.baseColor);
        Key(sk, m.emission);
        for (float f : {m.roughness, m.metallic, m.emissionStrength, m.transmission, m.ior, m.useProject ? 1.0f : 0.0f, m.coat, m.coatRoughness}) Key(sk, f);
    }
    for (auto& l : r.lights) {
        sk += std::to_string((int)l.type) + (l.visible ? "v" : "h");
        Key(sk, l.position);
        Key(sk, l.color);
        for (float f : {l.yaw, l.pitch, l.intensity, l.size, l.spotAngle}) Key(sk, f);
    }
    for (float f : {r.yaw, r.pitch, r.distance, s.fov, s.aperture, s.focusDistance, s.clampIndirect, s.envIntensity, s.envRotation, (float)s.maxBounces,
                    (float)s.device, (float)w, (float)h, s.envVisible ? 1.0f : 0.0f, s.ortho ? 1.0f : 0.0f})
        Key(sk, f);
    Key(sk, r.target);
    Key(sk, s.background);
    if (sk != r.shadingKey) {
        r.shadingKey = sk;
        restart = true;
    }
    if (restart) {
        r.gpu.Reset();
        r.cpu.Stop();
        r.cpuStarted = false;
        r.cpuShown = -1;
        r.startTime = Now();
        r.doneTime = 0;
    }
    r.width = w;
    r.height = h;
    const PtCamera cam = r.Camera();
    const std::vector<PtMaterial> mats = SceneMaterials(r.objects);
    if (s.device == 0) {
        if (!r.paused && r.gpu.Samples() < s.maxSamples) {
            // A band of rows per dispatch; the band grows while frames stay fast.
            const float dt = ImGui::GetIO().DeltaTime;
            if (dt > 1.0f / 25) r.rowsPerFrame = std::max(4u, r.rowsPerFrame * 3 / 4);
            else if (dt < 1.0f / 50) r.rowsPerFrame = std::min(h * 4, r.rowsPerFrame + std::max(4u, r.rowsPerFrame / 4));
            u32 budget = std::max(1u, r.rowsPerFrame);
            for (int guard = 0; budget > 0 && guard < 16 && r.gpu.Samples() < s.maxSamples; ++guard) {
                u32 rows = std::min({budget, h - std::min(r.gpu.Row(), h - 1), 256u});
                if (!r.gpu.Render(*ctx, mats, r.lights, s, cam, w, h, rows)) break;
                budget -= std::min(budget, rows);
            }
            if (r.gpu.Samples() >= s.maxSamples && r.doneTime == 0) r.doneTime = Now();
        }
    } else {
        r.cpu.SetPaused(r.paused);
        r.cpu.SetMaxSamples(s.maxSamples);
        const bool texReady = r.cpuTex || !r.gpu.packed[0];
        if (!r.cpuStarted && texReady) {
            auto sc = std::make_shared<PtScene>(*r.scene);
            sc->materials = mats;
            sc->lights.clear();
            for (auto& l : r.lights)
                if (l.visible) sc->lights.push_back(l);
            r.cpu.Start(sc, r.cpuTex, s, cam, (int)w, (int)h);
            r.cpuStarted = true;
        }
        int n = 0;
        if (r.cpu.Fetch(r.cpuPixels, n)) {
            r.gpu.UploadAverage(*ctx, r.cpuPixels, w, h);
            r.cpuShown = n;
        }
        if (n >= s.maxSamples && r.doneTime == 0) r.doneTime = Now();
    }
    // .hdr export waiting for its readback.
    if (r.hdrReadback && r.hdrReadback->ready) {
        const gpu::Readback& rb = *r.hdrReadback;
        std::vector<float> rgb((size_t)rb.width * rb.height * 3);
        const float* src = (const float*)rb.data.data();
        for (size_t p = 0; p < (size_t)rb.width * rb.height; ++p) {
            float n = std::max(src[p * 4 + 3], 1.0f), e = std::exp2(s.exposure);
            for (int c = 0; c < 3; ++c) rgb[p * 3 + c] = src[p * 4 + c] / n * e;
        }
        int ok = stbi_write_hdr(r.hdrPath.c_str(), (int)rb.width, (int)rb.height, 3, rgb.data());
        Status(ok ? "Rendu exporté : " + r.hdrPath : "Échec de l'export : " + r.hdrPath);
        r.hdrReadback.reset();
    }
    // Camera effects on the accumulation (exposure and tone curve of the render settings).
    if (r.gpu.accum) {
        PostSettings p = s.post;
        p.exposure = s.exposure;
        p.tonemap = s.tonemap;
        static const double t0 = Now();
        gpu::Texture* src = r.gpu.accum.get();
        bool accumulated = true;
        if (s.denoise > 0 && s.device == 0)
            if (gpu::Texture* dn = r.post.Denoise(*ctx, src, RenderSamples(r), s.denoise, r.denoiseDepth, r.gpu.guideA.get(), r.gpu.guideN.get())) {
                src = dn;
                accumulated = false;
            }
        if (gpu::Texture* out = r.post.Run(*ctx, src, accumulated, false, p, (float)(Now() - t0))) {
            r.display = out;
            return out;
        }
    }
    r.display = r.gpu.Tonemap(*ctx, s);
    return r.display;
}

int App::RenderSamples(PtRenderState& r) { return r.settings.device == 0 ? r.gpu.Samples() : std::max(r.cpuShown, 0); }

void App::ExportRender(DocView& v, const std::string& path) {
    if (path.empty() || !v.render) return;
    PtRenderState& r = *v.render;
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& c : ext) c = (char)tolower(c);
    if (ext == ".hdr") {
        if (!r.gpu.accum) return;
        r.hdrPath = path;
        r.hdrReadback = ctx->ReadbackTexture(r.gpu.accum.get());
        Status("Export du rendu...");
    } else if (r.display || r.gpu.view) {
        exports.push_back({path, ctx->ReadbackTexture(r.display ? r.display : r.gpu.view.get()), {}});
    }
}

void App::ExportRenderPasses(DocView& v, const std::string& path) {
    if (path.empty() || !v.render) return;
    PtRenderState& r = *v.render;
    ExportRender(v, path);
    const auto p = Utf8Path(path);
    const std::string stem = Narrow((p.parent_path() / p.stem()).wstring());
    static const char* names[] = {"_albedo.png", "_normal.png", "_depth.png", "_mask.png"};
    for (int m = 0; m < 4; ++m)
        if (gpu::Texture* t = r.gpu.Pass(*ctx, m, r.distance * 2.5f)) exports.push_back({stem + names[m], ctx->ReadbackTexture(t), {}});
    Status("Export des passes : " + stem + "_*.png");
}

void App::DrawRenderView() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Rendu", PanelFlag("Rendu"), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    DocView* v = Active();
    if (!open || !v) { ImGui::End(); return; }
    const ImVec2 start = ImGui::GetCursorPos();
    if (v->doc->kind != 1 || !v->mesh) {
        ImGui::SetCursorPos(ImVec2(start.x + 16 * dpi, start.y + 24 * dpi));
        ImGui::TextWrapped("Le rendu (path tracing) est disponible pour les projets Paint 3D : Fichier > Nouveau projet Paint 3D, ou ouvrez un fichier .sx3d.");
        ImGui::End();
        return;
    }
    PtRenderState& r = RenderStateFor(*v);
    PtSettings& s = r.settings;
    // Header bar.
    ImGui::SetCursorPos(ImVec2(start.x + 8 * dpi, start.y + 6 * dpi));
    const float rowX = ImGui::GetCursorPosX();
    bool first = true;
    auto flow = [&](float width) {
        if (first) { first = false; return; }
        ImGui::SameLine();
        if (ImGui::GetContentRegionAvail().x < width + 8 * dpi) {
            ImGui::NewLine();
            ImGui::SetCursorPosX(rowX);
        }
    };
    auto button = [&](const char* label) {
        flow(ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2);
        return ImGui::SmallButton(label);
    };
    if (button(r.paused ? "Reprendre" : "Pause")) r.paused = !r.paused;
    if (button("Recommencer")) r.shadingKey.clear();
    flow(90 * dpi);
    ImGui::SetNextItemWidth(90 * dpi);
    ImGui::Combo("##ptdev", &s.device, "GPU\0CPU\0");
    ui::HelpTooltip("GPU : shader de calcul (NVIDIA, AMD, Intel), le plus rapide.\nCPU : tous les cœurs du processeur, même résultat.");
    if (button("Textures du projet")) r.texDirty = true;
    ui::HelpTooltip("Relire les canaux peints (fait automatiquement à l'ouverture de l'onglet).");
    if (button("Caméra de la vue 3D")) {
        r.yaw = v->mesh->yaw;
        r.pitch = v->mesh->pitch;
        r.distance = v->mesh->distance;
        r.target = v->mesh->target;
    }
    if (button("Exporter l'image...")) {
        std::string f = platform::SaveFileDialog(hwnd, L"PNG (*.png)\0*.png\0Radiance HDR (*.hdr)\0*.hdr\0JPEG (*.jpg)\0*.jpg\0", L"png");
        if (!f.empty()) ExportRender(*v, f);
    }
    if (s.device == 0 && button("Exporter les passes...")) {
        std::string f = platform::SaveFileDialog(hwnd, L"PNG (*.png)\0*.png\0", L"png");
        if (!f.empty()) ExportRenderPasses(*v, f);
    }
    ui::HelpTooltip("L'image finale et ses passes pour le compositing : couleur seule (albédo), normales, profondeur, masque\n(<nom>_albedo.png, _normal.png, _depth.png, _mask.png).");
    const int samples = RenderSamples(r);
    const double t = (r.doneTime > 0 ? r.doneTime : Now()) - r.startTime;
    char info[200];
    snprintf(info, sizeof(info), "%s  %d / %d échantillons  %.1f s%s  %u x %u", s.device == 0 ? "GPU" : "CPU", samples, s.maxSamples, t,
             r.paused ? "  (pause)" : samples >= s.maxSamples ? "  (terminé)" : "", r.width, r.height);
    flow(ImGui::CalcTextSize(info).x + 20 * dpi);
    ImGui::TextDisabled("%s", info);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ui::HelpTooltip("Clic gauche / droit : orbite   Molette : zoom   Clic molette : déplacer\nLa scène (objets, lumières, HDRI, réglages) se règle dans le panneau « Scène de rendu » (onglet 3D).");
    if (!r.message.empty()) {
        flow(200 * dpi);
        ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", r.message.c_str());
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4 * dpi);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 8.0f);
    avail.y = std::max(avail.y, 8.0f);
    ImGui::InvisibleButton("##ptview", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
        r.yaw -= io.MouseDelta.x * 0.008f;
        r.pitch = std::clamp(r.pitch + io.MouseDelta.y * 0.008f, -1.5f, 1.5f);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        PtCamera c = r.Camera();
        float3 fwd = Normalize(c.target - c.position), right = Normalize(Cross({0, 1, 0}, fwd)), up = Cross(fwd, right);
        float k = r.distance * 0.0015f;
        r.target = r.target - right * (io.MouseDelta.x * k) + up * (io.MouseDelta.y * k);
    }
    if (hovered && io.MouseWheel != 0) r.distance = std::clamp(r.distance * std::pow(0.88f, io.MouseWheel), 0.2f, 100.0f);
    // Render size: the view, or a fixed resolution shown fitted.
    u32 w = (u32)avail.x, h = (u32)avail.y;
    if (!s.viewSize) {
        w = (u32)std::clamp(s.width, 16, 8192);
        h = (u32)std::clamp(s.height, 16, 8192);
    }
    gpu::Texture* img = UpdateRender(*v, w, h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(18, 18, 20, 255));
    if (img) {
        float k = std::min(avail.x / w, avail.y / h);
        if (s.viewSize) k = 1.0f;
        ImVec2 size(w * k, h * k), p0(origin.x + (avail.x - size.x) * 0.5f, origin.y + (avail.y - size.y) * 0.5f);
        dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(img), p0, ImVec2(p0.x + size.x, p0.y + size.y));
        // Progress of the current pass (GPU bands).
        if (s.device == 0 && r.gpu.Row() > 0) {
            float y = p0.y + size.y * r.gpu.Row() / (float)h;
            dl->AddLine(ImVec2(p0.x, y), ImVec2(p0.x + size.x, y), IM_COL32(255, 140, 0, 90));
        }
    }
    ImGui::End();
}

void App::DrawRenderScenePanel() {
    if (!ImGui::Begin("Scène de rendu", PanelFlag("Scène de rendu"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v || v->doc->kind != 1 || !v->mesh) {
        ImGui::TextWrapped("Scène du rendu (onglet « Rendu ») d'un projet Paint 3D : objets, lumières, HDRI et réglages.");
        ImGui::End();
        return;
    }
    PtRenderState& r = RenderStateFor(*v);
    PtSettings& s = r.settings;
    const float floorY = MeshMinY(v->mesh->data);
    ImGui::PushItemWidth(-90 * dpi);
    DrawScenesUI(*v);

    // ---------------------------------------------------------------- objects
    if (ImGui::CollapsingHeader("Objets", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto add = [&](PtObjectType t, const char* name) {
            PtObject o;
            o.type = t;
            o.name = name;
            if (t == PtObjectType::Plane) { o.position = {0, floorY, 0}; o.scale = 6.0f; o.material.baseColor = kPresets[1].color; o.material.roughness = 0.7f; }
            if (t == PtObjectType::Backdrop) { o.position = {0, floorY, 0.4f}; o.scale = 1.2f; o.material.baseColor = kPresets[1].color; o.material.roughness = 0.75f; }
            if (t == PtObjectType::Cube || t == PtObjectType::Sphere) { o.position = {1.6f, floorY + 0.4f, 0.6f}; o.scale = 0.4f; }
            r.objects.push_back(o);
            r.selObject = (int)r.objects.size() - 1;
            r.selLight = -1;
        };
        if (ImGui::SmallButton("+ Plan")) add(PtObjectType::Plane, "Sol");
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Fond")) add(PtObjectType::Backdrop, "Fond");
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Cube")) add(PtObjectType::Cube, "Cube");
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Sphère")) add(PtObjectType::Sphere, "Sphère");
        if (ImGui::SmallButton("+ Modèle...")) {
            std::string path = platform::OpenFileDialog(hwnd, L"Modèles et projets (*.obj, *.fbx, *.gltf, *.glb, *.uasset, *.sx3d)\0*.obj;*.fbx;*.gltf;*.glb;*.uasset;*.sx3d\0");
            if (!path.empty()) ImportIntoRender(*v, path);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Modèle du projet")) {
            add(PtObjectType::Project, "Modèle (copie)");
            r.objects.back().material.useProject = true;
            r.objects.back().position = {1.8f, 0, 0.8f};
        }
        ImGui::BeginChild("##ptobjs", ImVec2(0, std::min(8.0f, (float)r.objects.size() + 0.5f) * ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders);
        for (int i = 0; i < (int)r.objects.size(); ++i) {
            ImGui::PushID(i);
            ImGui::Checkbox("##vis", &r.objects[i].visible);
            ImGui::SameLine();
            char label[160];
            snprintf(label, sizeof(label), "%s  (%s)", r.objects[i].name.c_str(), kObjectTypes[(int)r.objects[i].type]);
            if (ImGui::Selectable(label, r.selObject == i && r.selLight < 0)) { r.selObject = i; r.selLight = -1; }
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (r.selLight < 0 && r.selObject >= 0 && r.selObject < (int)r.objects.size()) {
            PtObject& o = r.objects[r.selObject];
            if (ImGui::SmallButton("Dupliquer")) {
                PtObject c = o;
                c.name += " (copie)";
                c.position.x += 0.5f;
                r.objects.push_back(c);
                r.selObject = (int)r.objects.size() - 1;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Supprimer")) {
                r.objects.erase(r.objects.begin() + r.selObject);
                r.selObject = std::min(r.selObject, (int)r.objects.size() - 1);
            } else {
                ImGui::SameLine();
                if (ImGui::SmallButton("Poser au sol")) {
                    // Lowest point of the object on the project's floor level.
                    if (o.mesh) o.position.y = floorY - MeshMinY(*o.mesh) * o.scale;
                }
                ImGui::InputText("Nom", &o.name);
                ImGui::DragFloat3("Position", &o.position.x, 0.01f);
                ImGui::DragFloat3("Rotation", &o.rotation.x, 0.5f, -360, 360, "%.1f°");
                ImGui::DragFloat("Échelle", &o.scale, 0.01f, 0.01f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::SeparatorText("Matériau");
                const bool parts = o.UsesParts();
                if (parts) {
                    // Imported model: one material per part.
                    r.selPart = std::clamp(r.selPart, 0, (int)o.parts.size() - 1);
                    auto partName = [&](int i) {
                        std::string n = i < (int)o.mesh->materials.size() ? o.mesh->materials[i].name : std::string();
                        return std::to_string(i + 1) + " : " + (n.empty() ? std::string("Matériau") : n);
                    };
                    if (ImGui::BeginCombo("Partie", partName(r.selPart).c_str())) {
                        for (int i = 0; i < (int)o.parts.size(); ++i)
                            if (ImGui::Selectable(partName(i).c_str(), i == r.selPart)) r.selPart = i;
                        ImGui::EndCombo();
                    }
                }
                PtMaterial& m = parts ? o.parts[r.selPart] : o.material;
                const bool textured = o.type == PtObjectType::Project || o.type == PtObjectType::Sx3d || (parts && m.texSet >= 0);
                ImGui::BeginDisabled(!textured);
                ImGui::Checkbox(o.type == PtObjectType::Project ? "Textures peintes du projet" : o.type == PtObjectType::Sx3d ? "Textures peintes (.sx3d)" : "Texture du modèle", &m.useProject);
                ImGui::EndDisabled();
                ui::HelpTooltip(o.type == PtObjectType::File ? "Texture de couleur du matériau importé (FBX, glTF, OBJ .mtl), via les UV du modèle."
                                                             : "Couleur, rugosité, métal, émissif et normale (relief compris) viennent des canaux peints, via les UV du maillage.");
                if (ImGui::BeginCombo("Préréglage", "Choisir...")) {
                    for (const MaterialPreset& p : kPresets)
                        if (ImGui::Selectable(p.name)) {
                            m.baseColor = p.color;
                            m.roughness = p.rough;
                            m.metallic = p.metal;
                            m.transmission = p.transmission;
                            m.ior = p.ior;
                            m.emission = p.emission > 0 ? p.color : float3{0, 0, 0};
                            m.emissionStrength = p.emission;
                            m.coat = p.coat;
                            m.coatRoughness = 0.05f;
                            m.useProject = false;
                        }
                    ImGui::EndCombo();
                }
                ImGui::BeginDisabled(m.useProject);
                ColorLinear("Couleur", m.baseColor);
                ImGui::SliderFloat("Rugosité", &m.roughness, 0, 1);
                ImGui::SliderFloat("Métal", &m.metallic, 0, 1);
                ImGui::EndDisabled();
                ImGui::SliderFloat("Vernis", &m.coat, 0, 1);
                ui::HelpTooltip("Couche transparente brillante par-dessus (émail, laque, bois verni) :\nreflets nets sur une base mate ou colorée. S'applique aussi aux textures du projet.");
                if (m.coat > 0) ImGui::SliderFloat("Rugosité du vernis", &m.coatRoughness, 0, 1);
                ImGui::SliderFloat("Transmission", &m.transmission, 0, 1);
                ui::HelpTooltip("Verre / matière transparente (réfraction)");
                if (m.transmission > 0) ImGui::SliderFloat("Indice (IOR)", &m.ior, 1.0f, 2.5f);
                ColorLinear("Émission", m.emission);
                ImGui::DragFloat("Intensité émission", &m.emissionStrength, 0.05f, 0, 1000, "%.2f");
            }
        }
    }
    // ---------------------------------------------------------------- lights
    if (ImGui::CollapsingHeader("Lumières", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto add = [&](PtLightType t) {
            PtLight l;
            l.type = t;
            l.name = kLightTypes[(int)t];
            l.position = {1.8f, 2.2f, -1.4f};
            if (t == PtLightType::Point) l.intensity = 120, l.size = 0.05f;
            if (t == PtLightType::Spot) l.intensity = 300, l.size = 0.05f;
            if (t == PtLightType::Area) l.intensity = 60, l.size = 1.0f;
            if (t == PtLightType::Sun) l.intensity = 3, l.size = 0.6f;
            Aim(l, r.target);
            r.lights.push_back(l);
            r.selLight = (int)r.lights.size() - 1;
        };
        for (int t = 0; t < 4; ++t) {
            if (t) ImGui::SameLine();
            char b[48];
            snprintf(b, sizeof(b), "+ %s", kLightTypes[t]);
            if (ImGui::SmallButton(b)) add((PtLightType)t);
        }
        ImGui::BeginChild("##ptlights", ImVec2(0, std::min(6.0f, (float)r.lights.size() + 0.5f) * ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders);
        for (int i = 0; i < (int)r.lights.size(); ++i) {
            ImGui::PushID(1000 + i);
            ImGui::Checkbox("##vis", &r.lights[i].visible);
            ImGui::SameLine();
            if (ImGui::Selectable(r.lights[i].name.c_str(), r.selLight == i)) r.selLight = i;
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (r.selLight >= 0 && r.selLight < (int)r.lights.size()) {
            PtLight& l = r.lights[r.selLight];
            if (ImGui::SmallButton("Viser le modèle")) Aim(l, r.target);
            ImGui::SameLine();
            if (ImGui::SmallButton("Supprimer##light")) {
                r.lights.erase(r.lights.begin() + r.selLight);
                r.selLight = -1;
            } else {
                ImGui::InputText("Nom##light", &l.name);
                int type = (int)l.type;
                if (ImGui::Combo("Type", &type, "Ponctuelle\0Spot\0Surfacique\0Soleil\0")) l.type = (PtLightType)type;
                if (l.type != PtLightType::Sun) ImGui::DragFloat3("Position##light", &l.position.x, 0.01f);
                if (l.type != PtLightType::Point) {
                    ImGui::SliderFloat("Orientation", &l.yaw, -180, 180, "%.0f°");
                    ImGui::SliderFloat("Inclinaison", &l.pitch, -90, 90, "%.0f°");
                }
                ColorLinear("Couleur##light", l.color);
                ImGui::DragFloat("Intensité", &l.intensity, l.intensity * 0.01f + 0.01f, 0, 100000, "%.2f", ImGuiSliderFlags_Logarithmic);
                const char* sizeLabel = l.type == PtLightType::Area ? "Taille" : l.type == PtLightType::Sun ? "Angle du disque" : "Rayon";
                ImGui::DragFloat(sizeLabel, &l.size, 0.005f, 0.0f, l.type == PtLightType::Sun ? 20.0f : 20.0f, "%.3f");
                ui::HelpTooltip("Plus la source est grande, plus les ombres sont douces.");
                if (l.type == PtLightType::Spot) ImGui::SliderFloat("Ouverture", &l.spotAngle, 2, 170, "%.0f°");
            }
        }
    }
    // ---------------------------------------------------------------- environment
    if (ImGui::CollapsingHeader("Environnement (HDRI)", ImGuiTreeNodeFlags_DefaultOpen)) {
        const bool builtin = !s.hdri.empty() && s.hdri[0] == '*';
        int mode = builtin ? 0 : s.hdri.empty() ? 2 : 1;
        std::string fileLabel = mode == 1 ? Narrow(Utf8Path(s.hdri).filename().wstring()) : std::string("Fichier HDRI...");
        std::string cur = "Intégré...";
        for (auto& [id, label] : ProceduralEnvironments()) if (id == s.hdri) cur = label;
        ImGui::SetNextItemWidth(200 * dpi);
        if (ImGui::BeginCombo("##envbuiltin", cur.c_str())) {
            for (auto& [id, label] : ProceduralEnvironments())
                if (ImGui::Selectable(label.c_str(), id == s.hdri)) s.hdri = id;
            ImGui::EndCombo();
        }
        ui::HelpTooltip("Environnements intégrés : studio photo, ciel couvert, plein soleil, coucher de soleil, néons.");
        ImGui::SameLine();
        if (ImGui::RadioButton("Aucun", mode == 2)) s.hdri.clear();
        if (ImGui::Button(fileLabel.c_str())) {
            std::string path = platform::OpenFileDialog(hwnd, L"Environnement HDR (*.hdr)\0*.hdr\0Images\0*.png;*.jpg;*.exr\0");
            if (!path.empty()) {
                s.hdri = path;
                r.message.clear();
            }
        }
        ImGui::DragFloat("Intensité##env", &s.envIntensity, 0.01f, 0, 100, "%.2f");
        ImGui::SliderFloat("Rotation##env", &s.envRotation, -180, 180, "%.0f°");
        ImGui::Checkbox("Visible en fond", &s.envVisible);
        ColorLinear("Fond", s.background);
        ui::HelpTooltip("Couleur de fond sans HDRI, ou quand l'HDRI n'est pas visible");
    }
    // ---------------------------------------------------------------- camera
    if (ImGui::CollapsingHeader("Caméra")) {
        float fovDeg = s.fov * 57.29578f;
        ImGui::Checkbox("Orthographique", &s.ortho);
        ui::HelpTooltip("Projection parallèle (plans techniques, vues de face / côté / dessus) : la taille de la vue suit la distance et le champ de vision.");
        if (ImGui::SliderFloat("Champ de vision", &fovDeg, 10, 100, "%.0f°")) s.fov = fovDeg / 57.29578f;
        ImGui::DragFloat("Ouverture (flou)", &s.aperture, 0.001f, 0, 1, "%.3f");
        ui::HelpTooltip("Profondeur de champ : rayon de l'objectif. 0 = tout est net.");
        ImGui::BeginDisabled(s.aperture <= 0);
        ImGui::DragFloat("Mise au point", &s.focusDistance, 0.01f, 0.05f, 100, "%.2f");
        if (ImGui::SmallButton("Mise au point sur le modèle")) s.focusDistance = r.distance;
        ImGui::EndDisabled();
        ImGui::DragFloat("Distance", &r.distance, 0.01f, 0.2f, 100);
    }
    // ---------------------------------------------------------------- settings
    if (ImGui::CollapsingHeader("Réglages du rendu", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Combo("Calcul", &s.device, "GPU (carte graphique)\0CPU (processeur)\0");
        ImGui::DragInt("Échantillons", &s.maxSamples, 4, 1, 100000);
        ImGui::SliderInt("Rebonds", &s.maxBounces, 1, 32);
        ImGui::Checkbox("Taille de la vue", &s.viewSize);
        if (!s.viewSize) {
            ImGui::DragInt("Largeur", &s.width, 1, 16, 8192);
            ImGui::DragInt("Hauteur", &s.height, 1, 16, 8192);
            if (ImGui::BeginCombo("Format", "Choisir...")) {
                const int presets[][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {1080, 1080}, {2048, 2048}, {1080, 1350}};
                for (auto& p : presets) {
                    char b[32];
                    snprintf(b, sizeof(b), "%d x %d", p[0], p[1]);
                    if (ImGui::Selectable(b)) { s.width = p[0]; s.height = p[1]; }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::SliderFloat("Exposition", &s.exposure, -6, 6, "%.2f IL");
        ImGui::Combo("Courbe", &s.tonemap, "Linéaire\0ACES (film)\0Reinhard\0PBR Neutral (couleurs fidèles)\0AgX (film, naturel)\0");
        ui::HelpTooltip("PBR Neutral : les couleurs des matières restent celles peintes (rendu produit).\nAgX : courbe de film douce, hautes lumières naturelles.");
        ImGui::SliderFloat("Débruitage", &s.denoise, 0, 1, s.denoise > 0 ? "%.2f" : "désactivé");
        ui::HelpTooltip("Lisse le bruit du lancer de rayons en gardant les contours ; il s'efface à mesure que les échantillons s'accumulent.");
        ImGui::DragFloat("Limite indirecte", &s.clampIndirect, 0.5f, 0, 1000, "%.1f");
        ui::HelpTooltip("Écrête les reflets indirects très forts (lucioles). 0 = sans limite.");
        // Camera effects: applied to the image while it refines (no restart).
        PostSettings p = s.post;
        p.tonemap = s.tonemap;
        if (DrawPostSettings(p, false)) {
            s.tonemap = p.tonemap;
            s.post = p;
        }
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

}  // namespace sx
