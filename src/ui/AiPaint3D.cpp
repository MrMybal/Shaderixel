#include "ui/App.h"
#include "ui/AiSchema.h"
#include "doc/Brushes.h"
#include "render/MeshPaint.h"

#include <filesystem>

namespace sx {
namespace {

using ai::Range;
using ai::T;

constexpr const char* channels[] = {"base_color", "roughness", "metallic", "normal", "emissive", "occlusion",
                                    "height", "opacity", "user0", "user1", "user2", "user3"};
static_assert(std::size(channels) == static_cast<size_t>(PbrChannel::Count));

Json Camera(const MeshScene& mesh) {
    Json r = Json::Object();
    r["yaw"] = mesh.yaw;
    r["pitch"] = mesh.pitch;
    r["distance"] = mesh.distance;
    r["fov"] = mesh.fov;
    for (float v : {mesh.target.x, mesh.target.y, mesh.target.z}) r["target"].Push(v);
    r["ortho"] = mesh.ortho;
    r["wireframe"] = mesh.wireframe;
    r["quality"] = mesh.quality;
    r["viewChannel"] = mesh.viewChannel;
    return r;
}

Json ChannelNames(u32 mask) {
    Json result = Json::Array();
    for (size_t i = 0; i < std::size(channels); ++i) if (mask & (1u << i)) result.Push(channels[i]);
    return result;
}

std::optional<u32> ChannelMask(const Json& array) {
    if (!array.IsArray()) return {};
    u32 result = 0;
    for (const auto& item : array.Elements()) {
        const auto found = std::ranges::find(channels, item.S());
        if (found == std::end(channels)) return {};
        result |= 1u << (found - std::begin(channels));
    }
    return result;
}

bool Vector(const Json& j, size_t size, float low, float high) {
    return j.IsArray() && j.Size() == size && std::ranges::all_of(j.Elements(), [=](const auto& value) {
        return value.GetType() == Json::Type::Number && value.Num() >= low && value.Num() <= high;
    });
}

float Bounded(const Json& j, float fallback, float low, float high) {
    return static_cast<float>(std::clamp(j.Num(fallback), static_cast<double>(low), static_cast<double>(high)));
}

Json ViewPoints() {
    Json points = T("array", "Coordonnées normalisées [[x,y], ...] dans la vue 3D, origine en haut à gauche");
    points["minItems"] = 1;
    points["maxItems"] = 128;
    Json point = T("array");
    point["minItems"] = 2;
    point["maxItems"] = 2;
    point["items"] = Range("number", 0, 1);
    points["items"] = std::move(point);
    return points;
}

AiResult NoMesh() { return AiResult::Fail("NoMesh", "Ouvrez un projet ou un document avec un maillage"); }
}  // namespace

void App::RegisterAiPaint3D() {
    using namespace ai;
    bridge.Register({"shaderixel.paint3d_info", "Lit le maillage, les tuiles UDIM, la caméra, les canaux et les calques de matière. Utilisez avant de peindre ou de cibler un matériau.",
        Schema({}), false, {}, [this](const Json&, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return NoMesh();
            const auto& mesh = v->mesh->data;
            const auto& state = v->doc->S();
            Json r = Json::Object();
            r["mesh"] = mesh.name;
            r["vertices"] = static_cast<u32>(mesh.vertices.size());
            r["triangles"] = static_cast<u32>(mesh.indices.size() / 3);
            r["width"] = v->doc->Width(); r["height"] = v->doc->Height();
            r["camera"] = Camera(*v->mesh);
            r["channels"] = ChannelNames(v->doc->pbrChannels);
            r["activeLayer"] = state.activeLayer;
            r["maskEditing"] = state.editMask;
            r["udims"] = Json::Array();
            r["materials"] = Json::Array();
            r["presets"] = Json::Array();
            r["textureSets"] = Json::Array();
            for (auto udim : mesh.udims) r["udims"].Push(static_cast<u32>(udim));
            for (const auto& [set, region] : ExportParts(*v, true)) {
                Json row = Json::Object();
                row["name"] = set;
                row["width"] = region.Empty() ? v->doc->Width() : static_cast<u32>(region.Width());
                row["height"] = region.Empty() ? v->doc->Height() : static_cast<u32>(region.Height());
                row["channels"] = ChannelNames(v->doc->TextureSetChannels(set));
                r["textureSets"].Push(std::move(row));
            }
            for (Id id : MaterialIds(*v->doc)) {
                auto* layer = AnyMaterialLayer(*v->doc, id);
                if (!layer) continue;
                Json row = Json::Object();
                row["id"] = id; row["name"] = layer->name;
                row["kind"] = IsFolder3D(*v->doc, id) ? "folder" : IsPaintLayer3D(*v->doc, id) ? "paint" : "fill";
                row["visible"] = layer->visible;
                row["active"] = materialSel == id;
                row["folder"] = Folder3DOf(*v->doc, id);
                r["materials"].Push(std::move(row));
            }
            for (int i = 0; i < MaterialPresetCount(); ++i) r["presets"].Push(MaterialPresetName(i));
            return AiResult::Ok(std::move(r));
        }, Schema({{"camera", T("object")}, {"udims", T("array")}, {"materials", T("array")}, {"textureSets", T("array")}},
                  {"camera", "udims", "materials", "textureSets"})});

    bridge.Register({"shaderixel.paint3d_new", "Crée le même projet Paint 3D que le dialogue Nouveau. Accepte une primitive ou un fichier OBJ, FBX, GLTF/GLB, avec UV automatiques ou tuiles natives.",
        Schema({{"mesh", Enum({"sphere", "cube", "plane", "file"})}, {"path", T("string")}, {"name", T("string")},
                {"resolution", Range("integer", 1024, 4096)}, {"autoUv", T("boolean")}, {"perTile", T("boolean")},
                {"tileResolutions", T("string")}, {"channels", T("array")}, {"basePreset", Range("integer", 0, 4096)}}),
        true, {"path"}, [this](const Json& p, const std::string&) {
            const auto mesh = p["mesh"].S("sphere");
            const auto path = Utf8Path(p["path"].S());
            if (mesh == "file") {
                if (!path.is_absolute() || !std::filesystem::is_regular_file(path))
                    return AiResult::Fail("FileError", "Choisissez le chemin absolu d’un maillage existant");
                if (path.extension() == L".uasset")
                    return AiResult::Fail("InvalidParameters", "Utilisez la conversion Unreal dans le dialogue ou importez le GLB");
            }
            const auto resolution = p["resolution"].Num(1024);
            if (resolution != 1024 && resolution != 2048 && resolution != 4096)
                return AiResult::Fail("InvalidParameters", "Résolution attendue : 1024, 2048 ou 4096");
            auto options = decltype(p3dNew){};
            options.mesh = mesh == "file" ? 3 : mesh == "plane" ? 2 : mesh == "cube" ? 1 : 0;
            options.meshPath = p["path"].S();
            options.resolution = resolution == 4096 ? 2 : resolution == 2048 ? 1 : 0;
            options.autoUv = p["autoUv"].B();
            options.resolutionPerTile = p["perTile"].B();
            options.tileResolutions = p["tileResolutions"].S();
            options.basePreset = p["basePreset"].I(1);
            if (options.basePreset >= MaterialPresetCount())
                return AiResult::Fail("InvalidParameters", "Préréglage de matière inconnu");
            if (p.Has("channels")) {
                const auto mask = ChannelMask(p["channels"]);
                if (!mask) return AiResult::Fail("InvalidParameters", "Nom de canal inconnu");
                for (size_t i = 0; i < std::size(channels); ++i) options.channels[i] = (*mask & (1u << i)) != 0;
            }
            const auto previous = p3dNew;
            const size_t count = docs.size();
            p3dNew = std::move(options);
            try { NewPaint3DProject(); }
            catch (...) { p3dNew = previous; throw; }
            p3dNew = previous;
            if (docs.size() == count || !Active() || !Active()->mesh)
                return AiResult::Fail("CreateFailed", statusMsg);
            auto& d = *Active()->doc;
            if (!p["name"].S().empty()) d.SetName(p["name"].S());
            Json r = Json::Object();
            r["name"] = d.Name();
            r["width"] = d.Width(); r["height"] = d.Height();
            r["index"] = activeDoc;
            return AiResult::Ok(std::move(r));
        }});

    bridge.Register({"shaderixel.paint3d_camera", "Règle la caméra orbitale de la vue 3D. Angles en radians. Utilisez un preset ou réglez la projection, le filaire, la qualité et le canal affiché.",
        Schema({{"preset", Enum({"front", "back", "left", "right", "top", "bottom"})}, {"yaw", Range("number", -20, 20)},
                {"pitch", Range("number", -1.5695, 1.5695)}, {"fov", Range("number", 0.1, 2.3)}, {"distance", Range("number", 0.2, 50)},
                {"target", T("array")}, {"ortho", T("boolean")}, {"wireframe", T("boolean")},
                {"quality", Range("integer", 0, 2)}, {"viewChannel", Range("integer", 0, 16)}}),
        true, {}, [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return NoMesh();
            if (p.Has("target") && !Vector(p["target"], 3, -100, 100))
                return AiResult::Fail("InvalidParameters", "target attend trois coordonnées finies");
            auto& s = *v->mesh;
            const char* presets[] = {"front", "back", "left", "right", "top", "bottom"};
            const auto found = std::ranges::find(presets, p["preset"].S());
            if (found != std::end(presets)) SetView3DPreset(s, static_cast<int>(found - std::begin(presets)));
            s.yaw = Bounded(p["yaw"], s.yaw, -20, 20);
            s.pitch = Bounded(p["pitch"], s.pitch, -1.5695f, 1.5695f);
            s.distance = Bounded(p["distance"], s.distance, 0.2f, 50);
            s.fov = Bounded(p["fov"], s.fov, 0.1f, 2.3f);
            s.ortho = p["ortho"].B(s.ortho);
            s.wireframe = p["wireframe"].B(s.wireframe);
            s.quality = std::clamp(p["quality"].I(s.quality), 0, 2);
            s.viewChannel = std::clamp(p["viewChannel"].I(s.viewChannel), 0, 16);
            if (p.Has("target")) s.target = {p["target"].At(0).F(), p["target"].At(1).F(), p["target"].At(2).F()};
            return AiResult::Ok(Camera(s));
        }});

    bridge.Register({"shaderixel.paint3d_environment", "Charge un environnement HDRI dans la vue 3D ou modifie son intensité, sa rotation et son arrière-plan.",
        Schema({{"path", T("string")}, {"intensity", Range("number", 0, 20)}, {"rotation", Range("number", -20, 20)},
                {"background", T("boolean")}}), true, {"path"}, [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return NoMesh();
            auto& s = *v->mesh;
            const auto path = p["path"].S();
            if (!path.empty()) {
                std::string error;
                if (!Utf8Path(path).is_absolute()) return AiResult::Fail("InvalidParameters", "Chemin HDRI absolu requis");
                if (!LoadEnvironment(s, path, &error)) return AiResult::Fail("FileError", error);
            }
            s.envIntensity = Bounded(p["intensity"], s.envIntensity, 0, 20);
            s.envRotation = Bounded(p["rotation"], s.envRotation, -20, 20);
            s.envBackground = p["background"].B(s.envBackground);
            Json r = Json::Object();
            r["name"] = s.envName; r["intensity"] = s.envIntensity; r["rotation"] = s.envRotation;
            return AiResult::Ok(std::move(r));
        }});

    bridge.Register({"shaderixel.paint3d_brush", "Configure le matériau du pinceau 3D, les canaux et la projection. Utilisez paint3d_stroke pour appliquer le pinceau ou ui_input pour les outils interactifs.",
        Schema({{"size", Range("number", 1, 2048)}, {"flow", Range("number", 0, 1)}, {"opacity", Range("number", 0, 1)},
                {"hardness", Range("number", 0, 1)}, {"baseColor", T("array")}, {"roughness", Range("number", 0, 1)},
                {"metallic", Range("number", 0, 1)}, {"height", Range("number", 0, 1)}, {"occlusion", Range("number", 0, 1)},
                {"channels", T("array")}, {"normalProject", T("boolean")}, {"tip", T("boolean")}}),
        true, {}, [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return NoMesh();
            if (sim.view || meshStroking || fluidRun.active) return AiResult::Fail("Busy", "Terminez le geste en cours");
            if (p.Has("baseColor") && !Vector(p["baseColor"], 3, 0, 1) && !Vector(p["baseColor"], 4, 0, 1))
                return AiResult::Fail("InvalidParameters", "baseColor attend RGB ou RGBA en sRGB 0..1");
            if (p.Has("channels")) {
                const auto mask = ChannelMask(p["channels"]);
                if (!mask || !*mask) return AiResult::Fail("InvalidParameters", "Choisissez des canaux valides");
                for (size_t i = 0; i < std::size(channels); ++i) brush3D.use[i] = (*mask & (1u << i)) != 0;
            }
            if (p.Has("baseColor"))
                fg = {p["baseColor"].At(0).F(), p["baseColor"].At(1).F(), p["baseColor"].At(2).F(), p["baseColor"].At(3).F(1)};
            brush.size = Bounded(p["size"], brush.size, 1, 2048);
            brush.hardness = Bounded(p["hardness"], brush.hardness, 0, 1);
            brush.opacity = Bounded(p["opacity"], brush.opacity, 0, 1);
            brush.flow = Bounded(p["flow"], brush.flow, 0, 1);
            brush3D.rough = Bounded(p["roughness"], brush3D.rough, 0, 1);
            brush3D.metal = Bounded(p["metallic"], brush3D.metal, 0, 1);
            brush3D.height = Bounded(p["height"], brush3D.height, 0, 1);
            brush3D.occlusion = Bounded(p["occlusion"], brush3D.occlusion, 0, 1);
            brush3D.tip = p["tip"].B(brush3D.tip);
            brush3D.normalProject = p["normalProject"].B(brush3D.normalProject);
            Json output = Json::Object();
            output["size"] = brush.size; output["flow"] = brush.flow; output["opacity"] = brush.opacity;
            output["hardness"] = brush.hardness;
            u32 mask = 0;
            for (size_t i = 0; i < std::size(channels); ++i) if (brush3D.use[i]) mask |= 1u << i;
            output["channels"] = ChannelNames(mask);
            output["normalProject"] = brush3D.normalProject;
            return AiResult::Ok(std::move(output));
        }});

    bridge.Register({"shaderixel.paint3d_material", "Ajoute, sélectionne, renomme ou masque un calque de matière du projet Paint 3D. Activez un calque paint avant un geste.",
        Schema({{"action", Enum({"paint", "preset", "fill", "select", "update"})}, {"id", Range("integer", 1, 4294967295.0)},
                {"preset", Range("integer", 0, 4096)}, {"name", T("string")}, {"visible", T("boolean")},
                {"baseColor", T("array")}, {"roughness", Range("number", 0, 1)}, {"metallic", Range("number", 0, 1)},
                {"height", Range("number", 0, 1)}, {"fullMask", T("boolean")}}, {"action"}),
        true, {}, [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh || v->doc->kind != 1) return NoMesh();
            if (sim.view || meshStroking || fluidRun.active) return AiResult::Fail("Busy", "Terminez le geste en cours");
            auto& d = *v->doc;
            const auto action = p["action"].S();
            const auto name = p["name"].S();
            Id id = p["id"].U();
            if (action == "paint") {
                id = AddPaintLayer3D(*v, name.empty() ? "Peinture" : name);
            } else if (action == "preset") {
                const int preset = p["preset"].I(-1);
                if (preset < 0 || preset >= MaterialPresetCount()) return AiResult::Fail("InvalidParameters", "Préréglage inconnu");
                AddMaterialLayer(*v, preset);
                id = materialSel;
            } else if (action == "fill") {
                const auto& color = p["baseColor"];
                if (p.Has("baseColor") && !Vector(color, 3, 0, 1)) return AiResult::Fail("InvalidParameters", "baseColor attend RGB 0..1");
                if (!p.Has("baseColor") && !p.Has("roughness") && !p.Has("metallic") && !p.Has("height"))
                    return AiResult::Fail("InvalidParameters", "Indiquez au moins une valeur de matière");
                const float3 rgb{color.At(0).F(), color.At(1).F(), color.At(2).F()};
                const float rough = Bounded(p["roughness"], 0.5f, 0, 1), metal = Bounded(p["metallic"], 0, 0, 1),
                            height = Bounded(p["height"], 0.5f, 0, 1);
                id = AddMaterialLayerValues(*v, name.empty() ? "Matière" : name,
                    p.Has("baseColor") ? &rgb : nullptr, p.Has("roughness") ? &rough : nullptr,
                    p.Has("metallic") ? &metal : nullptr, p.Has("height") ? &height : nullptr, p["fullMask"].B(true));
            } else {
                if (!AnyMaterialLayer(d, id)) return AiResult::Fail("InvalidParameters", "Calque de matière introuvable");
                if (action == "select") SelectMaterial(d, id);
                else if (action == "update") {
                    Edit edit(d, v->history, "Matière 3D");
                    for (auto& layer : d.S().layers) if (layer.material == id) {
                        if (!name.empty()) layer.name = name;
                        if (p.Has("visible")) layer.visible = p["visible"].B();
                    }
                    d.SyncGraph();
                    edit.Commit();
                } else return AiResult::Fail("InvalidParameters", "Action de matière inconnue");
            }
            if (!id || !AnyMaterialLayer(d, id)) return AiResult::Fail("NotReady", "Aucun canal de matière compatible");
            Json result = Json::Object();
            result["id"] = id;
            result["activeLayer"] = d.S().activeLayer;
            return AiResult::Ok(std::move(result));
        }});

    bridge.Register({"shaderixel.paint3d_stroke", "Peint ou gomme un trait sur le maillage depuis la caméra actuelle. points contient [x,y] normalisés dans la vue. Le geste multicanal passe par le moteur de brosse 3D et est annulable.",
        Schema({{"points", ViewPoints()}, {"viewWidth", Range("integer", 128, 4096)}, {"viewHeight", Range("integer", 128, 4096)},
                {"erase", T("boolean")}, {"diameter", Range("number", 1, 2048)}}, {"points"}), true, {},
        [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return NoMesh();
            if (sim.view || meshStroking || fluidRun.active) return AiResult::Fail("Busy", "Terminez le geste en cours");
            const auto w = static_cast<u32>(std::clamp(p["viewWidth"].Num(1024), 128.0, 4096.0));
            const auto h = static_cast<u32>(std::clamp(p["viewHeight"].Num(1024), 128.0, 4096.0));
            std::vector<float2> points;
            for (const auto& point : p["points"].Elements()) {
                if (!Vector(point, 2, 0, 1)) return AiResult::Fail("InvalidParameters", "Point de vue incorrect");
                points.push_back({point.At(0).F() * w, point.At(1).F() * h});
            }
            if (points.empty() || points.size() > 128) return AiResult::Fail("InvalidParameters", "Le trait doit contenir de 1 à 128 points");
            auto& mesh = *v->mesh;
            mesh.UpdateCamera(static_cast<float>(w) / h);
            if (std::ranges::none_of(points, [&](auto point) { return PickMeshTriangle(mesh, point, w, h) >= 0; }))
                return AiResult::Fail("NoSurface", "Le trait ne touche pas le modèle");
            auto& renderer = v->renderer;
            auto& d = *v->doc;
            std::vector<float4> colors;
            auto surfaces = Paint3DTargets(*v, colors);
            if (surfaces.empty()) return AiResult::Fail("NoPaintLayer", "Sélectionnez un calque et des canaux de peinture");
            bool erase = p["erase"].B();
            BrushSettings b = erase ? eraser : brush;
            b.size = Bounded(p["diameter"], b.size, 1, 2048);
            if (d.PaintsMaskEdits()) {
                for (auto& color : colors) color = erase ? float4{0, 0, 0, 1} : float4{1, 1, 1, 1};
                erase = false;
            }
            auto tip = brush3D.tip ? TipDef(b) : nullptr;
            const float relative = tip && tip->spacing > 0 ? tip->spacing : b.spacing;
            const float spacing = std::max(1.0f, b.size * std::max(0.02f, relative));
            const float used = relative <= 0.35f ? 1.0f : spacing;
            const float flow = 1 - std::pow(std::max(0.0f, 1 - b.flow), used / spacing);
            std::vector<Dab> dabs;
            auto add = [&](float2 point) { dabs.push_back({point.x, point.y, b.size * 0.5f, flow}); };
            add(points.front());
            float remainder = 0;
            for (size_t i = 1; i < points.size(); ++i) {
                const float2 segment = points[i] - points[i - 1];
                const float length = Length(segment);
                if (length < 1e-4f) continue;
                float t = used - remainder;
                for (; t <= length; t += used) {
                    if (dabs.size() >= 10000) return AiResult::Fail("InvalidParameters", "Divisez ce geste en traits plus courts");
                    add(points[i - 1] + segment * (t / length));
                }
                remainder = std::max(0.0f, length - (t - used));
            }
            if (!renderer.MeshPrepare(d, *ctx, mesh) || !renderer.MeshRender(*ctx, mesh, GatherMaterial(*v), w, h))
                return AiResult::Fail("NotReady", "Le rendu du maillage est indisponible");
            const bool oldTip = renderer.meshTipOn, oldStencil = renderer.meshStencilOn, oldNormal = renderer.meshNormalMode;
            const Rect oldClip = renderer.meshClip;
            auto restore = [&] {
                renderer.meshTipOn = oldTip; renderer.meshStencilOn = oldStencil;
                renderer.meshNormalMode = oldNormal; renderer.meshClip = oldClip;
            };
            renderer.meshTipOn = false;
            if (tip) {
                EffectInstance fx;
                fx.params = b.tipParams;
                BindTipTextures(b, *tip);
                renderer.meshTipOn = renderer.BuildMeshTip(*ctx, *tip, fx, b);
                if (!renderer.meshTipOn) {
                    restore();
                    return AiResult::Fail("NotReady", "La pointe du pinceau est en cours de compilation");
                }
            }
            if (!renderer.BeginStrokeMulti(d, *ctx, surfaces)) {
                restore();
                return AiResult::Fail("NotReady", "Impossible de préparer le calque de peinture");
            }
            renderer.meshNormalMode = brush3D.normalProject;
            renderer.meshStencilOn = false;
            renderer.meshClip = {};
            renderer.MeshStrokeDabsMulti(d, *ctx, mesh, dabs, b, colors, erase);
            renderer.EndStrokeMulti(d, *ctx, v->history, "Peinture 3D");
            restore();
            Json r = Json::Object();
            r["dabs"] = static_cast<u32>(dabs.size());
            r["surfaces"] = static_cast<u32>(surfaces.size());
            return AiResult::Ok(std::move(r));
        }, Schema({{"dabs", T("integer")}, {"surfaces", T("integer")}}, {"dabs", "surfaces"})});

    bridge.Register({"shaderixel.paint3d_texture_set", "Règle les canaux d’une tuile UDIM ou d’un jeu de textures nommé. Consultez textureSets dans paint3d_info, puis spécifiez channels ou inherit.",
        Schema({{"name", T("string")}, {"channels", T("array")}, {"inherit", T("boolean")}}, {"name"}),
        true, {}, [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh || v->doc->kind != 1) return NoMesh();
            if (sim.view || meshStroking || fluidRun.active) return AiResult::Fail("Busy", "Terminez le geste en cours");
            const auto name = p["name"].S();
            const auto parts = ExportParts(*v, true);
            if (name.empty() || std::ranges::none_of(parts, [&](const auto& part) { return part.first == name; }))
                return AiResult::Fail("InvalidParameters", "Tuile ou jeu de textures introuvable");
            const bool inherit = p["inherit"].B();
            const auto mask = ChannelMask(p["channels"]);
            if (!inherit && !mask) return AiResult::Fail("InvalidParameters", "Fournissez les canaux ou choisissez inherit");
            SetTextureSetChannels(*v, name, mask.value_or(0), inherit);
            Json result = Json::Object();
            result["channels"] = ChannelNames(v->doc->TextureSetChannels(name));
            result["name"] = name;
            return AiResult::Ok(std::move(result));
        }});

    bridge.Register({"shaderixel.paint3d_bake", "Démarre, inspecte ou annule la cuisson des cartes du maillage. highPath et cagePath sont facultatifs pour le baker.",
        Schema({{"action", Enum({"start", "status", "cancel"})}, {"kind", Enum({"textures", "smart_masks"})},
                {"size", Range("integer", 512, 4096)}, {"highPath", T("string")}, {"cagePath", T("string")}}, {"action"}),
        true, {"highPath", "cagePath"}, [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return NoMesh();
            const auto action = p["action"].S();
            const bool smart = p["kind"].S() == "smart_masks";
            if (action == "start") {
                if (smart ? mapsBake.running : baker.running) return AiResult::Fail("Busy", "Une cuisson est déjà en cours");
                const int res = p["size"].I(1024);
                if (res != 512 && res != 1024 && res != 2048 && res != 4096)
                    return AiResult::Fail("InvalidParameters", "Taille attendue : 512, 1024, 2048 ou 4096");
                if (smart) {
                    const auto old = mapsResolution;
                    mapsResolution = res;
                    StartMeshMapsBake(*v);
                    mapsResolution = old;
                } else {
                    const auto high = p["highPath"].S(), cage = p["cagePath"].S();
                    if ((!high.empty() && !Utf8Path(high).is_absolute()) || (!cage.empty() && (!Utf8Path(cage).is_absolute() || high.empty())))
                        return AiResult::Fail("InvalidParameters", "Chemins de cuisson absolus requis. Une cage nécessite un modèle haute définition.");
                    baker.useHigh = !high.empty();
                    baker.highPath = high;
                    baker.cagePath = cage;
                    baker.sizeIndex = res == 4096 ? 3 : res == 2048 ? 2 : res == 1024 ? 1 : 0;
                    baker.exportPng = false;
                    StartBake(*v);
                    if (!baker.running) return AiResult::Fail("BakeFailed", statusMsg);
                }
            } else if (action == "cancel") {
                const auto cancel = smart ? mapsBake.cancel : baker.cancel;
                if (cancel) cancel->store(true);
            }
            const auto progress = smart ? mapsBake.progress : baker.progress;
            Json result = Json::Object();
            result["running"] = smart ? mapsBake.running : baker.running;
            result["progress"] = progress ? progress->load() : 0.f;
            return AiResult::Ok(std::move(result));
        }});
}

}  // namespace sx
