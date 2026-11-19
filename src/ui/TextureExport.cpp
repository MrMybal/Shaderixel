// Texture export of a Paint 3D project (Substance-like export presets): each preset
// lists its maps and what goes in each of their R, G, B, A (channels, packed masks,
// smoothness, constants). Options: PNG 8 / 16 bits or TGA, size, normal convention
// (DirectX / OpenGL), padding past the UV islands, file name pattern. The channels
// are read back from the GPU, then assembled and written on a worker thread.
#include "ui/App.h"
#include "ui/AiSchema.h"
#include "ui/Filter.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "io/WicLoad.h"

#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>
#include <stb_image_write.h>

namespace sx {

namespace {

using texexport::Map;
using texexport::Preset;

const std::vector<Preset>& Presets() {
    static const std::vector<Preset> p = {
        {"Unreal Engine (ORM)", "BaseColor, Normal (DirectX), ORM (R occlusion, G rugosité, B métal), Emissive.", false,
         {{"BaseColor", {"C.r", "C.g", "C.b", "O"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"ORM", {"AO", "R", "M", "1"}}, {"Emissive", {"E.r", "E.g", "E.b", "1"}}}},
        {"Unity HDRP", "BaseMap, Normal (OpenGL), MaskMap (R métal, G occlusion, B détail, A lissé).", true,
         {{"BaseMap", {"C.r", "C.g", "C.b", "O"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"MaskMap", {"M", "AO", "0", "S"}}, {"Emissive", {"E.r", "E.g", "E.b", "1"}}}},
        {"Unity URP / Built-in (métal)", "BaseMap, Normal (OpenGL), MetallicSmoothness (R métal, A lissé), Occlusion.", true,
         {{"BaseMap", {"C.r", "C.g", "C.b", "O"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"MetallicSmoothness", {"M", "M", "M", "S"}},
          {"Occlusion", {"AO", "AO", "AO", "1"}}, {"Emission", {"E.r", "E.g", "E.b", "1"}}}},
        {"glTF / Blender (ORM)", "BaseColor, Normal (OpenGL), OcclusionRoughnessMetallic (R occlusion, G rugosité, B métal), Emissive.", true,
         {{"BaseColor", {"C.r", "C.g", "C.b", "O"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"OcclusionRoughnessMetallic", {"AO", "R", "M", "1"}},
          {"Emissive", {"E.r", "E.g", "E.b", "1"}}}},
        {"Godot (ORM)", "Albedo, Normal (OpenGL), ORM (R occlusion, G rugosité, B métal), Emission.", true,
         {{"Albedo", {"C.r", "C.g", "C.b", "O"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"ORM", {"AO", "R", "M", "1"}}, {"Emission", {"E.r", "E.g", "E.b", "1"}}}},
        {"Textures séparées", "Une image par canal : couleur, rugosité, métal, normale (DirectX), émissif, occlusion, hauteur.", false,
         {{"BaseColor", {"C.r", "C.g", "C.b", "O"}}, {"Roughness", {"R", "R", "R", "1"}}, {"Metallic", {"M", "M", "M", "1"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}},
          {"Emissive", {"E.r", "E.g", "E.b", "1"}}, {"AO", {"AO", "AO", "AO", "1"}}, {"Height", {"H", "H", "H", "1"}}, {"Opacity", {"O", "O", "O", "1"}},
           {"User0", {"U0", "U0", "U0", "1"}}, {"User1", {"U1", "U1", "U1", "1"}},
           {"User2", {"U2", "U2", "U2", "1"}}, {"User3", {"U3", "U3", "U3", "1"}}}},
    };
    return p;
}

struct Source {
    const char* id;
    const char* label;
    PbrChannel channel;
    int component;
    float fallback;
};
using C = PbrChannel;
u32 Bit(C c) { return 1u << (int)c; }
const Source sources[] = {
    {"C.r","Couleur R",C::BaseColor,0,0.5f}, {"C.g","Couleur V",C::BaseColor,1,0.5f}, {"C.b","Couleur B",C::BaseColor,2,0.5f},
    {"C.a","Couverture",C::BaseColor,3,1}, {"R","Rugosité",C::Roughness,0,0.5f}, {"S","Lissé",C::Roughness,0,0.5f},
    {"M","Métal",C::Metallic,0,0}, {"AO","Occlusion",C::Occlusion,0,1}, {"H","Hauteur",C::Height,0,0.5f},
    {"N.r","Normale X",C::Normal,0,0.5f}, {"N.g","Normale Y",C::Normal,1,0.5f}, {"N.b","Normale Z",C::Normal,2,1},
    {"E.r","Émissif R",C::Emissive,0,0}, {"E.g","Émissif V",C::Emissive,1,0}, {"E.b","Émissif B",C::Emissive,2,0},
    {"O","Opacité",C::Opacity,0,1}, {"U0","Utilisateur 1",C::User0,0,0}, {"U1","Utilisateur 2",C::User1,0,0},
    {"U2","Utilisateur 3",C::User2,0,0}, {"U3","Utilisateur 4",C::User3,0,0}, {"1","1",C::Count,0,1}, {"0","0",C::Count,0,0}
};
const Source* FindSource(const std::string& id) {
    for (const auto& s : sources) if (id == s.id) return &s;
    return nullptr;
}

bool ValidPreset(const Preset& p) {
    if (p.name.empty() || p.maps.empty()) return false;
    for (const auto& m : p.maps) {
        if (m.suffix.empty()) return false;
        for (const auto& s : m.src) if (!FindSource(s)) return false;
    }
    return true;
}

bool ValidReadback(const gpu::Readback& r, int w, int h) {
    const size_t size = r.format == gpu::Format::RGBA32F ? 16 : r.format == gpu::Format::RGBA16F || r.format == gpu::Format::RGBA16 ? 8 :
                        r.format == gpu::Format::RGBA8 ? 4 : 0;
    return size && r.width == (u32)w && r.height == (u32)h && r.data.size() == (size_t)w * h * size;
}

float Read(const gpu::Readback& r, size_t pixel, int c) {
    const size_t i = pixel * 4 + c;
    switch (r.format) {
        case gpu::Format::RGBA32F: return reinterpret_cast<const float*>(r.data.data())[i];
        case gpu::Format::RGBA16F: return HalfToFloat(reinterpret_cast<const u16*>(r.data.data())[i]);
        case gpu::Format::RGBA16: return reinterpret_cast<const u16*>(r.data.data())[i] / 65535.0f;
        default: return r.data[i] / 255.0f;
    }
}

struct ExportComponent {
    const gpu::Readback* image;
    int component;
    float fallback;
    bool invert;
    float Sample(size_t p) const {
        float value = fallback;
        if (image) {
            const float a = Read(*image, p, 3);
            if (component == 3) value = std::clamp(a, 0.0f, 1.0f);
            else if (a > 1e-4f) value = std::clamp(LinearToSrgb(std::max(0.0f, Read(*image, p, component) / std::min(a, 1.0f))), 0.0f, 1.0f);
        }
        return invert ? 1.0f - value : value;
    }
};

std::vector<float> ResizeMap(std::vector<float> img, int w, int h, int ow, int oh) {
    if (w == ow && h == oh) return img;
    std::vector<float> out((size_t)ow * oh * 4);
    for (int y = 0; y < oh; ++y) for (int x = 0; x < ow; ++x) {
        const double x0 = (double)x * w / ow, x1 = (double)(x+1) * w / ow, y0 = (double)y * h / oh, y1 = (double)(y+1) * h / oh;
        float* dst = &out[((size_t)y * ow + x) * 4];
        for (int sy = (int)y0; sy < (int)std::ceil(y1); ++sy) for (int sx = (int)x0; sx < (int)std::ceil(x1); ++sx) {
            const float weight = (float)((std::min(x1, sx+1.0)-std::max(x0, (double)sx)) *
                                         (std::min(y1, sy+1.0)-std::max(y0, (double)sy)) / ((x1-x0)*(y1-y0)));
            for (int c = 0; c < 4; ++c) dst[c] += weight * img[((size_t)sy * w + sx) * 4 + c];
        }
    }
    return out;
}

bool EncodeMap(const std::string& path, const std::vector<float>& pixels, int w, int h, int depth, std::string& err) {
    if (depth == 1) {
        std::vector<u16> px(pixels.size());
        for (size_t i = 0; i < px.size(); ++i) px[i] = (u16)std::lround(std::clamp(pixels[i], 0.0f, 1.0f) * 65535.0f);
        return SavePng16(path, px, w, h, &err);
    }
    std::ofstream file(Utf8Path(path), std::ios::binary);
    if (!file) return false;
    std::vector<u8> px(pixels.size());
    for (size_t i = 0; i < px.size(); ++i) px[i] = (u8)std::lround(std::clamp(pixels[i], 0.0f, 1.0f) * 255.0f);
    auto write = [](void* f, void* data, int n) { static_cast<std::ofstream*>(f)->write(static_cast<const char*>(data), n); };
    const bool ok = depth == 2 ? stbi_write_tga_to_func(write, &file, w, h, 4, px.data()) != 0 :
                                 stbi_write_png_to_func(write, &file, w, h, 4, px.data(), w * 4) != 0;
    file.close();
    return ok && !file.fail();
}

bool WriteMap(const std::string& path, const std::vector<float>& pixels, int w, int h, int depth, std::string& err) {
    const auto dest = Utf8Path(path);
    std::error_code ec;
    std::filesystem::create_directories(dest.parent_path(), ec);
    if (ec) { err = ec.message(); return false; }
    static std::atomic<u64> serial{0};
    std::filesystem::path tmp;
    for (;;) {
        tmp = dest.parent_path() / ("shaderixel-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++serial) + ".tmp");
        HANDLE file = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) { CloseHandle(file); break; }
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            err = std::system_category().message(error);
            return false;
        }
    }
    bool ok = EncodeMap(Narrow(tmp.wstring()), pixels, w, h, depth, err);
    if (ok) {
        std::filesystem::rename(tmp, dest, ec);
        if (ec) { err = ec.message(); ok = false; }
    }
    if (!ok) std::filesystem::remove(tmp, ec);
    return ok;
}

}  // namespace

void App::LoadTextureExportPresets(const Json& settings) {
    customExportPresets.clear();
    for (const auto& j : settings.Elements()) {
        Preset p{j["name"].S(), "", j["opengl"].B(), {}};
        for (const auto& item : j["maps"].Elements()) {
            Map m{item["name"].S(), {}};
            const auto& ch = item["rgba"].Elements();
            if (ch.size() == 4) for (int i = 0; i < 4; ++i) m.src[i] = ch[i].S();
            p.maps.push_back(std::move(m));
        }
        if (ValidPreset(p)) customExportPresets.push_back(std::move(p));
    }
}

Json App::TextureExportPresetsJson() const {
    Json list = Json::Array();
    for (const auto& p : customExportPresets) {
        Json j = Json::Object();
        j["name"] = p.name;
        j["opengl"] = p.opengl;
        for (const auto& m : p.maps) {
            Json item = Json::Object();
            item["name"] = m.suffix;
            for (const auto& input : m.src) item["rgba"].Push(input);
            j["maps"].Push(item);
        }
        list.Push(j);
    }
    return list;
}

void App::StartTextureExport(DocView& v, std::function<void(TexExportResult)> completed, bool overwrite) {
    const auto& o = texExport;
    auto fail = [&](const std::string& message, const char* code = "ExportFailed") {
        Status(message);
        if (completed) completed({message, code, {}});
    };
    if (!v.mesh || o.folder.empty()) return fail("Export des textures : choisissez un dossier", "InvalidParameters");
    TexExportJob job;
    const auto& builtins = Presets();
    const int index = std::clamp(o.preset, 0, (int)(builtins.size() + customExportPresets.size()) - 1);
    job.preset = index < (int)builtins.size() ? builtins[index] : customExportPresets[index - builtins.size()];
    Document& d = *v.doc;
    job.opt = o;
    job.width = d.Width();
    job.height = d.Height();
    job.parts = ExportParts(v);
    u32 allChannels = 0;
    std::set<u32> distinctChannels;
    for (auto& [suffix, r] : job.parts) {
        if (suffix.empty() && r.Empty()) r = {0, 0, (int)d.Width(), (int)d.Height()};
        r = r.Clipped(d.Width(), d.Height());
        if (r.Empty()) return fail("Export des textures : tuile vide " + suffix);
        const u32 channels = d.TextureSetChannels(suffix);
        allChannels |= channels;
        distinctChannels.insert(channels);
        job.partChannels.push_back(channels);
        if (o.atlas) job.atlasChannels.push_back({r, channels});
    }
    if (o.atlas) {
        job.parts = {{"", {0,0,(int)d.Width(),(int)d.Height()}}};
        job.partChannels = {allChannels};
        std::erase_if(job.atlasChannels, [allChannels](const auto& region){ return region.second == allChannels; });
    }
    const std::string base = o.name.empty() ? (d.path.empty() ? std::string("Texture") : Narrow(Utf8Path(d.path).stem().wstring())) : o.name;
    // Built-in separate maps are optional when their channel is disabled, including emissive in packed templates.
    std::vector<std::vector<bool>> enabledMaps;
    for (u32 channels : job.partChannels) {
        auto& enabled = enabledMaps.emplace_back();
        for (const Map& map : job.preset.maps) {
            const C c = FindSource(map.src[0])->channel;
            const bool optional = index == 5 || (index < (int)builtins.size() && c == C::Emissive);
            enabled.push_back(!optional || (channels & Bit(c)) || (c == C::Normal && PbrNormalVariant(channels)));
        }
    }
    std::string error = texexport::PlanOutputs(job.parts, job.preset.maps, o.pattern.empty() ? "{nom}_{carte}" : o.pattern, base,
                                               o.depth == 2 ? ".tga" : ".png", job.outputs, enabledMaps);
    if (!error.empty()) return fail(error, "InvalidParameters");
    if (!overwrite) for (const auto& output : job.outputs) {
        const auto path = Utf8Path(o.folder) / Utf8Path(output.filename);
        if (std::filesystem::exists(path)) return fail("Un fichier existe : " + Narrow(path.wstring()), "ConfirmationRequired");
    }
    v.renderer.EvaluateComplete(d, *ctx);
    std::set<C> needed;
    for (const auto& output : job.outputs)
        for (const auto& id : job.preset.maps[output.map].src) needed.insert(FindSource(id)->channel);
    for (C c : needed) {
        if (c == C::Count) continue;
        if (c == C::Normal) {
            auto* normal = ChannelTexture(v,C::Normal);
            auto* height = ChannelTexture(v,C::Height);
            distinctChannels.insert(allChannels);
            std::set<int> variants;
            for (u32 mask : distinctChannels) variants.insert(PbrNormalVariant(mask));
            for (int variant : variants) {
                gpu::Texture* t = (variant & 1) ? normal : nullptr;
                if ((variant & 2) && height) {
                    auto baked = v.renderer.BakeNormal(*ctx,t,height,d.Width(),d.Height());
                    if (!baked) return fail("Impossible de calculer les normales du jeu de textures.");
                    t = baked.get();
                    job.bakedNormals.push_back(std::move(baked));
                }
                if (t && !(job.normals[variant] = ctx->ReadbackTexture(t))) return fail("Lecture GPU des normales impossible.");
            }
            continue;
        }
        if (!(allChannels & Bit(c))) continue;
        gpu::Texture* t = ChannelTexture(v, c);
        if (t) {
            job.ch[(int)c] = ctx->ReadbackTexture(t);
            if (!job.ch[(int)c]) return fail("Lecture GPU impossible : " + std::string(PbrChannelName(c)));
        }
    }
    if (o.padding > 0 && v.mesh->posMap) job.pos = ctx->ReadbackTexture(v.mesh->posMap.get());
    v.renderer.DropNodeCaches(d);
    job.completed = std::move(completed);
    job.overwrite = overwrite;
    texExportJobs.push_back(std::move(job));
    Status("Export des textures...");
}

void App::PollTextureExports() {
    for (size_t i = 0; i < texExportJobs.size();) {
        auto& j = texExportJobs[i];
        if (j.result.valid()) {
            if (j.result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++i; continue; }
            TexExportResult result;
            try { result = j.result.get(); }
            catch (const std::exception& e) { result = {std::string("Échec de l'export des textures : ") + e.what(), "ExportFailed", {}}; }
            Status(result.message);
            if (j.completed) j.completed(std::move(result));
            texExportJobs.erase(texExportJobs.begin() + i);
            continue;
        }
        bool ready = !j.pos || j.pos->ready;
        for (const auto& rb : j.ch) ready &= !rb || rb->ready;
        for (const auto& rb : j.normals) ready &= !rb || rb->ready;
        if (!ready) { ++i; continue; }
        j.bakedNormals.clear();
        j.result = std::async(std::launch::async, [ch = std::move(j.ch), pos = std::move(j.pos), o = j.opt, p = std::move(j.preset),
                            normals = std::move(j.normals), masks = std::move(j.partChannels), atlas = std::move(j.atlasChannels),
                            parts = std::move(j.parts), outputs = std::move(j.outputs), w = (int)j.width, h = (int)j.height,
                            overwrite = j.overwrite]() -> TexExportResult {
            TexExportResult result;
            auto fail = [&](std::string message, const char* code = "ExportFailed") {
                result.message = std::move(message);
                result.errorCode = code;
                return std::move(result);
            };
            for (const auto& rb : ch) if (rb && !ValidReadback(*rb, w, h)) return fail("Export : lecture GPU de canal incomplète.");
            for (const auto& rb : normals) if (rb && !ValidReadback(*rb, w, h)) return fail("Export : lecture GPU des normales incomplète.");
            if (pos && !ValidReadback(*pos, w, h)) return fail("Export : lecture GPU des UV incomplète.");
            std::error_code ec;
            std::filesystem::create_directories(Utf8Path(o.folder), ec);
            if (ec) return fail("Export : " + ec.message());
            int previous = -1, written = 0;
            std::vector<texexport::PaddingPixel> padding;
            for (const auto& output : outputs) {
                const Rect& r = parts[output.part].second;
                const int tw = r.Width(), th = r.Height();
                if (previous != output.part && pos) {
                    std::vector<u8> covered((size_t)tw * th);
                    for (int y = 0; y < th; ++y) for (int x = 0; x < tw; ++x)
                        // The viewport's six preview-padding texels carry alpha 0.5. Only geometry (1) seeds export dilation.
                        covered[(size_t)y * tw + x] = Read(*pos, (size_t)(r.y0 + y) * w + r.x0 + x, 3) > 0.75f;
                    padding = texexport::BuildPadding(std::move(covered), tw, th, o.padding);
                    previous = output.part;
                }
                const Map& map = p.maps[output.map];
                std::vector<float> image((size_t)tw * th * 4);
                auto components = [&](u32 enabled) {
                    std::array<ExportComponent,4> result{};
                    for (int k = 0; k < 4; ++k) {
                        const Source& s = *FindSource(map.src[k]);
                        const bool opengl = o.normal < 0 ? p.opengl : o.normal == 1;
                        const gpu::Readback* source = nullptr;
                        if (s.channel == C::Normal) source = normals[PbrNormalVariant(enabled)].get();
                        else if (s.channel != C::Count && (enabled & Bit(s.channel))) source = ch[(int)s.channel].get();
                        result[k] = {source,s.component,s.fallback,map.src[k] == "S" || (map.src[k] == "N.g" && opengl)};
                    }
                    return result;
                };
                const auto inputs = components(masks[output.part]);
                for (int y = 0; y < th; ++y) for (int x = 0; x < tw; ++x) {
                    const size_t src = (size_t)(r.y0 + y) * w + r.x0 + x, dst = ((size_t)y * tw + x) * 4;
                    for (int k = 0; k < 4; ++k) image[dst + k] = inputs[k].Sample(src);
                }
                for (const auto& [region, enabled] : atlas) {
                    const auto local = components(enabled);
                    for (int k = 0; k < 4; ++k) {
                        if (inputs[k].image == local[k].image) continue;
                        for (int y = region.y0; y < region.y1; ++y) for (int x = region.x0; x < region.x1; ++x) {
                            const size_t px = (size_t)y*w+x;
                            image[px*4+k] = local[k].Sample(px);
                        }
                    }
                }
                texexport::ApplyPadding(padding, tw, image);
                static const int fixed[] = {512,1024,2048,4096};
                const int ow = o.size <= 2 ? std::max(1, tw >> std::clamp(o.size, 0, 2)) : fixed[std::clamp(o.size-3, 0, 3)];
                const int oh = std::max(1, (int)std::lround((double)th * ow / tw));
                auto resized = ResizeMap(std::move(image), tw, th, ow, oh);
                const std::string path = o.folder + "/" + output.filename;
                if (!overwrite && std::filesystem::exists(Utf8Path(path)))
                    return fail("Un fichier existe : " + path, "ConfirmationRequired");
                std::string err;
                if (!WriteMap(path, resized, ow, oh, o.depth, err))
                    return fail("Échec de l'export : " + path + " (" + std::to_string(written) + " image(s) écrite(s)). " + err);
                result.files.push_back(path);
                ++written;
            }
            result.message = "Export des textures (" + p.name + ") : " + std::to_string(written) + " image(s) dans " + o.folder;
            return result;
        });
        ++i;
    }
}

void App::RegisterAiTextureExport() {
    using namespace ai;
    bridge.Register({"shaderixel.paint3d_export_presets", "Liste les modèles d’export des textures et la composition de leurs cartes.",
        Schema({}), false, {}, [this](const Json&, const std::string&) {
            Json r = Json::Object();
            r["presets"] = Json::Array();
            auto add = [&](const auto& presets) {
                for (const auto& preset : presets) {
                    Json row = Json::Object();
                    row["index"] = static_cast<u32>(r["presets"].Size());
                    row["name"] = preset.name;
                    row["maps"] = Json::Array();
                    for (const auto& map : preset.maps) row["maps"].Push(map.suffix);
                    r["presets"].Push(std::move(row));
                }
            };
            add(Presets());
            add(customExportPresets);
            return AiResult::Ok(std::move(r));
        }});

    bridge.Register({"shaderixel.paint3d_export", "Exporte les textures PBR par UDIM/jeu ou en atlas avec les modèles natifs. Attends la fin et renvoie les fichiers. overwrite confirme le remplacement de fichiers existants.",
        Schema({{"folder", T("string")}, {"name", T("string")}, {"pattern", T("string")}, {"preset", Range("integer", 0, 65535)},
                {"size", Enum({"native", "half", "quarter", "512", "1024", "2048", "4096"})}, {"atlas", T("boolean")},
                {"format", Enum({"png8", "png16", "tga"})}, {"normal", Enum({"preset", "directx", "opengl"})},
                {"padding", Range("integer", 0, 64)}, {"overwrite", T("boolean")}}, {"folder"}),
        true, {"folder"}, [this](const Json& p, const std::string&) {
            auto* v = Active();
            if (!v || !v->mesh) return AiResult::Fail("NoMesh", "Ouvrez un projet avec un maillage");
            if (sim.view || meshStroking || fluidRun.active || !texExportJobs.empty())
                return AiResult::Fail("Busy", "Terminez l’export ou le geste en cours");
            TexExportOptions selected;
            selected.folder = p["folder"].S();
            if (!Utf8Path(selected.folder).is_absolute()) return AiResult::Fail("InvalidParameters", "Dossier absolu requis");
            selected.name = p["name"].S();
            selected.pattern = p["pattern"].S("{nom}_{carte}");
            selected.preset = p["preset"].I(v->doc->exportTemplate);
            if (selected.preset < 0 || static_cast<size_t>(selected.preset) >= Presets().size() + customExportPresets.size())
                return AiResult::Fail("InvalidParameters", "Modèle d’export inconnu");
            const std::string sizes[] = {"native", "half", "quarter", "512", "1024", "2048", "4096"};
            selected.size = static_cast<int>(std::ranges::find(sizes, p["size"].S("native")) - std::begin(sizes));
            selected.atlas = p["atlas"].B();
            const auto format = p["format"].S(), normal = p["normal"].S();
            selected.depth = format == "png16" ? 1 : format == "tga" ? 2 : 0;
            selected.normal = normal == "opengl" ? 1 : normal == "directx" ? 0 : -1;
            selected.padding = std::clamp(p["padding"].I(8), 0, 64);
            auto finished = std::make_shared<std::optional<TexExportResult>>();
            const auto previous = texExport;
            texExport = std::move(selected);
            try { StartTextureExport(*v, [finished](TexExportResult result) { *finished = std::move(result); }, p["overwrite"].B()); }
            catch (...) { texExport = previous; throw; }
            texExport = previous;
            AiResult result;
            result.done = false;
            result.poll = [finished](AiResult& out) {
                if (!*finished) return false;
                const auto& result = finished->value();
                if (!result.errorCode.empty()) out = AiResult::Fail(result.errorCode, result.message);
                else {
                    out.result["files"] = Json::Array();
                    for (const auto& path : result.files) out.result["files"].Push(path);
                    out.result["message"] = result.message;
                }
                return true;
            };
            return result;
        }, Schema({{"files", T("array")}, {"message", T("string")}}, {"files", "message"})});
}

void App::DrawTextureExportDialog() {
    auto& E = texExport;
    if (!E.open) return;
    DocView* v = Active();
    if (!v || v->doc->kind != 1) { E.open = false; return; }
    ImGui::SetNextWindowSize(ImVec2(560 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Exporter les textures", &E.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ui::FilterBox("Filtrer (Ctrl+F)...", 320.0f * dpi);
    const auto& builtins = Presets();
    auto presetAt = [&](int i) -> const Preset& {
        return i < (int)builtins.size() ? builtins[i] : customExportPresets[i - builtins.size()];
    };
    const int count = (int)(builtins.size() + customExportPresets.size());
    E.preset = std::clamp(E.preset, 0, count - 1);
    ImGui::PushItemWidth(320 * dpi);
    if (ImGui::BeginCombo("Modèle", presetAt(E.preset).name.c_str())) {
        for (int i = 0; i < count; ++i)
            if (ImGui::Selectable(presetAt(i).name.c_str(), i == E.preset)) { E.preset = i; exportPresetEditing = -2; }
        ImGui::EndCombo();
    }
    const Preset& pr = presetAt(E.preset);
    if (!pr.desc.empty()) ImGui::TextWrapped("%s", pr.desc.c_str());
    if (exportPresetEditing == -2) {
        if (ImGui::Button("Dupliquer le modèle")) {
            exportPresetDraft = pr;
            exportPresetDraft.name += " (copie)";
            exportPresetEditing = -1;
        }
        if (E.preset >= (int)builtins.size()) {
            ImGui::SameLine();
            if (ImGui::Button("Modifier le modèle")) { exportPresetDraft = pr; exportPresetEditing = E.preset - (int)builtins.size(); }
        }
        for (const Map& m : pr.maps)
            ImGui::Text("%s     R %s   V %s   B %s   A %s", m.suffix.c_str(), FindSource(m.src[0])->label, FindSource(m.src[1])->label,
                        FindSource(m.src[2])->label, FindSource(m.src[3])->label);
    }
    if (exportPresetEditing != -2) {
        Preset& draft = exportPresetDraft;
        ImGui::InputText("Nom du modèle", &draft.name);
        ImGui::Checkbox("Normales OpenGL par défaut", &draft.opengl);
        int remove = -1;
        const float editorHeight = std::clamp((ImGui::GetFrameHeightWithSpacing()*2+4*dpi)*(float)draft.maps.size(), 32.0f*dpi, 230.0f*dpi);
        ImGui::BeginChild("##exportmaps", ImVec2(0, editorHeight), ImGuiChildFlags_Borders);
        for (int i = 0; i < (int)draft.maps.size(); ++i) {
            ImGui::PushID(i);
            Map& m = draft.maps[i];
            ImGui::InputText("Nom de la carte", &m.suffix);
            ImGui::SameLine();
            if (ImGui::SmallButton("Retirer")) remove = i;
            static const char* labels[] = {"R", "V", "B", "A"};
            for (int k = 0; k < 4; ++k) {
                if (k) ImGui::SameLine();
                ImGui::SetNextItemWidth(86 * dpi);
                if (ImGui::BeginCombo(labels[k], FindSource(m.src[k])->label)) {
                    for (auto& source : sources)
                        if (ImGui::Selectable(source.label, m.src[k] == source.id)) m.src[k] = source.id;
                    ImGui::EndCombo();
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (remove >= 0) draft.maps.erase(draft.maps.begin() + remove);
        if (ImGui::Button("Ajouter une carte")) draft.maps.push_back({"Carte" + std::to_string(draft.maps.size()+1), {"C.r","C.g","C.b","1"}});
        ImGui::SameLine();
        if (ImGui::Button("Enregistrer le modèle")) {
            bool duplicate = false;
            const int editing = exportPresetEditing < 0 ? -1 : (int)builtins.size() + exportPresetEditing;
            for (int i = 0; i < count; ++i) if (i != editing && presetAt(i).name == draft.name) duplicate = true;
            std::vector<texexport::Output> preview;
            const std::string error = texexport::PlanOutputs({{"", {}}}, draft.maps, "{nom}_{carte}", "Texture", ".png", preview);
            if (!ValidPreset(draft) || duplicate || !error.empty()) Status(error.empty() ? "Le modèle doit avoir un nom unique et des cartes nommées." : error);
            else {
                draft.desc.clear();
                const auto previous = customExportPresets;
                if (exportPresetEditing < 0) customExportPresets.push_back(draft);
                else customExportPresets[exportPresetEditing] = draft;
                if (SaveAiSettings()) {
                    if (exportPresetEditing < 0) E.preset = count;
                    exportPresetEditing = -2;
                } else {
                    customExportPresets = previous;
                    Status("Impossible d’enregistrer le modèle dans " + settingsPath);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler les modifications")) exportPresetEditing = -2;
    }
    ImGui::SeparatorText("Options");
    int layout = E.atlas ? 1 : 0;
    if (ImGui::Combo("Découpage", &layout, "Jeux de textures (matériau / UDIM)\0Atlas du projet\0")) E.atlas = layout == 1;
    ImGui::Combo("Format", &E.depth, "PNG 8 bits\0PNG 16 bits\0TGA 8 bits\0");
    ImGui::Combo("Taille", &E.size, "Taille native de la tuile\0Moitié\0Quart\0" "512\0" "1024\0" "2048\0" "4096\0");
    int nrm = E.normal + 1;
    if (ImGui::Combo("Normale", &nrm, "Selon le modèle\0DirectX (Y-, Unreal)\0OpenGL (Y+, Unity, Blender, Godot)\0")) E.normal = nrm - 1;
    ImGui::SliderInt("Marge (texels)", &E.padding, 0, 64);
    ui::HelpTooltip("Les couleurs des îles UV débordent de ce nombre de texels (pas de joints visibles de loin).");
    ImGui::InputText("Nom des fichiers", &E.pattern);
    ui::HelpTooltip("{nom} : nom de base, {carte} : nom de la carte (BaseColor, ORM...).");
    ImGui::InputTextWithHint("Nom de base", "nom du projet", &E.name);
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::InputTextWithHint("##folder", "dossier", &E.folder);
    ImGui::SameLine();
    if (ImGui::Button("...")) {
        std::string f = platform::SaveFileDialog(hwnd, L"Dossier des textures (nom au choix)\0*.png\0", L"png");
        if (!f.empty()) E.folder = Narrow(Utf8Path(f).parent_path().wstring());
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Dossier");
    ImGui::PopItemWidth();
    ImGui::Separator();
    ImGui::BeginDisabled(E.folder.empty() || !texExportJobs.empty() || exportPresetEditing != -2);
    if (ImGui::Button(texExportJobs.empty() ? "Exporter" : "Export en cours...", ImVec2(160 * dpi, 0))) StartTextureExport(*v);
    ImGui::EndDisabled();
    ImGui::End();
}

}  // namespace sx
