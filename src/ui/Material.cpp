// PBR material channels for 3D painting.
//
// A document becomes a material when it has root-level groups named after the
// channels ("Couleur de base", "Rugosité", ...). Each group's isolated
// composite feeds the 3D preview and the exported textures; without a base
// color group the whole document is the base color. Painting in 3D targets the
// active layer, so selecting a layer inside "Rugosité" paints roughness.
#include "ui/App.h"
#include "render/MeshPaint.h"

#include <cstdio>

namespace sx {

const char* PbrChannelName(PbrChannel c) {
    static const char* names[] = {"Couleur de base", "Rugosité", "Métal", "Normale", "Émissif", "Occlusion", "Hauteur", "Opacité", "Utilisateur 1", "Utilisateur 2", "Utilisateur 3", "Utilisateur 4"};
    return names[(int)c];
}

const char* PbrChannelSuffix(PbrChannel c) {
    static const char* names[] = {"BaseColor", "Roughness", "Metallic", "Normal", "Emissive", "AO", "Height", "Opacity", "User0", "User1", "User2", "User3"};
    return names[(int)c];
}

Id App::FindChannelGroup(Document& d, PbrChannel c) {
    for (auto& l : d.S().layers)
        if (l.kind == LayerKind::Group && l.parent == 0 && l.name == PbrChannelName(c)) return l.id;
    return 0;
}

gpu::Texture* App::ChannelTexture(DocView& v, PbrChannel c) {
    Document& d = *v.doc;
    Id g = d.Mode() == DocMode::Layers ? FindChannelGroup(d, c) : 0;
    if (!g) return c == PbrChannel::BaseColor ? v.lastImage : nullptr;
    for (auto& n : d.S().graph.nodes) {
        if (n.kind != NodeKind::Blend || n.layer != g || n.inputs.size() <= 1 || !n.inputs[1]) continue;
        auto* source = v.renderer.EvaluateNode(d,*ctx,n.inputs[1]);
        if (!source || c == PbrChannel::BaseColor || !v.mesh || d.S().textureSetChannels.empty()) {
            v.setChannelPreviews.erase((int)c);
            return source;
        }
        Hasher hash;
        hash.Pod(source); hash.Pod(v.renderer.NodeHash(n.inputs[1])); hash.Pod(d.ContentVersion());
        std::vector<Rect> disabled;
        for (const auto& [set,rect] : ExportParts(v,true)) if (!(d.TextureSetChannels(set) & (1u << (int)c))) {
            disabled.push_back(rect);
            hash.Pod(rect);
        }
        if (disabled.empty()) { v.setChannelPreviews.erase((int)c); return source; }
        auto& cached = v.setChannelPreviews[(int)c];
        if (!cached.texture || cached.texture->Width() != source->Width() || cached.texture->Height() != source->Height() ||
            cached.texture->GetFormat() != source->GetFormat()) {
            cached.texture = gpu::Dev().CreateTexture(source->Width(),source->Height(),source->GetFormat(),"TextureSetChannel");
            cached.key = 0;
        }
        if (!cached.texture) return nullptr;
        if (hash.h != cached.key) {
            ctx->CopyTexture(cached.texture.get(),source);
            const float neutral = c == PbrChannel::Occlusion || c == PbrChannel::Opacity ? 1.0f :
                                  c == PbrChannel::Roughness || c == PbrChannel::Height || c == PbrChannel::Normal ? SrgbToLinear(0.5f) : 0.0f;
            const float4 value{neutral,neutral,c == PbrChannel::Normal ? 1.0f : neutral,1.0f};
            for (const auto& rect : disabled) v.renderer.FillTexture(*ctx,cached.texture.get(),value,rect);
            cached.key = hash.h;
        }
        return cached.texture.get();
    }
    return nullptr;
}

MeshMaterial App::GatherMaterial(DocView& v) {
    MeshMaterial m;
    for (int c = 0; c <= (int)PbrChannel::Opacity; ++c) m.tex[c] = ChannelTexture(v, (PbrChannel)c);
    if (v.mesh && v.mesh->viewChannel >= 13 && v.mesh->viewChannel <= 16) {
        const int c = (int)PbrChannel::User0 + v.mesh->viewChannel - 13;
        m.tex[c] = ChannelTexture(v, (PbrChannel)c);
    }
    return m;
}

void App::CreatePbrChannels(DocView& v, bool project, u32 additionalChannels) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) { Status("Les canaux PBR utilisent le mode calques"); return; }
    Edit e(d, v.history, "Créer les canaux PBR");
    auto& L = d.S().layers;
    auto makeGroup = [&](PbrChannel c) {
        Layer g;
        g.id = d.NewId();
        g.kind = LayerKind::Group;
        g.name = PbrChannelName(c);
        g.passThrough = false;  // isolated: its composite is the channel
        g.visible = c == PbrChannel::BaseColor;
        return g;
    };
    auto solidDef = Effects().Find("solid_color");
    if (!FindChannelGroup(d, PbrChannel::BaseColor)) {
        Layer g = makeGroup(PbrChannel::BaseColor);
        for (auto& l : L) if (l.parent == 0) l.parent = g.id;
        if (project && solidDef) {
            // Project: a neutral base color under the layers.
            Layer f;
            f.id = d.NewId();
            f.kind = LayerKind::Fill;
            f.name = "Couleur de base (valeur)";
            f.parent = g.id;
            f.effect.effectId = "solid_color";
            f.effect.params = solidDef->DefaultParams();
            f.effect.params[0] = {SrgbToLinear(0.5f), SrgbToLinear(0.5f), SrgbToLinear(0.5f), 1};
            L.push_back(f);
        }
        L.push_back(g);
    }
    // Default values (visual gray levels), as solid fills inside each group.
    struct Def { PbrChannel c; float3 value; bool fill; };
    const Def defs[] = {{PbrChannel::Roughness, {0.5f, 0.5f, 0.5f}, true}, {PbrChannel::Metallic, {0, 0, 0}, true},
                        {PbrChannel::Normal, {0.5f, 0.5f, 1.0f}, true},    {PbrChannel::Emissive, {0, 0, 0}, false},
                        {PbrChannel::Occlusion, {1, 1, 1}, true},          {PbrChannel::Height, {0.5f, 0.5f, 0.5f}, true},
                        {PbrChannel::Opacity, {1, 1, 1}, true}, {PbrChannel::User0, {}, false}, {PbrChannel::User1, {}, false},
                        {PbrChannel::User2, {}, false}, {PbrChannel::User3, {}, false}};
    auto solid = Effects().Find("solid_color");
    Id paintLayer = 0;
    for (const Def& df : defs) {
        if (FindChannelGroup(d, df.c)) continue;
        if (project && !((d.pbrChannels | additionalChannels) & (1u << (int)df.c))) continue;
        Layer g = makeGroup(df.c);
        if (df.fill && solid) {
            Layer f;
            f.id = d.NewId();
            f.kind = LayerKind::Fill;
            f.name = std::string(PbrChannelName(df.c)) + " (valeur)";
            f.parent = g.id;
            f.effect.effectId = "solid_color";
            f.effect.params = solid->DefaultParams();
            f.effect.params[0] = {SrgbToLinear(df.value.x), SrgbToLinear(df.value.y), SrgbToLinear(df.value.z), 1};
            L.push_back(f);
        }
        if (project) {
            L.push_back(g);
            continue;
        }
        Layer p;  // layer to paint the channel
        p.id = d.NewId();
        p.kind = LayerKind::Pixel;
        p.name = std::string(PbrChannelName(df.c)) + " (peinture)";
        p.parent = g.id;
        p.surface = d.CreateSurface({0, 0, 0, 0});
        L.push_back(p);
        if (df.c == PbrChannel::Roughness) paintLayer = p.id;
        L.push_back(g);
    }
    if (paintLayer) d.S().activeLayer = paintLayer;
    d.SyncGraph();
    e.Commit();
    Status("Canaux PBR créés : peignez dans le calque « peinture » d'un canal (en gris pour les données)");
}

static bool WriteUnrealMaterialScript(const std::filesystem::path& directory, const std::string& stem,
                                      const std::vector<std::pair<PbrChannel, std::string>>& written);

std::array<gpu::ReadbackPtr,4> App::ReadSetNormals8(DocView& v, const std::vector<std::pair<std::string,Rect>>& parts) {
    Document& d = *v.doc;
    auto* normal = ChannelTexture(v,PbrChannel::Normal);
    auto* height = ChannelTexture(v,PbrChannel::Height);
    std::set<int> variants;
    for (const auto& [set, rect] : parts) variants.insert(PbrNormalVariant(d.kind == 1 ? d.TextureSetChannels(set) : ~0u));
    std::array<gpu::ReadbackPtr,4> out{};
    for (int variant : variants) {
        auto* t = (variant & 1) ? normal : nullptr;
        auto baked = (variant & 2) && height ? v.renderer.BakeNormal(*ctx,t,height,d.Width(),d.Height()) : nullptr;
        if (baked) t = baked.get();
        if (t) out[variant] = v.renderer.ExportRGBA8(*ctx,t);
    }
    return out;
}

void App::ExportPbrTextures(DocView& v, const std::string& pngBasePath) {
    if (pngBasePath.empty()) return;
    const auto parts = ExportParts(v);
    const auto dest = std::filesystem::absolute(Utf8Path(pngBasePath));
    const auto base = Narrow(dest.stem().wstring());
    const std::string error = texexport::ValidateSetNames(parts, base);
    if (!error.empty()) { Status(error); return; }
    Document& d = *v.doc;
    std::vector<std::pair<PbrChannel, gpu::ReadbackPtr>> channels;
    v.renderer.EvaluateComplete(d, *ctx);
    const auto normals = ReadSetNormals8(v, parts);
    if (std::any_of(normals.begin(),normals.end(),[](const auto& rb){ return bool(rb); }))
        channels.push_back({PbrChannel::Normal,{}});
    for (int c = 0; c < (int)PbrChannel::Count; ++c) {
        if ((PbrChannel)c == PbrChannel::Normal) continue;
        gpu::Texture* t = ChannelTexture(v, (PbrChannel)c);
        if (!t) continue;
        if (auto rb = v.renderer.ExportRGBA8(*ctx, t)) channels.push_back({(PbrChannel)c, std::move(rb)});
    }
    v.renderer.DropNodeCaches(d);
    for (const auto& [suffix, crop] : parts) {
        const auto [material, udim] = texexport::SetSuffixes(suffix);
        const std::string stem = base + material;
        std::vector<std::pair<PbrChannel, std::string>> written;
        const u32 enabled = d.kind == 1 ? d.TextureSetChannels(suffix) : ~0u;
        for (const auto& [c, source] : channels) {
            const auto& rb = c == PbrChannel::Normal ? normals[PbrNormalVariant(enabled)] : source;
            if (!rb || (c != PbrChannel::Normal && !(enabled & (1u << (int)c)))) continue;
            const auto filename = stem + "_" + PbrChannelSuffix(c) + udim + ".png";
            const auto file = Narrow((dest.parent_path() / Utf8Path(filename)).wstring());
            exports.push_back({file, rb, crop});
            written.push_back({c, file});
        }
        if (udim.empty() && !WriteUnrealMaterialScript(dest.parent_path(), stem, written)) {
            Status("Échec de l'écriture du script Unreal : " + stem);
            return;
        }
    }
    Status("Export des textures PBR : " + std::to_string(channels.size()) + " canal/canaux, " + std::to_string(parts.size()) + " jeu(x).");
}

static bool WriteUnrealMaterialScript(const std::filesystem::path& directory, const std::string& stem,
                                      const std::vector<std::pair<PbrChannel, std::string>>& written) {
    // Unreal Editor Python script: import the textures and wire a material.
    std::string py = "# Generated by Shaderixel: run in the Unreal Editor (Python) to import the textures and build the material.\n";
    py += "import unreal\n\n";
    py += "name = " + Json(stem).Dump(0) + "\npath = '/Game/Shaderixel/' + name\n";
    py += "tools = unreal.AssetToolsHelpers.get_asset_tools()\nmel = unreal.MaterialEditingLibrary\n\n";
    py += "def import_texture(file, asset, srgb, compression):\n"
          "    t = unreal.AssetImportTask()\n"
          "    t.set_editor_property('filename', file)\n"
          "    t.set_editor_property('destination_path', path)\n"
          "    t.set_editor_property('destination_name', asset)\n"
          "    t.set_editor_property('automated', True)\n"
          "    t.set_editor_property('replace_existing', True)\n"
          "    t.set_editor_property('save', True)\n"
          "    tools.import_asset_tasks([t])\n"
          "    tex = unreal.EditorAssetLibrary.load_asset(path + '/' + asset)\n"
          "    tex.set_editor_property('srgb', srgb)\n"
          "    tex.set_editor_property('compression_settings', compression)\n"
          "    unreal.EditorAssetLibrary.save_asset(path + '/' + asset)\n"
          "    return tex\n\n";
    py += "mat_name = 'M_' + name\n";
    py += "if unreal.EditorAssetLibrary.does_asset_exist(path + '/' + mat_name):\n    unreal.EditorAssetLibrary.delete_asset(path + '/' + mat_name)\n";
    py += "mat = tools.create_asset(mat_name, path, unreal.Material, unreal.MaterialFactoryNew())\n\n";
    py += "def sample(tex, y, sampler):\n"
          "    s = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -450, y)\n"
          "    s.set_editor_property('texture', tex)\n"
          "    s.set_editor_property('sampler_type', sampler)\n"
          "    return s\n\n";
    int y = -300;
    for (auto& [c, file] : written) {
        std::string f = file;
        for (auto& ch : f) if (ch == '\\') ch = '/';
        std::string asset = "T_" + stem + "_" + PbrChannelSuffix(c);
        std::string var = std::string("t_") + PbrChannelSuffix(c);
        bool color = c == PbrChannel::BaseColor || c == PbrChannel::Emissive;
        std::string compression = c == PbrChannel::Normal ? "unreal.TextureCompressionSettings.TC_NORMALMAP"
                                  : color                  ? "unreal.TextureCompressionSettings.TC_DEFAULT"
                                                           : "unreal.TextureCompressionSettings.TC_MASKS";
        std::string sampler = c == PbrChannel::Normal ? "unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL"
                              : color                 ? "unreal.MaterialSamplerType.SAMPLERTYPE_COLOR"
                                                      : "unreal.MaterialSamplerType.SAMPLERTYPE_MASKS";
        py += var + " = import_texture(" + Json(f).Dump(0) + ", " + Json(asset).Dump(0) + ", " + (color ? "True" : "False") + ", " + compression + ")\n";
        py += "s_" + std::string(PbrChannelSuffix(c)) + " = sample(" + var + ", " + std::to_string(y) + ", " + sampler + ")\n";
        const char* prop = c == PbrChannel::BaseColor ? "MP_BASE_COLOR" : c == PbrChannel::Roughness ? "MP_ROUGHNESS" : c == PbrChannel::Metallic ? "MP_METALLIC"
                           : c == PbrChannel::Normal ? "MP_NORMAL" : c == PbrChannel::Emissive ? "MP_EMISSIVE_COLOR" : c == PbrChannel::Opacity ? "MP_OPACITY_MASK" : "MP_AMBIENT_OCCLUSION";
        const char* out = color || c == PbrChannel::Normal ? "RGB" : "R";
        if (c == PbrChannel::Opacity) py += "mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)\n";
        if (c >= PbrChannel::User0) { y += 260; continue; }
        if (c == PbrChannel::Height) py += "# Height is already baked into the normal map; kept for displacement / parallax setups.\n\n";
        else py += "mel.connect_material_property(s_" + std::string(PbrChannelSuffix(c)) + ", '" + out + "', unreal.MaterialProperty." + prop + ")\n\n";
        y += 260;
    }
    py += "mel.recompile_material(mat)\nunreal.EditorAssetLibrary.save_asset(path + '/' + mat_name)\n";
    py += "unreal.log('Shaderixel: material ' + mat_name + ' created in ' + path)\n";
    const auto pyPath = directory / Utf8Path(stem + "_unreal_material.py");
    FILE* f = nullptr;
    if (_wfopen_s(&f, pyPath.c_str(), L"wb") != 0 || !f) return false;
    const bool ok = fwrite(py.data(), 1, py.size(), f) == py.size();
    return fclose(f) == 0 && ok;
}

}  // namespace sx
