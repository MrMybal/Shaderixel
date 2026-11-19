// Paint 3D projects (.sx3d): a texture set painted on a mesh, Substance-style.
// The document holds one isolated group per PBR channel; a "3D layer" is a
// material id spread over these groups:
//   - paint layer: one pixel layer per channel, painted together (brush material),
//   - fill layer: one value per channel and a shared mask (material presets).
// Projects have their own workspace layout, a reduced tool set, a decal
// projection tool and texture exports by template (separate maps, ORM...).
#include "ui/App.h"
#include "ui/Lang.h"
#include "ui/Filter.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "doc/Brushes.h"
#include "doc/Effects.h"

#include <fstream>

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>

namespace sx {

namespace {
const PbrChannel kPaintChannels[] = {PbrChannel::BaseColor, PbrChannel::Roughness, PbrChannel::Metallic, PbrChannel::Height, PbrChannel::Emissive, PbrChannel::Normal, PbrChannel::Occlusion, PbrChannel::Opacity,
    PbrChannel::User0, PbrChannel::User1, PbrChannel::User2, PbrChannel::User3};
}

// ------------------------------------------------------------------ project

void App::NewPaint3DProject() {
    auto& P = p3dNew;
    const int tileSize = P.resolution == 0 ? 1024 : P.resolution == 1 ? 2048 : 4096;
    int res = tileSize;
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
    // Several materials over the same UVs: one tile of the texture each (before an unwrap, which replaces them).
    // UDIM layouts (UVs over several 0..1 tiles): each tile becomes a cell of the texture.
    const bool udim = !(P.autoUv || !mesh.hasUvs) && PackUdims(mesh);
    const int materialMode = P.resolutionPerTile && P.tileMode == 0 ? 1 : P.tileMode;
    const bool tiled = !udim && !(P.autoUv || !mesh.hasUvs) && materialMode != 2 && TileMaterialUvs(mesh, res, materialMode);
    if (P.resolutionPerTile && P.mesh == 3 && !P.autoUv) {
        std::string error;
        if (!SizeTextureTiles(mesh, tileSize, P.tileResolutions, res, error)) { P.open = true; Status(error); return; }
        // Four RGBA32F geometry textures are allocated together by the 3D viewport, before the paint layers.
        const u64 budget = gpu::Dev().VideoMemoryBudget(), used = gpu::Dev().VideoMemoryUsage();
        if (res > tileSize && budget && (u64)res * res * 64 > budget - std::min(budget, used)) {
            P.open = true;
            Status("Mémoire vidéo insuffisante pour les cartes d’un atlas de " + std::to_string(res) + " px. Réduisez les résolutions des tuiles.");
            return;
        }
    }
    if (P.autoUv || !mesh.hasUvs) {
        // New UVs before anything is painted (models without UVs, or overlapping ones).
        UnwrapOptions o;
        o.resolution = (int)res;
        UnwrapResult r;
        if (UnwrapMesh(mesh, o, r)) mesh = std::move(r.mesh);
        else { Status("UV automatiques : " + r.error); return; }
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
    if (tiled)
        Status("Projet Paint 3D : " + std::to_string(res) + " px, " + std::to_string(v->mesh->data.materials.size()) +
               " matériaux, une tuile de texture chacun (export : un jeu de textures par matériau).");
    else
        Status("Projet Paint 3D : " + std::to_string(res) + " px. Peignez sur le modèle (canaux du pinceau : panneau Matériau du pinceau).");
}

Id App::AddPaintLayer3D(DocView& v, const std::string& name) {
    Document& d = *v.doc;
    Edit e(d, v.history, "Calque de peinture 3D");
    Id material = d.NewId();
    Id first = 0;  // by id: the next insert reallocates the layer vector
    for (PbrChannel c : kPaintChannels) {
        Id g = NewLayerParent3D(d, c, material);
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
        if (c == PbrChannel::BaseColor || !first) first = p.id;
    }
    if (first) {
        d.S().activeLayer = first;
        d.S().editMask = false;
    }
    d.SyncGraph();
    e.Commit();
    materialSel = material;
    return material;
}

bool App::IsPaintLayer3D(Document& d, Id material) {
    for (auto& l : d.S().layers)
        if (l.material == material && l.kind == LayerKind::Pixel && l.projection.empty()) return true;
    return false;
}

float4 App::BrushChannelValue(PbrChannel c) {
    if (c == PbrChannel::BaseColor) return LinearColor(fg);
    if (c == PbrChannel::Normal) return {SrgbToLinear(brush3D.normal.x), SrgbToLinear(brush3D.normal.y), SrgbToLinear(brush3D.normal.z), 1};
    if (c == PbrChannel::Emissive) return {SrgbToLinear(brush3D.emissive.x), SrgbToLinear(brush3D.emissive.y), SrgbToLinear(brush3D.emissive.z), 1};
    float g = c == PbrChannel::Roughness ? brush3D.rough : c == PbrChannel::Metallic ? brush3D.metal : brush3D.height;
    if (c == PbrChannel::Occlusion) g = brush3D.occlusion;
    if (c == PbrChannel::Opacity) g = brush3D.opacity;
    if (c >= PbrChannel::User0) g = brush3D.user[(int)c - (int)PbrChannel::User0];
    float lin = SrgbToLinear(g);
    return {lin, lin, lin, 1};
}

std::vector<Id> App::Paint3DTargets(DocView& v, std::vector<float4>& colors) {
    // Surfaces painted by a stroke in a project, with the value of each channel.
    std::vector<Id> out;
    colors.clear();
    v.renderer.disabledPaintRegions.clear();
    Document& d = *v.doc;
    Layer* al = d.ActiveLayer();
    if (!al) return out;
    if ((!al->surfacePath.empty() || al->locked) && !d.S().editMask && !d.S().editInpaint) return out;
    if (!al->material || !IsPaintLayer3D(d, al->material) || d.S().editMask || d.S().editInpaint) {
        // Fill material (shared mask) or a plain layer: single target.
        if (Id t = d.PaintTarget()) {
            out.push_back(t);
            colors.push_back(PaintColor(d));
        }
        return out;
    }
    const auto scope = d.LayerTextureSet(*al);
    u32 allowed = d.kind == 1 ? d.TextureSetChannels(scope) : ~0u;
    std::vector<std::pair<Rect,u32>> regions;
    if (!scope.empty() || !d.S().textureSetChannels.empty()) {
        allowed = 0;
        bool foundScope = false;
        for (const auto& [set, rect] : ExportParts(v,true)) {
            const u32 enabled = d.TextureSetChannels(set);
            const bool inside = scope.empty() || set == scope || texexport::SetSuffixes(set).first == scope;
            if (inside) { allowed |= enabled; foundScope = true; }
            regions.push_back({rect,inside ? enabled : 0u});
        }
        if (!foundScope) { allowed = d.TextureSetChannels(scope); regions.clear(); }  // a transferred folder retained its painted mask after a new layout
    }
    for (PbrChannel c : kPaintChannels) {
        if (!brush3D.use[(int)c] || !(allowed & (1u << (int)c))) continue;
        Layer* l = MaterialChannelLayer(d, al->material, c);
        if (!l || !l->surface || l->locked) continue;
        for (const auto& [rect,enabled] : regions)
            if (!(enabled & (1u << (int)c))) v.renderer.disabledPaintRegions[l->surface].push_back(rect);
        const float4 col = BrushChannelValue(c);
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

void App::SetTextureSetChannels(DocView& v, const std::string& set, u32 channels, bool inherit) {
    if (set.empty()) return;
    Document& d = *v.doc;
    const u32 old = d.TextureSetChannels(set);
    v.history.BeginGroup();
    {
        Edit edit(d, v.history, "Canaux du jeu de textures");
        if (inherit) d.S().textureSetChannels.erase(set);
        else d.S().textureSetChannels[set] = channels | 1u;
        edit.Commit();
    }
    const u32 enabled = d.TextureSetChannels(set) & ~old;
    bool needsRoot = false;
    for (PbrChannel c : kPaintChannels)
        if ((enabled & (1u << (int)c)) && !FindChannelGroup(d,c)) needsRoot = true;
    if (needsRoot) CreatePbrChannels(v, true, enabled);
    if (enabled) {
        Edit edit(d, v.history, "Canaux des calques de peinture du jeu");
        const auto ids = MaterialIds(d);
        for (Id id : ids) {
            const Layer* existing = AnyMaterialLayer(d,id);
            if (!existing || !IsPaintLayer3D(d,id)) continue;
            const std::string scope = d.LayerTextureSet(*existing);
            const bool broader = scope.empty() || texexport::SetSuffixes(set).first == scope;
            if (!broader && scope != set && texexport::SetSuffixes(scope).first != set) continue;
            const u32 local = broader ? enabled : enabled & d.TextureSetChannels(scope);
            const std::string name = existing->name;
            for (PbrChannel c : kPaintChannels) {
                if (!(local & (1u << (int)c)) || MaterialChannelLayer(d,id,c)) continue;
                Layer extra;
                extra.parent = NewLayerParent3D(d,c,id);
                if (!extra.parent) continue;
                extra.id = d.NewId();
                extra.kind = LayerKind::Pixel;
                extra.name = name;
                extra.material = id;
                extra.surface = d.CreateSurface({});
                d.S().layers.insert(d.S().layers.begin() + d.LayerIndex(extra.parent), extra);
            }
        }
        d.SyncGraph();
        edit.Commit();
    }
    v.history.EndGroup();
}

void App::DrawLayers3DPanel() {
    if (!ImGui::Begin("Calques 3D", PanelFlag("Calques 3D"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v || v->doc->kind != 1) {
        ImGui::TextWrapped("Panneau des projets Paint 3D (Fichier > Nouveau projet Paint 3D...).");
        ImGui::End();
        return;
    }
    Document& d = *v->doc;
    if (ImGui::Button("Canaux par jeu de textures")) ImGui::OpenPopup("##setchannels");
    if (ImGui::BeginPopup("##setchannels")) {
        std::map<std::string,std::vector<Rect>> scopes;
        for (const auto& [suffix, rect] : ExportParts(*v)) {
            if (suffix.empty()) continue;
            scopes[suffix].push_back(rect);
            const auto [material, udim] = texexport::SetSuffixes(suffix);
            if (!material.empty() && !udim.empty()) scopes[material].push_back(rect);
        }
        if (scopes.empty()) ImGui::TextDisabled("Ce projet utilise les canaux de l’atlas.");
        for (const auto& [key, rects] : scopes) {
            const std::string label = key.front() == '.' ? "UDIM " + key.substr(1) : key.substr(1);
            if (!ImGui::BeginMenu(label.c_str())) continue;
            if (ImGui::MenuItem("Ouvrir sa pile de peinture")) {
                Id folder = 0;
                for (const auto& l : d.S().layers) if (l.textureSet == key) { folder = l.material; break; }
                if (folder) SelectMaterial(d,folder);
                else CreateFolder3D(*v,0,label,rects,false,key);
            }
            if (ImGui::MenuItem("Hériter des canaux", nullptr, !d.S().textureSetChannels.contains(key)))
                SetTextureSetChannels(*v,key,0,true);
            for (int c = 0; c < (int)PbrChannel::Count; ++c) {
                const u32 channels = d.TextureSetChannels(key), bit = 1u << c;
                bool enabled = (channels & bit) != 0;
                ImGui::BeginDisabled(c == (int)PbrChannel::BaseColor);
                if (ImGui::Checkbox(PbrChannelName((PbrChannel)c), &enabled))
                    SetTextureSetChannels(*v,key,enabled ? channels|bit : channels&~bit);
                ImGui::EndDisabled();
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
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
    if (ImGui::Button("+ Maillage \xE2\x96\xBE")) ImGui::OpenPopup("##addmesh3d");
    ui::HelpTooltip("Calques calculés à partir de la forme du modèle : normale, position, occlusion ambiante, courbure,\n"
                    "détection des bords, creux, épaisseur, dégradé, bruit ; ou le matériau du pinceau masqué par l'un d'eux.");
    if (ImGui::BeginPopup("##addmesh3d")) {
        if (DrawMeshOperatorMenu(*v)) ImGui::CloseCurrentPopup();
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
    ImGui::SameLine();
    if (ImGui::Button("+ Pile masquée")) ImGui::OpenPopup("##setstack");
    if (ImGui::BeginPopup("##setstack")) {
        if (ImGui::MenuItem("Depuis la sélection", nullptr, false, d.Selection() != nullptr))
            CreateFolder3D(*v, 0, "Pile de la sélection", {}, true);
        const auto parts = ExportParts(*v);
        std::map<std::string, std::vector<Rect>> materials;
        for (const auto& [suffix, rect] : parts) {
            const auto [material, udim] = texexport::SetSuffixes(suffix);
            if (!material.empty() && !udim.empty()) materials[material.substr(1)].push_back(rect);
        }
        for (const auto& [material, regions] : materials) {
            if (regions.size() > 1 && ImGui::MenuItem((material + " (toutes les tuiles)").c_str()))
                CreateFolder3D(*v, 0, material, regions, false, "_" + material);
        }
        for (const auto& [suffix, rect] : parts) {
            if (rect.Empty()) continue;
            const std::string label = suffix.starts_with(".") ? "UDIM " + suffix.substr(1) : suffix.substr(1);
            ImGui::PushID(rect.x0); ImGui::PushID(rect.y0);
            if (ImGui::MenuItem(label.c_str())) CreateFolder3D(*v, 0, label, {rect}, false, suffix);
            ImGui::PopID(); ImGui::PopID();
        }
        ImGui::EndPopup();
    }
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
                if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(sp, ImVec2(sp.x + hh, sp.y + hh)))
                    BigPreviewTooltip(v->renderer.Thumbnail(*ctx, (6ull << 58) | m, s->tex.get(), s->version, (u32)(300 * dpi), (u32)(300 * dpi)),
                                      bc->name);
            }
        } else {
            float4 col = bc && !bc->effect.params.empty() ? bc->effect.params[0] : float4{0.4f, 0.4f, 0.4f, 1};
            dl->AddRectFilled(sp, ImVec2(sp.x + hh, sp.y + hh), ImGui::ColorConvertFloat4ToU32(ImVec4(LinearToSrgb(col.x), LinearToSrgb(col.y), LinearToSrgb(col.z), 1)), 3);
        }
        dl->AddRect(sp, ImVec2(sp.x + hh, sp.y + hh), cyui::palette().line, 3);
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
                static const char* abbr[] = {"C", "R", "M", "N", "E", "O", "H", "Op", "U1", "U2", "U3", "U4"};
                chans += std::string(abbr[(int)c]) + " ";
            }
        // (assembled: each piece translated)
        const std::string mask = any->mask ? " " + Tr("+ masque") : std::string();
        if (folder) ImGui::TextDisabled("%s%s", TrC("Dossier"), mask.c_str());
        else ImGui::TextDisabled("%s  %s%s", TrC(paint ? "Peinture" : "Remplissage"), chans.c_str(), mask.c_str());
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
                                                                    {PbrChannel::Metallic, "Métal"}, {PbrChannel::Height, "Hauteur"}, {PbrChannel::Emissive, "Émissif"},
        {PbrChannel::Opacity, "Opacité"}, {PbrChannel::User0, "U1"}, {PbrChannel::User1, "U2"},
        {PbrChannel::User2, "U3"}, {PbrChannel::User3, "U4"}};
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
    auto& B = brush3D;
    // Paint mode: what a click on the model does. One tile per mode, the current one highlighted, and the tab with
    // its settings opened when it changes.
    struct Mode { Tool t; int icon; const char* name; const char* help; int tab; };
    static const Mode modes[] = {
        {Tool::Brush, ui::IconBrush, "Pinceau", "Clic-glisser sur le modèle : peint le matériau de l'onglet Peinture. (B)", 0},
        {Tool::Eraser, ui::IconEraser, "Gomme", "Clic-glisser sur le modèle : efface la peinture du calque. (E)", 0},
        {Tool::PolyFill, ui::IconPolyFill, "Remplir", "Clic : remplit le triangle, la face, l'île UV ou la pièce sous le curseur.", 1},
        {Tool::Clone, ui::IconStamp, "Tampon", "Ctrl+clic choisit la source. Glissez pour dupliquer la matière sur les canaux cochés.", 0},
        {Tool::Smudge, ui::IconFinger, "Doigt", "Glissez pour étaler les canaux cochés du calque sur le modèle.", 0},
        {Tool::SurfaceMask, ui::IconMask, "Masque rapide", "Peignez pour protéger la surface. Ctrl+peindre libère la zone.", 0},
        {Tool::SurfacePath, ui::IconPen, "Tracé", "Cliquez pour poser les points du tracé. Glissez un point pour le déplacer.", 0},
        {Tool::Projection, ui::IconStamp, "Décalque", "Clic : pose une image sur le modèle (projection). (P)", 2},
        {Tool::Particles, ui::IconParticles, "Particules", "Maintenir le clic : des gouttes partent du pinceau et peignent là où elles passent.", 3},
        {Tool::Fluid, ui::IconFluid, "Liquide", "Peinture liquide qui coule avec la gravité (tous les canaux du matériau).", 3},
    };
    const int nModes = (int)std::size(modes);
    int cur = -1;
    for (int i = 0; i < nModes; ++i) if (modes[i].t == tool) cur = i;
    {
        ImGui::SeparatorText("Mode de peinture");
        const auto& pal = cyui::palette();
        const float gap = 4 * dpi;
        const float avail = ImGui::GetContentRegionAvail().x;
        const float tw = std::max(48 * dpi, (avail - gap * (nModes - 1)) / nModes);
        const float th = std::max(44 * dpi, ImGui::GetFontSize() * 3.1f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int i = 0; i < nModes; ++i) {
            if (i > 0) {
                ImGui::SameLine(0, gap);
                if (ImGui::GetContentRegionAvail().x < tw - 1) ImGui::NewLine();
            }
            const Mode& m = modes[i];
            ImGui::PushID(i);
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##mode", ImVec2(tw, th))) {
                tool = m.t;
                viewTool[1] = m.t;
            }
            const bool hov = ImGui::IsItemHovered(), sel = i == cur;
            const ImVec2 p1(p0.x + tw, p0.y + th);
            dl->AddRectFilled(p0, p1, sel ? pal.accent : ImGui::GetColorU32(hov ? ImGuiCol_ButtonHovered : ImGuiCol_FrameBg), 6 * dpi);
            const ImU32 ink = sel ? pal.accentText : hov ? pal.text : pal.textDim;
            ui::DrawIcon(dl, m.icon, ImVec2((p0.x + p1.x) * 0.5f, p0.y + th * 0.36f), th * 0.42f, ink);
            const char* label = TrC(m.name);
            const ImVec2 ls = ImGui::CalcTextSize(label);
            ImGui::PushClipRect(p0, p1, true);
            dl->AddText(ImVec2(std::max(p0.x + 2, (p0.x + p1.x - ls.x) * 0.5f), p1.y - ls.y - 3 * dpi), ink, label);
            ImGui::PopClipRect();
            if (hov) ImGui::SetTooltip("%s\n%s", TrC(m.name), TrC(m.help));
            ImGui::PopID();
        }
        if (cur >= 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, pal.accent);
            ImGui::TextWrapped("%s : %s", TrC(modes[cur].name), TrC(modes[cur].help));
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Choisissez un mode : l'outil actif ne peint pas sur le modèle.");
        }
    }
    ui::FilterBox();
    // The settings tab of the mode just chosen comes forward.
    static Tool seenTool = Tool::Count;
    const bool toolJump = tool != seenTool;
    seenTool = tool;
    const int wantTab = toolJump && cur >= 0 ? modes[cur].tab : -1;
    auto tab = [&](const char* label, int index) {
        return ImGui::BeginTabItem(label, nullptr, wantTab == index ? ImGuiTabItemFlags_SetSelected : 0);
    };
    if (ImGui::BeginTabBar("##paint3dtabs", ImGuiTabBarFlags_FittingPolicyShrink)) {
        if (tab("Peinture", 0)) {
            DrawBrushPbrPreview(84 * dpi);
            ImGui::SeparatorText("Matériau peint");
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
        auto* view = Active();
        const auto* selected = view ? view->doc->ActiveLayer() : nullptr;
        const std::string scope = selected && view->doc->kind == 1 ? view->doc->LayerTextureSet(*selected) : std::string{};
        ImGui::BeginDisabled(!scope.empty() && !(view->doc->TextureSetChannels(scope) & (1u << (int)c)));
        if (ImGui::Checkbox(label, &B.use[(int)c]) && B.use[(int)c]) {
            if (view && view->doc->kind == 1) {
                auto& doc = *view->doc;
                const auto* active = doc.ActiveLayer();
                if (active && !doc.S().editMask && IsPaintLayer3D(doc, active->material) && active->surfacePath.empty() &&
                    !MaterialChannelLayer(doc, active->material, c)) {
                    const Id mat = active->material;
                    Layer extra = *active;
                    view->history.BeginGroup();
                    const u32 previous = doc.pbrChannels;
                    if (scope.empty()) doc.pbrChannels |= 1u << (int)c;
                    if (!FindChannelGroup(doc, c)) {
                        const u32 next = doc.pbrChannels;
                        view->history.PushCustom("Canal PBR", [&doc,previous,next](bool undo){ doc.pbrChannels = undo ? previous : next; });
                        CreatePbrChannels(*view, true, 1u << (int)c);
                    }
                    Edit edit(doc, view->history, "Ajouter un canal au calque de peinture");
                    extra.id = doc.NewId();
                    extra.filters.clear();
                    extra.parent = NewLayerParent3D(doc, c, mat);
                    extra.surface = doc.CreateSurface({0,0,0,0});
                    doc.S().layers.insert(doc.S().layers.begin()+doc.LayerIndex(extra.parent), extra);
                    doc.SyncGraph();
                    edit.Commit();
                    view->history.EndGroup();
                }
            }
        }
        ImGui::SameLine(150 * dpi);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::BeginDisabled(!B.use[(int)c]);
        switch (c) {
            case PbrChannel::BaseColor: ImGui::ColorEdit3("##v", &fg.x); break;
            case PbrChannel::Roughness: ImGui::SliderFloat("##v", &B.rough, 0, 1, "%.2f"); break;
            case PbrChannel::Metallic: ImGui::SliderFloat("##v", &B.metal, 0, 1, "%.2f"); break;
            case PbrChannel::Height: ImGui::SliderFloat("##v", &B.height, 0, 1, B.height > 0.5f ? "%.2f (relief)" : B.height < 0.5f ? "%.2f (creux)" : "%.2f"); break;
            case PbrChannel::Normal: ImGui::ColorEdit3("##v", &B.normal.x); break;
            case PbrChannel::Occlusion: ImGui::SliderFloat("##v", &B.occlusion, 0, 1); break;
            case PbrChannel::Opacity: ImGui::SliderFloat("##v", &B.opacity, 0, 1); break;
            case PbrChannel::User0: case PbrChannel::User1: case PbrChannel::User2: case PbrChannel::User3:
                ImGui::SliderFloat("##v", &B.user[(int)c - (int)PbrChannel::User0], 0, 1); break;
            case PbrChannel::Emissive: ImGui::ColorEdit3("##v", &B.emissive.x); break;
            default: break;
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::PopID();
    };
    row(PbrChannel::BaseColor, "Couleur");
    row(PbrChannel::Roughness, "Rugosité");
    row(PbrChannel::Metallic, "Métal");
    row(PbrChannel::Height, "Hauteur");
    row(PbrChannel::Emissive, "Émissif");
    row(PbrChannel::Normal, "Normale");
    row(PbrChannel::Occlusion, "Occlusion");
    for (int c = (int)PbrChannel::Opacity; c < (int)PbrChannel::Count; ++c) row((PbrChannel)c, PbrChannelName((PbrChannel)c));
    if (DocView* av = Active(); av && av->mesh && !av->mesh->data.udims.empty()) {
        ImGui::Checkbox("Limiter à la tuile UDIM sous le pinceau", &B.limitMaterial);
        ui::HelpTooltip("Modèle UDIM : un trait ne peint que la tuile (1001, 1002...) sur laquelle il commence.");
    } else if (DocView* av = Active(); av && av->mesh && !av->mesh->data.uvTiles.empty()) {
        ImGui::Checkbox("Limiter au matériau sous le pinceau", &B.limitMaterial);
        ui::HelpTooltip("Modèle à plusieurs matériaux (une tuile de texture chacun) : un trait ne peint que le matériau\n"
                        "sur lequel il commence, même s'il déborde sur une pièce voisine.");
    }


            // The brush on the model: tip, dynamics, stroke helpers.
            ImGui::SeparatorText("Pointe et trait");
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
        ImGui::Checkbox("Taille en unités du modèle", &brush3D.modelSize);
        if (brush3D.modelSize) ImGui::SliderFloat("Diamètre dans le modèle", &brush3D.modelDiameter, 0.005f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("Projection selon la normale moyenne", &brush3D.normalProject);
        ui::HelpTooltip("Chaque touche est posée à plat sur la surface sous le pinceau (la moyenne de ses normales) au lieu d'être\n"
                        "projetée depuis la caméra : les marques restent rondes sur les surfaces vues de biais.\n"
                        "Le contour du pinceau montre ce plan. Sans symétrie ni pochoir.");
        if (DocView* v = Active()) {
            ImGui::Combo("Symétrie", &v->renderer.meshSymmetry, "Aucune\0Axe X\0Axe Y\0Axe Z\0");
            ui::HelpTooltip("Chaque trait est aussi peint de l'autre côté du modèle (miroir sur l'axe choisi de l'objet).");
        }
        if (ImGui::TreeNode("Dynamique")) {
            ImGui::SliderFloat("Angle", &B.angle, -180, 180, "%.0f°");
            ImGui::Checkbox("Suivre le trait", &B.follow);
            ui::HelpTooltip("La pointe tourne dans le sens du trait (rayures, coutures, traînées).");
            ImGui::Checkbox("Orienter la pointe avec l'inclinaison du stylet", &B.followTilt);
            ImGui::Checkbox("Orienter la pointe avec la rotation du stylet", &B.followTwist);
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
            ImGui::EndTabItem();
        }
        if (tab("Remplissage", 1)) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::Combo("##polymode", &B.polyMode, "Triangle\0Face (polygone plat)\0Île UV\0Pièce du maillage\0Tout le modèle\0");
    ImGui::TextDisabled("Clic sur le modèle : remplit l'élément sous le curseur avec le matériau de l'onglet Peinture\n(ou le masque du matériau). Ctrl+clic : efface.");
            ImGui::EndTabItem();
        }
        if (tab("Décalque", 2)) {
    std::string dn = decal.path.empty() ? "(aucune image)" : Narrow(Utf8Path(decal.path).filename().wstring());
    ImGui::TextUnformatted(dn.c_str());
    if (ImGui::Button("Choisir une image...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0");
        if (!f.empty()) LoadDecal(f);
    }
    ImGui::SliderFloat("Taille##decal", &decal.size, 0.05f, 2.0f, "%.2f");  // (the brush size above is "Taille" too)
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
    ImGui::TextDisabled("Mode Décalque : clic sur le modèle pour poser l'image\n(molette + Ctrl : taille, Maj : rotation).");
            ImGui::EndTabItem();
        }
        if (tab("Simulation", 3)) {
            ImGui::SeparatorText("Particules");
            if (DocView* view = Active(); view && view->mesh) DrawParticleOptions(*view);
            ImGui::SeparatorText("Peinture liquide");
            DrawFluidOptions(true);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
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

void App::RetileMaterials(DocView& v) {
    if (!v.mesh) return;
    Document& d = *v.doc;
    const MeshData old = v.mesh->data;
    MeshData tiled = old;
    std::vector<u32> source;
    if (!TileMaterialUvs(tiled, d.Width(), 1, &source)) {
        Status("Une tuile par matériau : il faut un modèle à plusieurs matériaux");
        return;
    }
    // The paint follows: every painted surface warped from the old UVs to the new ones (same surface points).
    const float W = (float)d.Width(), H = (float)d.Height();
    std::vector<float2> dst(tiled.vertices.size()), src(tiled.vertices.size());
    for (size_t i = 0; i < tiled.vertices.size(); ++i) {
        dst[i] = {tiled.vertices[i].uv.x * W, tiled.vertices[i].uv.y * H};
        const float2 ou = source[i] < old.vertices.size() ? old.vertices[source[i]].uv : float2{0, 0};
        src[i] = {ou.x * W, ou.y * H};
    }
    const auto surfaces = d.UvPaintSurfaces();
    v.renderer.Prepare(d, *ctx);
    v.history.BeginGroup();
    for (Id id : surfaces) {
        Surface* s = d.GetSurface(id);
        if (!s || s->tex->Width() != (u32)W || s->tex->Height() != (u32)H) continue;
        auto before = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "TileBefore");
        ctx->CopyTexture(before.get(), s->tex.get());
        v.renderer.WarpTriangles(d, *ctx, id, before.get(), nullptr, Rect{0, 0, (int)W, (int)H}, dst, src, tiled.indices, false);
        auto after = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "TileAfter");
        ctx->CopyTexture(after.get(), s->tex.get());
        v.history.PushPaint("Une tuile par matériau", id, {0, 0, (int)W, (int)H}, before, after);
    }
    // The new UVs belong to the same step: undoing it puts the old model back with the old paint layout.
    auto oldMesh = std::make_shared<const MeshData>(old), newMesh = std::make_shared<const MeshData>(tiled);
    DocView* vp = &v;  // (the history belongs to the view: it never outlives it)
    v.history.PushCustom("Une tuile par matériau", [this, vp, oldMesh, newMesh](bool undo) { AttachMesh(*vp, MeshData(undo ? *oldMesh : *newMesh)); });
    v.history.EndGroup();
    uvTool.previousMesh = std::make_shared<MeshData>(old);  // UV tool: "Revenir aux UV précédentes"
    const size_t n = tiled.materials.size();
    AttachMesh(v, std::move(tiled));
    d.dirty = true;
    Status("Une tuile par matériau : " + std::to_string(n) + " matériaux, peinture reportée (Ctrl+Z annule tout ; les exports écrivent un jeu par matériau)");
}

std::vector<std::pair<std::string, Rect>> App::ExportParts(DocView& v, bool actualSets) {
    std::vector<std::pair<std::string, Rect>> parts;
    const Document& d = *v.doc;
    if (exportWholeTexture && !actualSets) {
        parts.push_back({"", Rect{}});
        return parts;
    }
    auto materialSuffix = [](const MeshData& m, size_t i) {
        std::string name = m.materials[i].name.empty() ? "Materiau" + std::to_string(i + 1) : m.materials[i].name;
        for (auto& c : name)
            if (!(isalnum((unsigned char)c) || c == '-' || c == '_' || (unsigned char)c >= 0x80)) c = '_';
        return "_" + name;
    };
    if (v.mesh && !v.mesh->data.udims.empty()) {
        const auto& m = v.mesh->data;
        for (size_t k = 0; k < m.udims.size(); ++k) {
            const float4 t = UdimRect(m, (int)k);
            const std::string material = m.udimMaterials.empty() ? "" : materialSuffix(m, m.udimMaterials[k]);
            parts.push_back({material + "." + std::to_string(m.udims[k]), Rect{(int)std::lround(t.x * d.Width()), (int)std::lround(t.y * d.Height()),
                                                                   (int)std::lround(t.z * d.Width()), (int)std::lround(t.w * d.Height())}});
        }
        return parts;
    }
    if (v.mesh && !v.mesh->data.uvTiles.empty()) {
        const auto& m = v.mesh->data;
        for (size_t i = 0; i < m.uvTiles.size() && i < m.materials.size(); ++i) {
            const float4 t = m.uvTiles[i];
            if (t.z <= t.x || t.w <= t.y) continue;  // material without triangles
            Rect r = {(int)std::lround(t.x * d.Width()), (int)std::lround(t.y * d.Height()), (int)std::lround(t.z * d.Width()), (int)std::lround(t.w * d.Height())};
            parts.push_back({materialSuffix(m, i), r});
        }
    }
    if (parts.empty()) parts.push_back({"", Rect{}});
    return parts;
}

void App::ExportProjectTextures(DocView& v, const std::string& basePath) {
    Document& d = *v.doc;
    if (basePath.empty()) return;
    if (d.exportTemplate == 0) { ExportPbrTextures(v, basePath); return; }
    const auto parts = ExportParts(v);
    const std::string error = texexport::ValidateSetNames(parts, Narrow(Utf8Path(basePath).stem().wstring()));
    if (!error.empty()) { Status(error); return; }
    // Packed maps: ORM (Unreal: R occlusion, G roughness, B metal) or Unity mask map (R metal, G occlusion, A smoothness).
    auto p = std::filesystem::absolute(Utf8Path(basePath));
    std::string stem = Narrow((p.parent_path() / p.stem()).wstring());
    v.renderer.EvaluateComplete(d, *ctx);
    auto rb = [&](PbrChannel c) -> gpu::ReadbackPtr {
        gpu::Texture* t = ChannelTexture(v, c);
        return t ? v.renderer.ExportRGBA8(*ctx, t) : nullptr;
    };
    const bool unity = d.exportTemplate == 2;
    const gpu::ReadbackPtr ao = rb(PbrChannel::Occlusion), rough = rb(PbrChannel::Roughness), metal = rb(PbrChannel::Metallic);
    // Base color, normal (height baked in) and emissive as separate maps.
    const auto normals = ReadSetNormals8(v,parts);
    auto grab = [&](gpu::Texture* t) { return t ? v.renderer.ExportRGBA8(*ctx, t) : nullptr; };
    const gpu::ReadbackPtr base = grab(ChannelTexture(v, PbrChannel::BaseColor));
    const gpu::ReadbackPtr emissive = grab(ChannelTexture(v, PbrChannel::Emissive));
    v.renderer.DropNodeCaches(d);
    const std::string stemAll = stem;
    const std::string nameAll = Narrow(p.stem().wstring());
    // One texture set per part: the whole texture, or each material's tile (the readbacks are shared).
    for (const auto& [suffix, crop] : parts) {
    const u32 enabled = d.TextureSetChannels(suffix);
    const auto normal = normals[PbrNormalVariant(enabled)];
    const auto emis = (enabled & (1u << (int)PbrChannel::Emissive)) ? emissive : nullptr;
    const auto [materialSuffix, udimSuffix] = texexport::SetSuffixes(suffix);
    const std::string setStem = stemAll + materialSuffix;
    // UDIM tiles: name_Channel.1001.png (the tile number last, as the 3D tools expect).
    const bool udimPart = !udimSuffix.empty();
    auto named = [&](const char* channel) { return setStem + channel + udimSuffix + ".png"; };
    PackJob job;
    job.unity = unity;
    job.path = named(job.unity ? "_MaskMap" : "_ORM");
    job.ao = (enabled & (1u << (int)PbrChannel::Occlusion)) ? ao : nullptr;
    job.rough = (enabled & (1u << (int)PbrChannel::Roughness)) ? rough : nullptr;
    job.metal = (enabled & (1u << (int)PbrChannel::Metallic)) ? metal : nullptr;
    job.crop = crop.Empty() ? Rect{0, 0, (int)d.Width(), (int)d.Height()} : crop;
    packJobs.push_back(job);
    if (base) exports.push_back({named(unity ? "_BaseMap" : "_BaseColor"), base, crop});
    if (normal) exports.push_back({named("_Normal"), normal, crop});
    if (emis) exports.push_back({named("_Emissive"), emis, crop});
    if (!job.unity && !udimPart) {
        // Manifest read by the Unreal plugin (Tools > Shaderixel > Importer un export Shaderixel...).
        Json m = Json::Object();
        m["format"] = "shaderixel-textures";
        m["version"] = 1;
        m["name"] = nameAll + suffix;
        Json texs = Json::Array();
        auto add = [&](const char* role, const char* fileSuffix) {
            Json t = Json::Object();
            t["role"] = role;
            t["file"] = nameAll + suffix + fileSuffix;
            texs.Push(t);
        };
        if (base) add("BaseColor", "_BaseColor.png");
        if (normal) add("Normal", "_Normal.png");
        add("ORM", "_ORM.png");
        if (emis) add("Emissive", "_Emissive.png");
        m["textures"] = texs;
        std::ofstream(Utf8Path(setStem + ".sxue.json"), std::ios::binary) << m.Dump(1);
    }
    }
    if (parts.size() > 1 && !texexport::SetSuffixes(parts[0].first).second.empty())
        Status("Export des textures : " + std::to_string(parts.size()) + " tuiles UDIM, " + stemAll + "_*.<tuile>.png");
    else if (parts.size() > 1)
        Status("Export des textures : " + std::to_string(parts.size()) + " matériaux, " + stemAll + "_<matériau>_*.png" + (unity ? "" : " (+ .sxue.json)"));
    else
        Status("Export des textures : " + stemAll + "_*.png" + (unity ? "" : " (+ .sxue.json pour le plugin Unreal)"));
}

void App::UpdatePaint3D() {
    if (demoMode != 114) CheckParticleSession();
    for (auto& v : docs) {
        if (v->surfaceTools.stroke != Tool::Count &&
            (v.get() != Active() || v->surfaceTools.lastFrame + 1 < ImGui::GetFrameCount()))
            EndSurfaceTool(*v);
    }
    PollMeshMapsBake();
    PollBake();
    PollTextureExports();
    PollTurntable();
    PollSmartMaterialSave();
    PollMaterialPick();
    if (packJobs.empty()) return;
    const auto& job = packJobs.front();
    if (packedMapWrite.valid()) {
        if (packedMapWrite.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            Status((packedMapWrite.get() ? "Exporté : " : "Échec de l'export : ") + job.path);
        } catch (const std::exception& e) {
            Status("Échec de l'export : " + job.path + " (" + e.what() + ")");
        }
        packJobs.erase(packJobs.begin());
        return;
    }
    auto ready = [](const gpu::ReadbackPtr& r) { return !r || r->ready; };
    if (!ready(job.ao) || !ready(job.rough) || !ready(job.metal)) return;
    packedMapWrite = std::async(std::launch::async, [job] {
        const Rect c = job.crop;
        if (c.Empty()) return false;
        for (const auto& r : {job.ao, job.rough, job.metal}) {
            if (r && (c.x0 < 0 || c.y0 < 0 || c.x1 > (int)r->width || c.y1 > (int)r->height ||
                      r->data.size() != (size_t)r->width * r->height * 4)) return false;
        }
        std::vector<u8> px((size_t)c.Width() * c.Height() * 4);
        auto sample = [](const gpu::ReadbackPtr& r, int x, int y, u8 fallback) {
            return r ? r->data[((size_t)y * r->width + x) * 4] : fallback;
        };
        for (int y = c.y0; y < c.y1; ++y) for (int x = c.x0; x < c.x1; ++x) {
            const u8 ao = sample(job.ao, x, y, 255), rough = sample(job.rough, x, y, 128), metal = sample(job.metal, x, y, 0);
            u8* out = &px[((size_t)(y-c.y0) * c.Width() + x-c.x0) * 4];
            if (job.unity) { out[0] = metal; out[1] = ao; out[2] = 0; out[3] = 255 - rough; }
            else { out[0] = ao; out[1] = rough; out[2] = metal; out[3] = 255; }
        }
        return stbi_write_png(job.path.c_str(), c.Width(), c.Height(), 4, px.data(), c.Width() * 4) != 0;
    });
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
        ImGui::BeginDisabled(P.autoUv);
        ImGui::SetNextItemWidth(300 * dpi);
        ImGui::Combo("Matériaux multiples", &P.tileMode, "Auto : une tuile par matériau si leurs UV se chevauchent\0Une tuile par matériau\0Garder les UV du modèle\0");
        ui::HelpTooltip("Un modèle à plusieurs matériaux (chacun avec ses UV de 0 à 1) : chaque matériau reçoit sa propre\n"
                        "zone de la texture, et l'export écrit un jeu de textures par matériau (Modèle_Matériau_BaseColor.png...).");
        ImGui::EndDisabled();
    }
    ImGui::SeparatorText("Texture");
    ImGui::Combo("Résolution", &P.resolution, "1024 x 1024\0" "2048 x 2048\0" "4096 x 4096\0");
    if (P.mesh == 3 && !P.autoUv) {
        ImGui::Checkbox("Résolution native par tuile ou matériau", &P.resolutionPerTile);
        if (P.resolutionPerTile) {
            ImGui::InputTextMultiline("Résolutions individuelles", &P.tileResolutions, ImVec2(250*dpi, 60*dpi));
            ui::HelpTooltip("Une ligne par exception : 1001 = 2048, Carrosserie = 1024 ou Carrosserie.1001 = 512. Le couple matériau–UDIM est prioritaire.");
        }
    }
    ImGui::SeparatorText("Canaux");
    static const char* names[] = {"Couleur de base", "Rugosité", "Métal", "Normale", "Émissif", "Occlusion (AO)", "Hauteur", "Opacité", "Utilisateur 1", "Utilisateur 2", "Utilisateur 3", "Utilisateur 4"};
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
