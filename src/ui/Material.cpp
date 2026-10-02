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
    static const char* names[] = {"Couleur de base", "Rugosité", "Métal", "Normale", "Émissif", "Occlusion", "Hauteur"};
    return names[(int)c];
}

const char* PbrChannelSuffix(PbrChannel c) {
    static const char* names[] = {"BaseColor", "Roughness", "Metallic", "Normal", "Emissive", "AO", "Height"};
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
    for (auto& n : d.S().graph.nodes)
        if (n.kind == NodeKind::Blend && n.layer == g && n.inputs.size() > 1 && n.inputs[1]) return v.renderer.EvaluateNode(d, *ctx, n.inputs[1]);
    return nullptr;
}

MeshMaterial App::GatherMaterial(DocView& v) {
    MeshMaterial m;
    for (int c = 0; c < (int)PbrChannel::Count; ++c) m.tex[c] = ChannelTexture(v, (PbrChannel)c);
    return m;
}

void App::CreatePbrChannels(DocView& v) {
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
    if (!FindChannelGroup(d, PbrChannel::BaseColor)) {
        Layer g = makeGroup(PbrChannel::BaseColor);
        for (auto& l : L) if (l.parent == 0) l.parent = g.id;
        L.push_back(g);
    }
    // Default values (visual gray levels), as solid fills inside each group.
    struct Def { PbrChannel c; float3 value; bool fill; };
    const Def defs[] = {{PbrChannel::Roughness, {0.5f, 0.5f, 0.5f}, true}, {PbrChannel::Metallic, {0, 0, 0}, true},
                        {PbrChannel::Normal, {0.5f, 0.5f, 1.0f}, true},    {PbrChannel::Emissive, {0, 0, 0}, false},
                        {PbrChannel::Occlusion, {1, 1, 1}, true},          {PbrChannel::Height, {0.5f, 0.5f, 0.5f}, true}};
    auto solid = Effects().Find("solid_color");
    Id paintLayer = 0;
    for (const Def& df : defs) {
        if (FindChannelGroup(d, df.c)) continue;
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

void App::ExportPbrTextures(DocView& v, const std::string& pngBasePath) {
    if (pngBasePath.empty()) return;
    Document& d = *v.doc;
    auto p = std::filesystem::absolute(Utf8Path(pngBasePath));  // the Unreal script needs absolute paths
    std::string stem = Narrow(p.stem().wstring());
    std::vector<std::pair<PbrChannel, std::string>> written;
    v.renderer.EvaluateComplete(d, *ctx);
    gpu::TexturePtr baked;
    {
        gpu::Texture* n = ChannelTexture(v, PbrChannel::Normal);
        gpu::Texture* h = ChannelTexture(v, PbrChannel::Height);
        if (h) baked = v.renderer.BakeNormal(*ctx, n, h, d.Width(), d.Height());
    }
    for (int c = 0; c < (int)PbrChannel::Count; ++c) {
        gpu::Texture* t = ChannelTexture(v, (PbrChannel)c);
        if ((PbrChannel)c == PbrChannel::Normal && baked) t = baked.get();  // normal map with the height baked in
        if (!t) continue;
        std::string file = Narrow((p.parent_path() / Utf8Path(stem + "_" + PbrChannelSuffix((PbrChannel)c) + ".png")).wstring());
        if (auto rb = v.renderer.ExportRGBA8(*ctx, t)) {
            exports.push_back({file, rb});
            written.push_back({(PbrChannel)c, file});
        }
    }
    v.renderer.DropNodeCaches(d);
    // Unreal Editor Python script: import the textures and wire a material.
    std::string py = "# Generated by Shaderixel: run in the Unreal Editor (Python) to import the textures and build the material.\n";
    py += "import unreal\n\n";
    py += "name = '" + stem + "'\npath = '/Game/Shaderixel/' + name\n";
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
        py += var + " = import_texture(r'" + f + "', '" + asset + "', " + (color ? "True" : "False") + ", " + compression + ")\n";
        py += "s_" + std::string(PbrChannelSuffix(c)) + " = sample(" + var + ", " + std::to_string(y) + ", " + sampler + ")\n";
        const char* prop = c == PbrChannel::BaseColor ? "MP_BASE_COLOR" : c == PbrChannel::Roughness ? "MP_ROUGHNESS" : c == PbrChannel::Metallic ? "MP_METALLIC"
                           : c == PbrChannel::Normal ? "MP_NORMAL" : c == PbrChannel::Emissive ? "MP_EMISSIVE_COLOR" : "MP_AMBIENT_OCCLUSION";
        const char* out = color || c == PbrChannel::Normal ? "RGB" : "R";
        if (c == PbrChannel::Height) py += "# Height is already baked into the normal map; kept for displacement / parallax setups.\n\n";
        else py += "mel.connect_material_property(s_" + std::string(PbrChannelSuffix(c)) + ", '" + out + "', unreal.MaterialProperty." + prop + ")\n\n";
        y += 260;
    }
    py += "mel.recompile_material(mat)\nunreal.EditorAssetLibrary.save_asset(path + '/' + mat_name)\n";
    py += "unreal.log('Shaderixel: material ' + mat_name + ' created in ' + path)\n";
    std::string pyPath = Narrow((p.parent_path() / Utf8Path(stem + "_unreal_material.py")).wstring());
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(pyPath).c_str(), L"wb") == 0 && f) {
        fwrite(py.data(), 1, py.size(), f);
        fclose(f);
    }
    Status("Textures PBR exportées (" + std::to_string(written.size()) + ") + script Unreal : " + pyPath);
}

}  // namespace sx
