// Paint 3D projects (.sx3d): a texture set painted on a mesh, Substance-style.
// The document holds one isolated group per PBR channel; a "3D layer" is a
// material id spread over these groups:
//   - paint layer: one pixel layer per channel, painted together (brush material),
//   - fill layer: one value per channel and a shared mask (material presets).
// Projects have their own workspace layout, a reduced tool set, a decal
// projection tool and texture exports by template (separate maps, ORM...).
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "doc/Brushes.h"
#include "doc/Effects.h"
#include "ai/Net.h"

#include <fstream>

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>

namespace sx {

namespace {
const PbrChannel kPaintChannels[] = {PbrChannel::BaseColor, PbrChannel::Roughness, PbrChannel::Metallic, PbrChannel::Height, PbrChannel::Emissive};
}

// ------------------------------------------------------------------ mesh in the file

Json App::MeshToJson(const MeshData& m) {
    Json j = Json::Object();
    j["name"] = m.name;
    j["vertices"] = net::Base64Encode(m.vertices.data(), m.vertices.size() * sizeof(MeshVertex));
    j["indices"] = net::Base64Encode(m.indices.data(), m.indices.size() * sizeof(u32));
    // Placement of the original model (exports in its own units).
    Json c = Json::Array();
    c.Push(m.srcCenter.x); c.Push(m.srcCenter.y); c.Push(m.srcCenter.z);
    j["srcCenter"] = c;
    j["srcScale"] = m.srcScale;
    return j;
}

bool App::MeshFromJson(const Json& j, MeshData& m) {
    std::vector<u8> v = net::Base64Decode(j["vertices"].S()), i = net::Base64Decode(j["indices"].S());
    if (v.empty() || i.empty() || v.size() % sizeof(MeshVertex) || i.size() % sizeof(u32)) return false;
    m.name = j["name"].S("Maillage");
    m.vertices.resize(v.size() / sizeof(MeshVertex));
    memcpy(m.vertices.data(), v.data(), v.size());
    m.indices.resize(i.size() / sizeof(u32));
    memcpy(m.indices.data(), i.data(), i.size());
    if (j["srcCenter"].Size() == 3) m.srcCenter = {j["srcCenter"].At(0).F(), j["srcCenter"].At(1).F(), j["srcCenter"].At(2).F()};
    m.srcScale = j["srcScale"].F(1.0f);
    m.boundsMin = {1e30f, 1e30f, 1e30f};
    m.boundsMax = {-1e30f, -1e30f, -1e30f};
    for (auto& p : m.vertices) {
        m.boundsMin = {std::min(m.boundsMin.x, p.pos.x), std::min(m.boundsMin.y, p.pos.y), std::min(m.boundsMin.z, p.pos.z)};
        m.boundsMax = {std::max(m.boundsMax.x, p.pos.x), std::max(m.boundsMax.y, p.pos.y), std::max(m.boundsMax.z, p.pos.z)};
    }
    return true;
}

// ------------------------------------------------------------------ project

void App::NewPaint3DProject() {
    auto& P = p3dNew;
    const u32 res = P.resolution == 0 ? 1024 : P.resolution == 1 ? 2048 : 4096;
    MeshData mesh;
    if (P.mesh == 3 && Narrow(Utf8Path(P.meshPath).extension().wstring()) == ".uasset") {
        // Unreal mesh: exported by Unreal Editor first, then the same settings with the glTF.
        auto settings = P;
        ConvertUAssetAsync(P.meshPath, [this, settings](const std::string& glb) {
            auto saved = p3dNew;
            p3dNew = settings;
            p3dNew.meshPath = glb;
            NewPaint3DProject();
            p3dNew = saved;
        });
        return;
    }
    if (P.mesh == 3) {
        std::string err;
        if (!LoadMesh(P.meshPath, mesh, &err)) { Status("Maillage : " + err); return; }
    } else {
        mesh = P.mesh == 1 ? MakeCube() : P.mesh == 2 ? MakePlane() : MakeSphere();
    }
    if (P.autoUv || !mesh.hasUvs) {
        // New UVs before anything is painted (models without UVs, or overlapping ones).
        UnwrapOptions o;
        o.resolution = (int)res;
        UnwrapResult r;
        if (UnwrapMesh(mesh, o, r)) mesh = std::move(r.mesh);
        else Status("UV automatiques : " + r.error);
    }
    std::string name = (mesh.name.empty() ? std::string("Projet") : mesh.name) + " (3D)";
    NewDocument(res, res, Precision::Half16, DocMode::Layers, 3, name);
    DocView* v = Active();
    if (!v) return;
    Document& d = *v->doc;
    d.kind = 1;
    d.pbrChannels = 0;
    for (int c = 0; c < (int)PbrChannel::Count; ++c) if (P.channels[c]) d.pbrChannels |= 1u << c;
    d.pbrChannels |= 1u;  // base color always
    d.exportTemplate = P.exportTemplate;
    AttachMesh(*v, std::move(mesh));
    CreatePbrChannels(*v, true);
    // Base material (full mask) + an empty paint layer on top.
    AddMaterialLayer(*v, std::clamp(P.basePreset, 0, MaterialPresetCount() - 1));
    if (Layer* l = AnyMaterialLayer(d, materialSel); l && l->mask) {
        v->renderer.Prepare(d, *ctx);  // the mask's initial clear first
        v->renderer.FillSelection(d, *ctx, nullptr, l->mask, 0, {1, 1, 1, 1}, 1.0f, "Masque", nullptr, nullptr, false);
    }
    if (!P.skipPaintLayer) AddPaintLayer3D(*v, "Peinture 1");
    v->history.Clear();
    d.dirty = false;
    Status("Projet Paint 3D : " + std::to_string(res) + " px. Peignez sur le modèle (canaux du pinceau : panneau Matériau du pinceau).");
}

Id App::AddPaintLayer3D(DocView& v, const std::string& name) {
    Document& d = *v.doc;
    Edit e(d, v.history, "Calque de peinture 3D");
    Id material = d.NewId();
    Layer* first = nullptr;
    for (PbrChannel c : kPaintChannels) {
        Id g = FindChannelGroup(d, c);
        if (!g) continue;
        Layer p;
        p.id = d.NewId();
        p.kind = LayerKind::Pixel;
        p.name = name;
        p.parent = g;
        p.material = material;
        p.surface = d.CreateSurface({0, 0, 0, 0});
        auto& L = d.S().layers;
        int gi = d.LayerIndex(g);
        L.insert(L.begin() + gi, p);
        if (c == PbrChannel::BaseColor || !first) first = &L[gi];
    }
    if (first) {
        d.S().activeLayer = first->id;
        d.S().editMask = false;
    }
    d.SyncGraph();
    e.Commit();
    materialSel = material;
    return material;
}

bool App::IsPaintLayer3D(Document& d, Id material) {
    for (auto& l : d.S().layers)
        if (l.material == material && l.kind == LayerKind::Pixel) return true;
    return false;
}

std::vector<Id> App::Paint3DTargets(DocView& v, std::vector<float4>& colors) {
    // Surfaces painted by a stroke in a project, with the value of each channel.
    std::vector<Id> out;
    colors.clear();
    Document& d = *v.doc;
    Layer* al = d.ActiveLayer();
    if (!al) return out;
    if (!al->material || !IsPaintLayer3D(d, al->material) || d.S().editMask) {
        // Fill material (shared mask) or a plain layer: single target.
        if (Id t = d.PaintTarget()) {
            out.push_back(t);
            colors.push_back(PaintColor(d));
        }
        return out;
    }
    for (PbrChannel c : kPaintChannels) {
        if (!brush3D.use[(int)c]) continue;
        Layer* l = MaterialChannelLayer(d, al->material, c);
        if (!l || !l->surface) continue;
        float4 col;
        switch (c) {
            case PbrChannel::BaseColor: col = LinearColor(fg); break;
            case PbrChannel::Emissive: col = {SrgbToLinear(brush3D.emissive.x), SrgbToLinear(brush3D.emissive.y), SrgbToLinear(brush3D.emissive.z), 1}; break;
            default: {
                float g = c == PbrChannel::Roughness ? brush3D.rough : c == PbrChannel::Metallic ? brush3D.metal : brush3D.height;
                float lin = SrgbToLinear(g);  // data channels hold visual gray levels
                col = {lin, lin, lin, 1};
            }
        }
        // Base color first: it carries the decal colors.
        if (c == PbrChannel::BaseColor) {
            out.insert(out.begin(), l->surface);
            colors.insert(colors.begin(), col);
        } else {
            out.push_back(l->surface);
            colors.push_back(col);
        }
    }
    return out;
}

// ------------------------------------------------------------------ panels

void App::DrawLayers3DPanel() {
    if (!ImGui::Begin("Calques 3D", PanelFlag("Calques 3D"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v || v->doc->kind != 1) {
        ImGui::TextWrapped("Panneau des projets Paint 3D (Fichier > Nouveau projet Paint 3D...).");
        ImGui::End();
        return;
    }
    Document& d = *v->doc;
    if (ImGui::Button("+ Peinture")) AddPaintLayer3D(*v, "Peinture " + std::to_string(MaterialIds(d).size() + 1));
    ui::HelpTooltip("Calque de peinture : un calque par canal, peints ensemble avec les valeurs du pinceau.");
    ImGui::SameLine();
    if (ImGui::Button("+ Matériau \xE2\x96\xBE")) ImGui::OpenPopup("##addmat3d");
    ui::HelpTooltip("Calque de remplissage : un matériau (valeur par canal) et un masque partagé à peindre.");
    if (ImGui::BeginPopup("##addmat3d")) {
        if (ImGui::MenuItem("Matériau projeté (image)...")) {
            std::string f = platform::OpenFileDialog(hwnd, L"Images (texture qui se répète)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0");
            if (!f.empty()) AddProjectedMaterial(*v, f, Narrow(Utf8Path(f).stem().wstring()));
        }
        if (ImGui::BeginMenu("Matériau projeté (motif)")) {
            if (!patternsScanned) ScanPatterns();
            for (auto& pt : patterns)
                if (ImGui::MenuItem(pt.name.c_str())) AddProjectedMaterial(*v, "pattern:" + pt.name, pt.name);
            if (patterns.empty()) ImGui::TextDisabled("Aucun motif (panneau Motifs)");
            ImGui::EndMenu();
        }
        ImGui::Separator();
        for (int i = 0; i < MaterialPresetCount(); ++i)
            if (ImGui::MenuItem(MaterialPresetName(i))) AddMaterialLayer(*v, i);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Intelligent \xE2\x96\xBE") || std::exchange(smartGridRequest, false)) ImGui::OpenPopup("##addsmart3d");
    ui::HelpTooltip("Matériau intelligent : plusieurs couches dont les masques suivent la forme du modèle\n(usure des arêtes, saleté des creux, poussière, rouille...).");
    if (ImGui::BeginPopup("##addsmart3d")) {
        if (DrawSmartMaterialGrid(*v)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    std::vector<Id> mats = MaterialIds(d);
    if (Layer* al = d.ActiveLayer(); al && al->material) materialSel = al->material;
    ImGui::SameLine();
    ImGui::BeginDisabled(!materialSel);
    auto moveMat = [&](int dir) {
        Edit e(d, v->history, "Déplacer le calque 3D");
        std::vector<Id> ids;
        for (auto& l : d.S().layers) if (l.material == materialSel) ids.push_back(l.id);
        for (Id id : ids) d.MoveLayerSibling(id, dir);
        e.Commit();
    };
    if (ImGui::ArrowButton("##up", ImGuiDir_Up)) moveMat(1);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##down", ImGuiDir_Down)) moveMat(-1);
    ImGui::SameLine();
    bool del = ImGui::Button("Supprimer");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("+ Dossier")) CreateFolder3D(*v, IsFolder3D(d, materialSel) ? 0 : materialSel);
    ui::HelpTooltip("Dossier de calques 3D : visibilité, opacité, fusion et masque (intelligent) communs.\nAutour du calque sélectionné, ou vide en haut.");
    ImGui::Separator();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float hh = ImGui::GetFrameHeight() * 1.5f;
    static std::string mat3dFilter;
    ui::ListFilter(mat3dFilter, "Filtrer les calques 3D (Ctrl+F)...");
    for (Id m : mats) {
        Layer* any = AnyMaterialLayer(d, m);
        if (!any) continue;
        if (!mat3dFilter.empty() && !ui::FilterMatch(mat3dFilter, any->name) && any->kind != LayerKind::Group) continue;
        const bool paint = IsPaintLayer3D(d, m);
        const bool folder = any->kind == LayerKind::Group;
        const int depth = Folder3DDepth(d, m);
        if (depth) ImGui::Indent(depth * 18 * dpi);
        ImGui::PushID((int)m);
        bool vis = any->visible;
        if (ImGui::Checkbox("##vis", &vis)) {
            if (ImGui::GetIO().KeyAlt) {
                SoloMaterial(*v, m);
            } else {
                Edit e(d, v->history, "Visibilité du calque 3D");
                for (auto& l : d.S().layers) if (l.material == m) l.visible = vis;
                d.SyncGraph();
                e.Commit();
            }
        }
        ui::HelpTooltip("Afficher / masquer le calque 3D\nAlt + clic : lui seul (encore : tous)");
        ImGui::SameLine();
        // Thumbnail: base color layer (paint) or swatch (fill).
        ImVec2 sp = ImGui::GetCursorScreenPos();
        Layer* bc = MaterialChannelLayer(d, m, PbrChannel::BaseColor);
        if (folder) {
            // Folder: a folder shape.
            const ImU32 fc = IM_COL32(200, 170, 90, 255);
            dl->AddRectFilled(ImVec2(sp.x + hh * 0.1f, sp.y + hh * 0.22f), ImVec2(sp.x + hh * 0.5f, sp.y + hh * 0.36f), fc, 2);
            dl->AddRectFilled(ImVec2(sp.x + hh * 0.1f, sp.y + hh * 0.32f), ImVec2(sp.x + hh * 0.9f, sp.y + hh * 0.82f), fc, 3);
        } else if (paint && bc && bc->surface) {
            if (Surface* s = d.GetSurface(bc->surface)) {
                gpu::Texture* th = v->renderer.Thumbnail(*ctx, (2ull << 58) | m, s->tex.get(), s->version, (u32)hh, (u32)hh);
                dl->AddRectFilled(sp, ImVec2(sp.x + hh, sp.y + hh), IM_COL32(60, 60, 66, 255), 3);
                dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(th), sp, ImVec2(sp.x + hh, sp.y + hh));
            }
        } else {
            float4 col = bc && !bc->effect.params.empty() ? bc->effect.params[0] : float4{0.4f, 0.4f, 0.4f, 1};
            dl->AddRectFilled(sp, ImVec2(sp.x + hh, sp.y + hh), ImGui::ColorConvertFloat4ToU32(ImVec4(LinearToSrgb(col.x), LinearToSrgb(col.y), LinearToSrgb(col.z), 1)), 3);
        }
        dl->AddRect(sp, ImVec2(sp.x + hh, sp.y + hh), ui::col::Border, 3);
        ImGui::Dummy(ImVec2(hh, hh));
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (ImGui::Selectable(any->name.c_str(), materialSel == m, 0, ImVec2(0, hh * 0.55f))) {
            materialSel = m;
            if (paint) {
                Layer* t = bc ? bc : any;
                d.S().activeLayer = t->id;
                d.S().editMask = false;
            } else {
                SelectMaterial(d, m);
            }
        }
        // Channels this layer drives.
        std::string chans;
        for (PbrChannel c : kPaintChannels)
            if (MaterialChannelLayer(d, m, c)) {
                static const char* abbr[] = {"C", "R", "M", "N", "E", "O", "H"};
                chans += std::string(abbr[(int)c]) + " ";
            }
        if (folder) ImGui::TextDisabled("Dossier%s", any->mask ? " + masque" : "");
        else ImGui::TextDisabled("%s  %s%s", paint ? "Peinture" : "Remplissage", chans.c_str(), any->mask ? " + masque" : "");
        ImGui::EndGroup();
        ImGui::PopID();
        if (depth) ImGui::Unindent(depth * 18 * dpi);
    }
    if (del && materialSel) {
        Edit e(d, v->history, "Supprimer le calque 3D");
        std::vector<Id> ids;
        for (auto& l : d.S().layers) if (l.material == materialSel) ids.push_back(l.id);
        for (Id id : ids) d.RemoveLayer(id);
        e.Commit();
        materialSel = 0;
    }
    // Selected layer: opacity, mask tools.
    if (Layer* any = materialSel ? AnyMaterialLayer(d, materialSel) : nullptr) {
        ImGui::SeparatorText(any->kind == LayerKind::Group ? "Dossier sélectionné" : "Calque sélectionné");
        {
            // Folders: in / out, ungroup.
            const Id in = Folder3DOf(d, materialSel);
            Id above = 0;  // the folder just above in the list (same level)
            for (size_t k = 0; k < mats.size(); ++k)
                if (mats[k] == materialSel) {
                    for (size_t j = k; j-- > 0;)
                        if (Folder3DOf(d, mats[j]) == in) { above = IsFolder3D(d, mats[j]) ? mats[j] : 0; break; }
                    break;
                }
            bool any3 = false;
            if (above && above != materialSel) { if (ImGui::SmallButton("Entrer dans le dossier au-dessus")) MoveIntoFolder3D(*v, materialSel, above); any3 = true; }
            if (in) { if (any3) ImGui::SameLine(); if (ImGui::SmallButton("Sortir du dossier")) MoveOutOfFolder3D(*v, materialSel); any3 = true; }
            if (any->kind == LayerKind::Group) { if (any3) ImGui::SameLine(); if (ImGui::SmallButton("Dégrouper")) UngroupFolder3D(*v, materialSel); }
        }
        any = materialSel ? AnyMaterialLayer(d, materialSel) : nullptr;
        if (!any) { ImGui::End(); return; }
        float op = any->opacity * 100;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##op3d", &op, 0, 100, "Opacité %.0f %%")) {
            for (auto& l : d.S().layers) if (l.material == materialSel) l.opacity = op / 100;
            d.SyncGraph();
        }
        TrackUndo("Opacité du calque 3D");
        {
            // Blend mode of the layer in every channel it drives.
            int bm = (int)any->blend;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##blend3d", (std::string("Fusion : ") + BlendModeName(any->blend)).c_str())) {
                for (int k = 0; k < (int)BlendMode::Count; ++k)
                    if (ImGui::Selectable(BlendModeName((BlendMode)k), k == bm)) {
                        Edit e(d, v->history, "Fusion du calque 3D");
                        for (auto& l : d.S().layers) if (l.material == materialSel) l.blend = (BlendMode)k;
                        d.SyncGraph();
                        e.Commit();
                    }
                ImGui::EndCombo();
            }
        }
        DrawProjectionEditor(*v, materialSel);
        DrawMaterialFilters(*v, materialSel);
        if (any->mask) DrawSmartMaskEditor(*v, materialSel);
        if (any->mask && !any->generator.empty() && any->maskEdit) {
            // Smart mask: brush strokes are corrections over it (kept when it is generated again).
            if (ImGui::SmallButton("Peindre des retouches")) SelectMaterial(d, materialSel);
            ImGui::SameLine();
            if (ImGui::SmallButton("Effacer les retouches"))
                v->renderer.FillSelection(d, *ctx, &v->history, any->maskEdit, 1, {0, 0, 0, 0}, 1, "Effacer les retouches du masque", nullptr, nullptr, false);
            ImGui::TextDisabled("Pinceau : ajoute le matériau, gomme : l'enlève.\nLes retouches restent quand le masque est recalculé.");
        } else if (any->mask) {
            if (ImGui::SmallButton("Masque : tout")) v->renderer.FillSelection(d, *ctx, &v->history, any->mask, 0, {1, 1, 1, 1}, 1, "Remplir le masque", nullptr, nullptr, false);
            ImGui::SameLine();
            if (ImGui::SmallButton("rien")) v->renderer.FillSelection(d, *ctx, &v->history, any->mask, 0, {0, 0, 0, 1}, 1, "Vider le masque", nullptr, nullptr, false);
            ImGui::SameLine();
            if (ImGui::SmallButton("Peindre le masque")) SelectMaterial(d, materialSel);
            ImGui::TextDisabled("Pinceau blanc : applique le matériau, noir : l'enlève.");
        } else {
            ImGui::TextDisabled("Pinceau / projection : peint les canaux cochés\ndans Matériau du pinceau.");
            if (ImGui::SmallButton("Remplir avec le matériau du pinceau")) {
                // Every checked channel of the layer, filled with the brush value (inside the selection if any).
                std::vector<float4> colors;
                std::vector<Id> targets = Paint3DTargets(*v, colors);
                v->history.BeginGroup();
                for (size_t i = 0; i < targets.size(); ++i)
                    v->renderer.FillSelection(d, *ctx, &v->history, targets[i], 0, colors[i], 1.0f, "Remplir le calque 3D");
                v->history.EndGroup();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Effacer")) {
                std::vector<float4> colors;
                std::vector<Id> targets = Paint3DTargets(*v, colors);
                v->history.BeginGroup();
                for (Id t : targets) v->renderer.FillSelection(d, *ctx, &v->history, t, 1, {}, 1.0f, "Effacer le calque 3D");
                v->history.EndGroup();
            }
        }
        ImGui::SetNextItemWidth(120 * dpi);
        ImGui::Combo("Symétrie (pinceau 3D)", &v->renderer.meshSymmetry, "Aucune\0Axe X\0Axe Y\0Axe Z\0");
        ui::HelpTooltip("Chaque trait est aussi peint de l'autre côté du modèle (miroir sur l'axe choisi de l'objet).");
    }
    ImGui::End();
}

void App::SoloMaterial(DocView& v, Id m) {
    // Only this 3D layer shown; again on the solo layer: every layer shown.
    Document& d = *v.doc;
    Layer* any = AnyMaterialLayer(d, m);
    if (!any) return;
    bool solo = any->visible;
    for (Id o : MaterialIds(d))
        if (o != m)
            if (Layer* ol = AnyMaterialLayer(d, o); ol && ol->visible) solo = false;
    // Its folders stay visible (a hidden folder hides what it holds).
    std::vector<Id> keep = {m};
    for (Id f = Folder3DOf(d, m); f && keep.size() < 16; f = Folder3DOf(d, f)) keep.push_back(f);
    Edit e(d, v.history, solo ? "Afficher tous les calques 3D" : "Calque 3D seul");
    for (auto& l : d.S().layers)
        if (l.material) l.visible = solo || std::find(keep.begin(), keep.end(), l.material) != keep.end();
    d.SyncGraph();
    e.Commit();
}

void App::DrawMaterialFilters(DocView& v, Id material) {
    // Adjustments of a 3D layer (non destructive filters of its channel layers), per channel.
    Document& d = *v.doc;
    static const std::pair<const char*, const char*> kFilters[] = {
        {"hue_saturation", "Teinte / saturation"}, {"brightness_contrast", "Luminosité / contraste"}, {"levels", "Niveaux"}, {"curves", "Courbes"},
        {"color_balance", "Balance des couleurs"}, {"gaussian_blur", "Flou"}, {"sharpen", "Netteté"}, {"noise", "Bruit"}, {"invert", "Inverser"}};
    static const std::pair<PbrChannel, const char*> kChannels[] = {{PbrChannel::BaseColor, "Couleur"}, {PbrChannel::Roughness, "Rugosité"},
                                                                    {PbrChannel::Metallic, "Métal"}, {PbrChannel::Height, "Hauteur"}, {PbrChannel::Emissive, "Émissif"}};
    ImGui::SeparatorText("Filtres");
    bool changed = false;
    Id removeLayer = 0;
    int removeIndex = -1;
    for (auto& [c, cname] : kChannels) {
        Layer* l = MaterialChannelLayer(d, material, c);
        if (!l) continue;
        for (int i = 0; i < (int)l->filters.size(); ++i) {
            EffectInstance& fx = l->filters[i];
            ImGui::PushID((int)l->id * 64 + i);
            auto def = fx.Def();
            const std::string title = std::string(def ? def->name.c_str() : fx.effectId.c_str()) + "  (" + cname + ")";
            const bool open = ImGui::TreeNodeEx("##fx", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", title.c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
            if (ImGui::SmallButton("x")) { removeLayer = l->id; removeIndex = i; }
            if (open) {
                if (EffectParamsUI(fx, title.c_str())) changed = true;
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if (ImGui::SmallButton("+ Filtre")) ImGui::OpenPopup("##addfx3d");
    if (ImGui::BeginPopup("##addfx3d")) {
        for (auto& [id, name] : kFilters) {
            if (!Effects().Find(id)) continue;
            if (!ImGui::BeginMenu(name)) continue;
            auto add = [&](int which) {
                Edit e(d, v.history, std::string("Filtre : ") + name);
                for (auto& [c, cname] : kChannels) {
                    if (which >= 0 && kChannels[which].first != c) continue;
                    if (Layer* l = MaterialChannelLayer(d, material, c)) {
                        EffectInstance fx;
                        fx.effectId = id;
                        fx.params = Effects().Find(id)->DefaultParams();
                        l->filters.push_back(fx);
                    }
                }
                d.SyncGraph();
                e.Commit();
            };
            for (int k = 0; k < (int)(sizeof(kChannels) / sizeof(kChannels[0])); ++k)
                if (MaterialChannelLayer(d, material, kChannels[k].first) && ImGui::MenuItem(kChannels[k].second)) add(k);
            ImGui::Separator();
            if (ImGui::MenuItem("Tous les canaux")) add(-1);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ui::HelpTooltip("Ajustements non destructifs du calque 3D, par canal (teinte de la couleur, contraste de la rugosité, flou du relief...).");
    if (removeLayer) {
        Edit e(d, v.history, "Supprimer le filtre");
        if (Layer* l = d.FindLayer(removeLayer); l && removeIndex < (int)l->filters.size()) l->filters.erase(l->filters.begin() + removeIndex);
        d.SyncGraph();
        e.Commit();
    }
    if (changed) d.SyncGraph();
    TrackUndo("Réglage du filtre");
}

void App::DrawBrushMaterialPanel() {
    if (!ImGui::Begin("Matériau du pinceau", PanelFlag("Matériau du pinceau"))) { ImGui::End(); return; }
    ui::FilterBox();
    // A 3D tool just chosen: its section is scrolled into view (they sit at the bottom of the panel).
    static Tool seenTool = Tool::Brush;
    const bool toolJump = tool != seenTool;
    seenTool = tool;
    auto section = [&](const char* title, Tool t) {
        if (tool == t) {
            if (toolJump) ImGui::SetScrollHereY(0.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ui::col::Accent);
        }
        ImGui::SeparatorText(title);
        if (tool == t) ImGui::PopStyleColor();
    };
    auto& B = brush3D;
    ImGui::TextDisabled("Valeurs peintes dans chaque canal d'un calque de peinture.");
    if (ImGui::BeginCombo("Préréglage", "Choisir...")) {
        for (int i = 0; i < MaterialPresetCount(); ++i) {
            if (!ImGui::Selectable(MaterialPresetName(i))) continue;
            float3 c;
            float r, m, h;
            bool hc, hr, hm, hh;
            MaterialPresetValues(i, c, r, m, h, hc, hr, hm, hh);
            if (hc) fg = {c.x, c.y, c.z, 1};
            B.use[(int)PbrChannel::BaseColor] = hc;
            B.use[(int)PbrChannel::Roughness] = hr;
            B.use[(int)PbrChannel::Metallic] = hm;
            B.use[(int)PbrChannel::Height] = hh;
            B.rough = r;
            B.metal = m;
            B.height = h;
        }
        ImGui::EndCombo();
    }
    auto row = [&](PbrChannel c, const char* label) {
        ImGui::PushID((int)c);
        ImGui::Checkbox(label, &B.use[(int)c]);
        ImGui::SameLine(150 * dpi);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::BeginDisabled(!B.use[(int)c]);
        switch (c) {
            case PbrChannel::BaseColor: ImGui::ColorEdit3("##v", &fg.x); break;
            case PbrChannel::Roughness: ImGui::SliderFloat("##v", &B.rough, 0, 1, "%.2f"); break;
            case PbrChannel::Metallic: ImGui::SliderFloat("##v", &B.metal, 0, 1, "%.2f"); break;
            case PbrChannel::Height: ImGui::SliderFloat("##v", &B.height, 0, 1, B.height > 0.5f ? "%.2f (relief)" : B.height < 0.5f ? "%.2f (creux)" : "%.2f"); break;
            case PbrChannel::Emissive: ImGui::ColorEdit3("##v", &B.emissive.x); break;
            default: break;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    };
    row(PbrChannel::BaseColor, "Couleur");
    row(PbrChannel::Roughness, "Rugosité");
    row(PbrChannel::Metallic, "Métal");
    row(PbrChannel::Height, "Hauteur");
    row(PbrChannel::Emissive, "Émissif");

    // The brush on the model: tip, dynamics, stroke helpers.
    ImGui::SeparatorText("Pinceau 3D");
    {
        BrushSettings& bs = tool == Tool::Eraser ? eraser : brush;
        auto td = TipDef(bs);
        const std::string tipName = td && td->def ? td->def->name : "Rond (dureté)";
        ImGui::Checkbox("Pointe du pinceau", &B.tip);
        ui::HelpTooltip("Coché : la forme du pinceau choisi (panneau Pinceaux, ou la liste à droite) est projetée sur le modèle,\navec ses variantes ; décoché : disque rond avec la dureté.");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##tip3d", tipName.c_str(), ImGuiComboFlags_HeightLarge)) {
            if (ImGui::Selectable("Rond (dureté)", !td)) SelectTip(bs, "");
            std::string cat;
            for (auto& b : Brushes().All()) {
                if (!b->def) continue;
                if (b->def->category != cat) { cat = b->def->category; ImGui::SeparatorText(cat.c_str()); }
                if (ImGui::Selectable((b->def->name + "##" + b->id).c_str(), td && td->id == b->id)) { SelectTip(bs, b->id); B.tip = true; }
            }
            ImGui::EndCombo();
        }
        ImGui::SliderFloat("Taille", &bs.size, 1, 600, "%.0f px", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Flux", &bs.flow, 0.01f, 1, "%.2f");
        ImGui::SliderFloat("Espacement", &bs.spacing, 0.02f, 2.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        if (ImGui::TreeNode("Dynamique")) {
            ImGui::SliderFloat("Angle", &B.angle, -180, 180, "%.0f°");
            ImGui::Checkbox("Suivre le trait", &B.follow);
            ui::HelpTooltip("La pointe tourne dans le sens du trait (rayures, coutures, traînées).");
            ImGui::SliderFloat("Variation d'angle", &B.jitterAngle, 0, 1, "%.2f");
            ImGui::SliderFloat("Variation de taille", &B.jitterSize, 0, 1, "%.2f");
            ImGui::SliderFloat("Dispersion", &B.scatter, 0, 3, "%.2f");
            ui::HelpTooltip("Chaque touche est déplacée au hasard autour du trait (en diamètres).");
            ImGui::SliderFloat("Variation de flux", &B.jitterFlow, 0, 1, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Trait")) {
            ImGui::SliderFloat("Lissage (fil)", &B.lazy, 0, 120, B.lazy > 0 ? "%.0f px" : "désactivé");
            ui::HelpTooltip("Le pinceau suit le curseur au bout d'un fil : traits réguliers.");
            if (DocView* v = Active(); v)
                ImGui::SliderFloat("Angle limite", &v->renderer.meshFacingMin, -0.2f, 0.9f, "%.2f");
            ui::HelpTooltip("Les zones vues trop de biais ne sont pas peintes (0 : jusqu'au bord, plus haut : seulement les faces bien de face).");
            ImGui::TextDisabled("Maj+clic : ligne droite depuis la fin du dernier trait.");
            ImGui::TreePop();
        }
    }
    section("Remplissage par polygone", Tool::PolyFill);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::Combo("##polymode", &B.polyMode, "Triangle\0Face (polygone plat)\0Île UV\0Pièce du maillage\0Tout le modèle\0");
    if (ImGui::Button("Outil Remplissage par polygone")) tool = Tool::PolyFill;
    ui::HelpTooltip("Clic sur le modèle : remplit l'élément sous le curseur avec le matériau du pinceau\n(ou le masque du matériau). Ctrl+clic : efface.");
    section("Simulation (particules)", Tool::Particles);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##simpreset", SimPresetName(B.simPreset))) {
        for (int i = 0; i < SimPresetCount(); ++i) {
            if (ImGui::Selectable(SimPresetName(i), i == B.simPreset)) B.simPreset = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", SimPresetHelp(i));
        }
        ImGui::EndCombo();
    }
    ImGui::SliderFloat("Particules / s", &B.simRate, 5, 400, "%.0f", ImGuiSliderFlags_Logarithmic);
    float sz = B.simSize * 100;
    if (ImGui::SliderFloat("Taille des gouttes", &sz, 0.2f, 10, "%.1f %%", ImGuiSliderFlags_Logarithmic)) B.simSize = sz / 100;
    ImGui::SliderFloat("Gravité", &B.simGravity, 0, 3, "%.2f");
    ImGui::SliderFloat("Adhérence", &B.simAdhesion, 0, 2, "%.2f");
    ImGui::SliderFloat("Durée de vie", &B.simLife, 0.2f, 10, "%.1f s");
    if (B.simPreset == 3) ImGui::SliderFloat("Dispersion", &B.simSpread, 0, 2, "%.2f");
    if (ImGui::Button("Outil Simulation")) tool = Tool::Particles;
    ui::HelpTooltip("Maintenez le clic sur le modèle : les particules partent du pinceau (sa taille fixe la zone)\net peignent le matériau du pinceau là où elles passent, même hors de la vue.");
    section("Peinture liquide", Tool::Fluid);
    if (tool == Tool::Fluid) {
        DrawFluidOptions(true);
    } else {
        ImGui::TextDisabled("Coulures simulées : tous les canaux du matériau à la fois, avec la gravité.");
        if (ImGui::Button("Outil Peinture liquide")) tool = Tool::Fluid;
    }
    section("Projection (décalque)", Tool::Projection);
    std::string dn = decal.path.empty() ? "(aucune image)" : Narrow(Utf8Path(decal.path).filename().wstring());
    ImGui::TextUnformatted(dn.c_str());
    if (ImGui::Button("Choisir une image...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0");
        if (!f.empty()) LoadDecal(f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Outil Projection")) tool = Tool::Projection;
    ImGui::SliderFloat("Taille", &decal.size, 0.05f, 2.0f, "%.2f");
    ImGui::SliderFloat("Rotation", &decal.rotation, -180, 180, "%.0f°");
    ImGui::SliderFloat("Opacité##dec", &decal.opacity, 0, 1, "%.2f");
    ImGui::Checkbox("Couleurs de l'image", &decal.imageColor);
    ui::HelpTooltip("Coché : la couleur de base reçoit l'image ; sinon la couleur du pinceau, l'image servant de pochoir.");
    ImGui::Checkbox("Pochoir (peindre à travers l'image)", &decal.stencil);
    ui::HelpTooltip("Avec le pinceau ou la gomme : l'image reste à l'écran et le pinceau ne peint qu'à travers elle\n(S + glisser : la déplacer, Ctrl / Maj + molette : taille / rotation).");
    if (decal.stencil) {
        ImGui::SameLine();
        ImGui::Checkbox("Inverser##stencil", &decal.stencilInvert);
        ImGui::SameLine();
        ImGui::Checkbox("Luminosité", &decal.stencilLuminance);
        ui::HelpTooltip("Coché : les zones claires de l'image laissent passer la peinture ; sinon seulement sa transparence.");
        if (decal.stencilLuminance) {
            ImGui::SliderFloat("Seuil##stencil", &decal.stencilThreshold, 0, 1, "%.2f");
            ui::HelpTooltip("Luminosité de l'image qui laisse passer la moitié de la peinture.");
            ImGui::SliderFloat("Contraste##stencil", &decal.stencilContrast, 0.5f, 30, "%.1f", ImGuiSliderFlags_Logarithmic);
            ui::HelpTooltip("Élevé : pochoir franc (noir ou blanc), même avec une image aux tons moyens.");
        }
    }
    ImGui::TextDisabled("Outil Projection : clic sur le modèle pour poser le décalque\n(molette + Ctrl : taille, Maj : rotation).");
    ImGui::End();
}

bool App::LoadDecal(const std::string& path) {
    int w = 0, h = 0, c = 0;
    u8* px = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!px) { Status("Image illisible : " + path); return false; }
    decal.tex = gpu::Dev().CreateTexture((u32)w, (u32)h, gpu::Format::RGBA8, "Decal");
    ctx->Upload(decal.tex.get(), px, (u32)w * 4);
    stbi_image_free(px);
    decal.w = w;
    decal.h = h;
    decal.path = path;
    return true;
}

// ------------------------------------------------------------------ exports by template

void App::ExportProjectTextures(DocView& v, const std::string& basePath) {
    Document& d = *v.doc;
    if (basePath.empty()) return;
    if (d.exportTemplate == 0) { ExportPbrTextures(v, basePath); return; }
    // Packed maps: ORM (Unreal: R occlusion, G roughness, B metal) or Unity mask map (R metal, G occlusion, A smoothness).
    auto p = std::filesystem::absolute(Utf8Path(basePath));
    std::string stem = Narrow((p.parent_path() / p.stem()).wstring());
    v.renderer.EvaluateComplete(d, *ctx);
    auto rb = [&](PbrChannel c) -> gpu::ReadbackPtr {
        gpu::Texture* t = ChannelTexture(v, c);
        return t ? v.renderer.ExportRGBA8(*ctx, t) : nullptr;
    };
    PackJob job;
    job.unity = d.exportTemplate == 2;
    job.path = stem + (job.unity ? "_MaskMap.png" : "_ORM.png");
    job.ao = rb(PbrChannel::Occlusion);
    job.rough = rb(PbrChannel::Roughness);
    job.metal = rb(PbrChannel::Metallic);
    packJobs.push_back(job);
    // Base color, normal (height baked in) and emissive as separate maps.
    gpu::Texture* n = ChannelTexture(v, PbrChannel::Normal);
    gpu::Texture* h = ChannelTexture(v, PbrChannel::Height);
    gpu::TexturePtr baked = h ? v.renderer.BakeNormal(*ctx, n, h, d.Width(), d.Height()) : nullptr;
    auto put = [&](gpu::Texture* t, const char* suffix) {
        if (t) if (auto r = v.renderer.ExportRGBA8(*ctx, t)) exports.push_back({stem + suffix, r, {}});
    };
    put(ChannelTexture(v, PbrChannel::BaseColor), job.unity ? "_BaseMap.png" : "_BaseColor.png");
    put(baked ? baked.get() : n, "_Normal.png");
    if (d.pbrChannels & (1u << (int)PbrChannel::Emissive)) put(ChannelTexture(v, PbrChannel::Emissive), "_Emissive.png");
    v.renderer.DropNodeCaches(d);
    if (!job.unity) {
        // Manifest read by the Unreal plugin (Tools > Shaderixel > Importer un export Shaderixel...).
        Json m = Json::Object();
        m["format"] = "shaderixel-textures";
        m["version"] = 1;
        m["name"] = Narrow(p.stem().wstring());
        Json texs = Json::Array();
        auto add = [&](const char* role, const char* suffix) {
            Json t = Json::Object();
            t["role"] = role;
            t["file"] = Narrow(p.stem().wstring()) + suffix;
            texs.Push(t);
        };
        add("BaseColor", "_BaseColor.png");
        add("Normal", "_Normal.png");
        add("ORM", "_ORM.png");
        if (d.pbrChannels & (1u << (int)PbrChannel::Emissive)) add("Emissive", "_Emissive.png");
        m["textures"] = texs;
        std::ofstream(Utf8Path(stem + ".sxue.json"), std::ios::binary) << m.Dump(1);
    }
    Status("Export des textures : " + stem + "_*.png" + (job.unity ? "" : " (+ .sxue.json pour le plugin Unreal)"));
}

void App::UpdatePaint3D() {
    PollMeshMapsBake();
    PollBake();
    PollTextureExports();
    PollTurntable();
    PollSmartMaterialSave();
    PollMaterialPick();
    for (size_t i = 0; i < packJobs.size();) {
        PackJob& j = packJobs[i];
        auto ready = [](const gpu::ReadbackPtr& r) { return !r || r->ready; };
        if (!ready(j.ao) || !ready(j.rough) || !ready(j.metal)) { ++i; continue; }
        const gpu::Readback* ref = j.rough ? j.rough.get() : j.ao ? j.ao.get() : j.metal.get();
        if (ref) {
            const u32 W = ref->width, H = ref->height;
            std::vector<u8> px((size_t)W * H * 4, 255);
            auto ch = [&](const gpu::ReadbackPtr& r, size_t p, u8 def) { return r && r->width == W ? r->data[p * 4] : def; };
            for (size_t p = 0; p < (size_t)W * H; ++p) {
                u8 ao = ch(j.ao, p, 255), ro = ch(j.rough, p, 128), me = ch(j.metal, p, 0);
                if (j.unity) { px[p * 4] = me; px[p * 4 + 1] = ao; px[p * 4 + 2] = 0; px[p * 4 + 3] = (u8)(255 - ro); }
                else { px[p * 4] = ao; px[p * 4 + 1] = ro; px[p * 4 + 2] = me; px[p * 4 + 3] = 255; }
            }
            stbi_write_png(j.path.c_str(), (int)W, (int)H, 4, px.data(), (int)W * 4);
            Status("Exporté : " + j.path);
        }
        packJobs.erase(packJobs.begin() + i);
    }
}

void App::DrawPaint3DNewDialog() {
    auto& P = p3dNew;
    if (!P.open) return;
    ImGui::SetNextWindowSize(ImVec2(460 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Nouveau projet Paint 3D", &P.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ImGui::SeparatorText("Maillage");
    ImGui::RadioButton("Sphère", &P.mesh, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Cube", &P.mesh, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Plan", &P.mesh, 2);
    ImGui::SameLine();
    ImGui::RadioButton("Fichier", &P.mesh, 3);
    if (P.mesh == 3) {
        ImGui::SetNextItemWidth(300 * dpi);
        ImGui::InputText("##meshpath", &P.meshPath);
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            std::string f = platform::OpenFileDialog(hwnd, L"Maillages (*.obj, *.fbx, *.gltf, *.glb, *.uasset)\0*.obj;*.fbx;*.gltf;*.glb;*.uasset\0");
            if (!f.empty()) P.meshPath = f;
        }
        ImGui::Checkbox("Générer des UV automatiques (xatlas)", &P.autoUv);
        ui::HelpTooltip("Pour les modèles sans UV ou aux UV superposées. Réglages fins ensuite : Vue 3D > UV automatiques.");
    }
    ImGui::SeparatorText("Texture");
    ImGui::Combo("Résolution", &P.resolution, "1024 x 1024\0" "2048 x 2048\0" "4096 x 4096\0");
    ImGui::SeparatorText("Canaux");
    static const char* names[] = {"Couleur de base", "Rugosité", "Métal", "Normale", "Émissif", "Occlusion (AO)", "Hauteur"};
    for (int c = 0; c < (int)PbrChannel::Count; ++c) {
        if (c % 3) ImGui::SameLine(c % 3 * 150 * dpi);
        ImGui::BeginDisabled(c == 0);
        ImGui::Checkbox(names[c], &P.channels[c]);
        ImGui::EndDisabled();
    }
    ImGui::Combo("Modèle d'export", &P.exportTemplate, "Textures séparées\0Unreal (BaseColor, Normal, ORM)\0Unity HDRP (BaseMap, Normal, MaskMap)\0");
    ImGui::SeparatorText("Matériau de base");
    if (ImGui::BeginCombo("##base", MaterialPresetName(std::clamp(P.basePreset, 0, MaterialPresetCount() - 1)))) {
        for (int i = 0; i < MaterialPresetCount(); ++i) if (ImGui::Selectable(MaterialPresetName(i), i == P.basePreset)) P.basePreset = i;
        ImGui::EndCombo();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(P.mesh == 3 && P.meshPath.empty());
    if (ImGui::Button("Créer", ImVec2(140 * dpi, 0))) {
        P.open = false;
        NewPaint3DProject();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(140 * dpi, 0))) P.open = false;
    ImGui::End();
}

}  // namespace sx
