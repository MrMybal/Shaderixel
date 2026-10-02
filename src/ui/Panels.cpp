// Layers, properties, color and brush panels.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sx {

void App::TrackUndo(const char* label) {
    DocView* v = Active();
    if (!v || !frameBeforeValid) return;
    if (ImGui::IsItemActivated()) editBefore = frameBefore;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        v->history.PushState(label, editBefore ? *editBefore : frameBefore, v->doc->S());
        editBefore.reset();
    } else if (ImGui::IsItemDeactivated()) {
        editBefore.reset();
    }
}

bool App::EffectParamsUI(EffectInstance& fx, const char* label) {
    EffectDefPtr def = fx.Def();
    if (!def) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Effet introuvable : %s", fx.effectId.c_str());
        return false;
    }
    if (fx.params.size() < def->params.size())
        for (size_t i = fx.params.size(); i < def->params.size(); ++i) fx.params.push_back(def->params[i].def);
    bool changed = false;
    ImGui::PushID(label);
    for (size_t i = 0; i < def->params.size(); ++i) {
        const ParamDef& p = def->params[i];
        float4& val = fx.params[i];
        ImGui::PushID((int)i);
        bool c = false;
        switch (p.type) {
            case ParamType::Float: c = ImGui::SliderFloat(p.label.c_str(), &val.x, p.min, p.max, "%.3f"); break;
            case ParamType::Int: {
                int iv = (int)std::lround(val.x);
                c = ImGui::SliderInt(p.label.c_str(), &iv, (int)p.min, (int)p.max);
                val.x = (float)iv;
                break;
            }
            case ParamType::Bool: {
                bool b = val.x > 0.5f;
                c = ImGui::Checkbox(p.label.c_str(), &b);
                val.x = b ? 1.0f : 0.0f;
                break;
            }
            case ParamType::Color: {
                // Stored linear; edited in sRGB like every other color in the UI.
                float s[4] = {LinearToSrgb(val.x), LinearToSrgb(val.y), LinearToSrgb(val.z), val.w};
                c = ImGui::ColorEdit4(p.label.c_str(), s, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Float);
                if (c) val = {SrgbToLinear(s[0]), SrgbToLinear(s[1]), SrgbToLinear(s[2]), s[3]};
                break;
            }
            case ParamType::Float2: c = ImGui::SliderFloat2(p.label.c_str(), &val.x, p.min, p.max, "%.3f"); break;
            case ParamType::Enum: {
                int iv = (int)std::lround(val.x);
                const char* cur = iv >= 0 && iv < (int)p.enumItems.size() ? p.enumItems[iv].c_str() : "?";
                if (ImGui::BeginCombo(p.label.c_str(), cur)) {
                    for (int k = 0; k < (int)p.enumItems.size(); ++k)
                        if (ImGui::Selectable(p.enumItems[k].c_str(), k == iv)) { val.x = (float)k; c = true; }
                    ImGui::EndCombo();
                }
                if (c && Active()) Active()->history.PushState(p.label, frameBefore, Active()->doc->S());
                break;
            }
        }
        if (p.type != ParamType::Enum) {
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && p.type != ParamType::Bool) {
                val = p.def;  // double-click resets
                c = true;
            }
            TrackUndo(p.label.c_str());
        }
        changed |= c;
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

// ------------------------------------------------------------------ layers

void App::DrawLayers() {
    if (!ImGui::Begin("Calques")) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v) { ImGui::End(); return; }
    Document& d = *v->doc;

    if (d.Mode() == DocMode::Nodes) {
        ImGui::TextWrapped("Ce document est en mode nodal : la composition se fait dans le panneau Graphe.");
        std::string why;
        bool can = d.CanConvertToLayers(&why);
        ImGui::BeginDisabled(!can);
        if (ImGui::Button("Convertir en calques")) {
            Edit e(d, v->history, "Convertir en calques");
            d.ConvertToLayers();
            e.Commit();
        }
        ImGui::EndDisabled();
        if (!can) ImGui::TextColored(ImVec4(1, 0.7f, 0.35f, 1), "Graphe non linéaire : %s", why.c_str());
        ImGui::End();
        return;
    }

    Layer* active = d.ActiveLayer();
    // Blend + opacity of the active layer (Photoshop header).
    ImGui::BeginDisabled(!active);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    bool isGroup = active && active->kind == LayerKind::Group;
    const char* curBlend = !active ? "Normal" : (isGroup && active->passThrough) ? "Traverser" : BlendModeName(active->blend);
    if (ImGui::BeginCombo("##blend", curBlend)) {
        if (isGroup && ImGui::Selectable("Traverser", active->passThrough)) {
            active->passThrough = true;
            d.SyncGraph();
            v->history.PushState("Mode de fusion", frameBefore, d.S());
        }
        for (int i = 0; i < kBlendMenuCount; ++i) {
            int m = kBlendMenuOrder[i];
            if (m < 0) { ImGui::Separator(); continue; }
            if (ImGui::Selectable(BlendModeName((BlendMode)m), active && (int)active->blend == m && !(isGroup && active->passThrough))) {
                active->blend = (BlendMode)m;
                active->passThrough = false;
                d.SyncGraph();
                v->history.PushState("Mode de fusion", frameBefore, d.S());
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    float opacity = active ? active->opacity * 100.0f : 100.0f;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##opacity", &opacity, 0, 100, "Opacité %.0f %%") && active) {
        active->opacity = opacity / 100.0f;
        d.SyncGraph();
    }
    TrackUndo("Opacité");
    ImGui::EndDisabled();

    // Layer list, top layer first; children of collapsed groups are hidden.
    float thumb = 38.0f * dpi;
    ImGui::BeginChild("##layerlist", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4), ImGuiChildFlags_Borders);
    auto& layers = d.S().layers;
    Id dragId = 0, dropTarget = 0;
    bool dropInto = false;
    auto hiddenByCollapse = [&](const Layer& l) {
        Id p = l.parent;
        while (p) {
            Layer* g = d.FindLayer(p);
            if (!g) break;
            if (g->collapsed) return true;
            p = g->parent;
        }
        return false;
    };
    for (int i = (int)layers.size() - 1; i >= 0; --i) {
        Layer& l = layers[i];
        if (hiddenByCollapse(l)) continue;
        ImGui::PushID((int)l.id);
        bool selected = l.id == d.S().activeLayer;
        bool group = l.kind == LayerKind::Group;
        int depth = d.LayerDepth(l.id);
        float indent = depth * 16.0f * dpi + (l.clipped ? 14.0f * dpi : 0.0f);
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        float rowH = group ? ImGui::GetFrameHeight() + 8 : thumb + 8;
        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(w, rowH))) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && !group) {  // Ctrl+click: load layer pixels as selection
                SelMode m = io.KeyShift && io.KeyAlt ? SelMode::Intersect : io.KeyShift ? SelMode::Add : io.KeyAlt ? SelMode::Subtract : SelMode::Replace;
                SelectFromLayer(*v, l, m);
            } else {
                d.S().activeLayer = l.id;
                d.S().editMask = false;
                if (group && ImGui::IsMouseDoubleClicked(0)) l.collapsed = !l.collapsed;
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !group) ImGui::SetTooltip("Ctrl+clic : sélectionner les pixels du calque");
        if (ImGui::BeginDragDropSource()) {
            Id id = l.id;
            ImGui::SetDragDropPayload("SX_LAYER", &id, sizeof(Id));
            ImGui::Text("%s", l.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("SX_LAYER")) {
                dragId = *(const Id*)p->Data;
                dropTarget = l.id;
                // Dropping on the lower half of a group row puts the layer inside it.
                dropInto = group && ImGui::GetMousePos().y > rowMin.y + rowH * 0.5f;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##ctx")) {
            d.S().activeLayer = l.id;
            LayerMenuItems();
            ImGui::EndPopup();
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Visibility toggle
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 2, rowMin.y + (rowH - 22 * dpi) * 0.5f));
        if (ui::IconButton("vis", l.visible ? ui::IconEye : ui::IconEyeOff, false, 22 * dpi, "Afficher / masquer")) {
            DocState before = d.S();
            l.visible = !l.visible;
            d.SyncGraph();
            v->history.PushState(l.visible ? "Afficher le calque" : "Masquer le calque", before, d.S());
        }
        float x0 = rowMin.x + 30 * dpi + indent;
        if (l.clipped) {  // clipping arrow
            float cx = x0 - 9 * dpi, cy = rowMin.y + rowH * 0.5f;
            dl->AddLine(ImVec2(cx, cy - 8 * dpi), ImVec2(cx, cy + 4 * dpi), ui::col::TextDim, 1.5f);
            dl->AddTriangleFilled(ImVec2(cx - 4 * dpi, cy + 3 * dpi), ImVec2(cx + 4 * dpi, cy + 3 * dpi), ImVec2(cx, cy + 8 * dpi), ui::col::TextDim);
        }
        float textX;
        if (group) {
            // Collapse arrow + folder icon
            float h = rowH;
            ImGui::SetCursorScreenPos(ImVec2(x0, rowMin.y + (h - 18 * dpi) * 0.5f));
            if (ImGui::InvisibleButton("##fold", ImVec2(16 * dpi, 18 * dpi))) l.collapsed = !l.collapsed;
            ImVec2 c(x0 + 8 * dpi, rowMin.y + h * 0.5f);
            float a = 4.5f * dpi;
            if (l.collapsed) dl->AddTriangleFilled(ImVec2(c.x - a * 0.6f, c.y - a), ImVec2(c.x + a, c.y), ImVec2(c.x - a * 0.6f, c.y + a), ui::col::Text);
            else dl->AddTriangleFilled(ImVec2(c.x - a, c.y - a * 0.6f), ImVec2(c.x + a, c.y - a * 0.6f), ImVec2(c.x, c.y + a), ui::col::Text);
            ui::DrawIcon(dl, ui::IconFolder, ImVec2(x0 + 30 * dpi, c.y), 18 * dpi, l.passThrough ? ui::col::TextDim : ui::col::Accent2);
            textX = x0 + 46 * dpi;
        } else {
            // Thumbnail(s)
            ImVec2 tp(x0, rowMin.y + 4);
            gpu::Texture* src = nullptr;
            u64 hash = 0;
            if (l.kind == LayerKind::Pixel) {
                if (Surface* s = d.GetSurface(l.surface)) { src = s->tex.get(); hash = s->version; }
            } else {
                for (auto& n : d.S().graph.nodes)
                    if (n.layer == l.id && n.kind == NodeKind::Effect) {
                        src = v->renderer.NodeOutput(n.id);
                        hash = v->renderer.NodeHash(n.id);
                        break;
                    }
            }
            if (src) {
                gpu::Texture* t = v->renderer.Thumbnail(*ctx, ((u64)l.id << 1), src, hash, (u32)thumb, (u32)thumb);
                dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), tp, ImVec2(tp.x + thumb, tp.y + thumb));
            }
            if (l.kind != LayerKind::Pixel) ui::DrawIcon(dl, ui::IconFx, ImVec2(tp.x + thumb * 0.5f, tp.y + thumb * 0.5f), thumb * 0.5f, IM_COL32(255, 255, 255, 200));
            bool contentSel = selected && !d.S().editMask;
            dl->AddRect(ImVec2(tp.x - 1, tp.y - 1), ImVec2(tp.x + thumb + 1, tp.y + thumb + 1), contentSel ? ui::col::Accent2 : ui::col::Border, 0, 0, contentSel ? 2.0f : 1.0f);
            textX = tp.x + thumb + 8;
            if (l.mask) {
                ImVec2 mp(textX, tp.y);
                if (Surface* s = d.GetSurface(l.mask)) {
                    gpu::Texture* t = v->renderer.Thumbnail(*ctx, ((u64)l.id << 1) | 1, s->tex.get(), s->version, (u32)thumb, (u32)thumb);
                    dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), mp, ImVec2(mp.x + thumb, mp.y + thumb));
                }
                bool maskSel = selected && d.S().editMask;
                dl->AddRect(ImVec2(mp.x - 1, mp.y - 1), ImVec2(mp.x + thumb + 1, mp.y + thumb + 1), maskSel ? ui::col::Accent2 : ui::col::Border, 0, 0, maskSel ? 2.0f : 1.0f);
                if (!l.maskEnabled) dl->AddLine(mp, ImVec2(mp.x + thumb, mp.y + thumb), ui::col::Error, 2);
                ImGui::SetCursorScreenPos(mp);
                if (ImGui::InvisibleButton("##mask", ImVec2(thumb, thumb))) {
                    d.S().activeLayer = l.id;
                    d.S().editMask = true;
                }
                ui::HelpTooltip("Clic : éditer le masque");
                ImGui::SetCursorScreenPos(tp);
                if (ImGui::InvisibleButton("##content", ImVec2(thumb, thumb))) {
                    d.S().activeLayer = l.id;
                    d.S().editMask = false;
                }
                textX += thumb + 8;
            }
        }
        ImVec2 tpos(textX, rowMin.y + (rowH - ImGui::GetFontSize()) * 0.5f);
        dl->AddText(tpos, l.visible ? ui::col::Text : ui::col::TextDim, l.name.c_str());
        if (l.clipped) dl->AddLine(ImVec2(tpos.x, tpos.y + ImGui::GetFontSize() + 1), ImVec2(tpos.x + ImGui::CalcTextSize(l.name.c_str()).x, tpos.y + ImGui::GetFontSize() + 1), ui::col::TextDim);
        if (l.locked) ui::DrawIcon(dl, ui::IconLock, ImVec2(rowMin.x + w - 14 * dpi, rowMin.y + rowH * 0.5f), 12 * dpi, ui::col::TextDim);
        if (l.filters.size()) {
            char fx[16];
            snprintf(fx, sizeof(fx), "fx %zu", l.filters.size());
            dl->AddText(ImVec2(rowMin.x + w - 48 * dpi, tpos.y), ui::col::Accent2, fx);
        }
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + rowH + ImGui::GetStyle().ItemSpacing.y));
        ImGui::PopID();
    }
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
    if (dragId && dropTarget && dragId != dropTarget) {
        Edit e(d, v->history, "Déplacer le calque");
        d.MoveLayerTo(dragId, dropTarget, dropInto);
        e.Commit();
    }

    // Bottom toolbar
    float bs = ImGui::GetFrameHeight();
    if (ui::IconButton("new", ui::IconPlus, false, bs, "Nouveau calque (Ctrl+Maj+N)")) {
        Edit e(d, v->history, "Nouveau calque");
        d.AddPixelLayer("Calque " + std::to_string(layers.size() + 1));
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("grp", ui::IconFolder, false, bs, "Grouper le calque (Ctrl+G)") && active) {
        Edit e(d, v->history, "Grouper");
        d.GroupLayer(active->id);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("fx", ui::IconFx, false, bs, "Nouveau calque de remplissage ou de réglage")) ImGui::OpenPopup("##newfx");
    if (ImGui::BeginPopup("##newfx")) {
        if (ImGui::BeginMenu("Remplissage")) {
            EffectMenu([&](const std::string& id) { Edit e(d, v->history, "Calque de remplissage"); MaskFromSelection(*v, d.AddEffectLayer(LayerKind::Fill, id).id, false); e.Commit(); }, true, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Réglage")) {
            EffectMenu([&](const std::string& id) { Edit e(d, v->history, "Calque de réglage"); MaskFromSelection(*v, d.AddEffectLayer(LayerKind::Adjustment, id).id, false); e.Commit(); }, false, true);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!active);
    if (ui::IconButton("mask", ui::IconMask, false, bs, "Ajouter un masque de fusion") && active && !active->mask) {
        Edit e(d, v->history, "Ajouter un masque");
        MaskFromSelection(*v, active->id, true);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("dup", ui::IconCopy, false, bs, "Dupliquer (Ctrl+J)") && active) {
        Edit e(d, v->history, "Dupliquer le calque");
        d.DuplicateLayer(active->id);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("up", ui::IconUp, false, bs, "Monter (Ctrl+])") && active) {
        Edit e(d, v->history, "Monter le calque");
        d.MoveLayerSibling(active->id, +1);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("down", ui::IconDown, false, bs, "Descendre (Ctrl+[)") && active) {
        Edit e(d, v->history, "Descendre le calque");
        d.MoveLayerSibling(active->id, -1);
        e.Commit();
    }
    ImGui::SameLine();
    if (ui::IconButton("del", ui::IconTrash, false, bs, "Supprimer le calque") && active) {
        Edit e(d, v->history, "Supprimer le calque");
        d.RemoveLayer(active->id);
        e.Commit();
    }
    ImGui::EndDisabled();
    ImGui::End();
}

// ------------------------------------------------------------------ properties

void App::DrawProperties() {
    if (!ImGui::Begin("Propriétés")) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v) { ImGui::End(); return; }
    Document& d = *v->doc;
    ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.42f);

    if (d.Mode() == DocMode::Layers) {
        Layer* l = d.ActiveLayer();
        if (!l) {
            ImGui::TextDisabled("Aucun calque sélectionné.");
        } else {
            bool changed = false;
            ImGui::InputText("Nom", &l->name);
            TrackUndo("Renommer");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
            const char* kinds[] = {"Calque de pixels", "Calque de remplissage", "Calque de réglage", "Groupe"};
            ImGui::TextDisabled("%s", kinds[(int)l->kind]);
            if (ImGui::Checkbox("Visible", &l->visible)) changed = true;
            TrackUndo("Visibilité");
            ImGui::SameLine();
            if (ImGui::Checkbox("Verrouillé", &l->locked)) changed = true;
            TrackUndo("Verrouillage");
            if (l->kind != LayerKind::Pixel) {
                ImGui::SeparatorText(l->kind == LayerKind::Fill ? "Remplissage" : "Réglage");
                changed |= EffectParamsUI(l->effect, "main");
            }
            if (l->mask) {
                ImGui::SeparatorText("Masque de fusion");
                if (ImGui::Checkbox("Activé", &l->maskEnabled)) changed = true;
                TrackUndo("Masque activé");
                ImGui::SameLine();
                bool edit = d.S().editMask;
                if (ImGui::Checkbox("Peindre le masque", &edit)) d.S().editMask = edit;
                ImGui::SameLine();
                if (ImGui::SmallButton("Supprimer")) {
                    DocState before = d.S();
                    l->mask = 0;
                    d.S().editMask = false;
                    d.SyncGraph();
                    v->history.PushState("Supprimer le masque", before, d.S());
                }
            }
            ImGui::SeparatorText("Filtres");
            int removeAt = -1;
            for (size_t i = 0; i < l->filters.size(); ++i) {
                EffectInstance& f = l->filters[i];
                ImGui::PushID((int)i);
                auto def = f.Def();
                if (ImGui::Checkbox("##en", &f.enabled)) changed = true;
                TrackUndo("Activer le filtre");
                ImGui::SameLine();
                bool open = ImGui::TreeNodeEx("##f", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", def ? def->name.c_str() : f.effectId.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24 * dpi);
                if (ui::IconButton("rm", ui::IconTrash, false, 20 * dpi, "Supprimer le filtre")) removeAt = (int)i;
                if (open) {
                    changed |= EffectParamsUI(f, "f");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (removeAt >= 0) {
                DocState before = d.S();
                l->filters.erase(l->filters.begin() + removeAt);
                d.SyncGraph();
                v->history.PushState("Supprimer le filtre", before, d.S());
            }
            if (ImGui::Button("+ Ajouter un filtre")) ImGui::OpenPopup("##addfilter");
            if (ImGui::BeginPopup("##addfilter")) {
                EffectMenu([&](const std::string& id) {
                    DocState before = d.S();
                    EffectInstance fx;
                    fx.effectId = id;
                    if (auto def = Effects().Find(id)) fx.params = def->DefaultParams();
                    l->filters.push_back(fx);
                    d.SyncGraph();
                    v->history.PushState("Ajouter un filtre", before, d.S());
                }, false, true);
                ImGui::EndPopup();
            }
            if (changed) d.SyncGraph();
        }
    } else {
        Node* n = d.S().graph.Find(d.S().activeNode);
        if (!n) {
            ImGui::TextDisabled("Aucun nœud sélectionné.");
            ImGui::TextWrapped("Clic droit dans le graphe pour ajouter des nœuds.");
        } else {
            ImGui::InputText("Nom", &n->name);
            TrackUndo("Renommer");
            switch (n->kind) {
                case NodeKind::Output: ImGui::TextDisabled("Sortie du document"); break;
                case NodeKind::Surface: ImGui::TextDisabled("Calque de pixels - peignez dessus avec le pinceau"); break;
                case NodeKind::Blend: {
                    ImGui::Checkbox("Désactivé (bypass)", &n->bypass);
                    TrackUndo("Bypass");
                    if (ImGui::BeginCombo("Mode", BlendModeName(n->blend))) {
                        for (int i = 0; i < kBlendMenuCount; ++i) {
                            int m = kBlendMenuOrder[i];
                            if (m < 0) { ImGui::Separator(); continue; }
                            if (ImGui::Selectable(BlendModeName((BlendMode)m), (int)n->blend == m)) {
                                n->blend = (BlendMode)m;
                                v->history.PushState("Mode de fusion", frameBefore, d.S());
                            }
                        }
                        ImGui::EndCombo();
                    }
                    float op = n->opacity * 100.0f;
                    if (ImGui::SliderFloat("Opacité", &op, 0, 100, "%.0f %%")) n->opacity = op / 100.0f;
                    TrackUndo("Opacité");
                    break;
                }
                case NodeKind::Effect: {
                    ImGui::Checkbox("Désactivé (bypass)", &n->bypass);
                    TrackUndo("Bypass");
                    bool custom = !n->fx.customCode.empty();
                    auto def = n->fx.Def();
                    if (def) ImGui::TextDisabled("%s  -  %s", def->category.c_str(), custom ? "HLSL personnalisé" : n->fx.effectId.c_str());
                    ImGui::SeparatorText("Paramètres");
                    int oldInputs = NodeInputCount(*n);
                    EffectParamsUI(n->fx, "p");
                    if (custom) {
                        ImGui::SeparatorText("Code HLSL / Slang");
                        static Id editingNode = 0;
                        static std::string buffer;
                        if (editingNode != n->id) { editingNode = n->id; buffer = n->fx.customCode; }
                        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
                        ImGui::InputTextMultiline("##code", &buffer, ImVec2(-1, std::max(220.0f * dpi, ImGui::GetContentRegionAvail().y - 90 * dpi)), ImGuiInputTextFlags_AllowTabInput);
                        ImGui::PopFont();
                        bool apply = ImGui::Button("Compiler (Ctrl+Entrée)") || (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl);
                        if (apply && buffer != n->fx.customCode) {
                            DocState before = d.S();
                            n->fx.customCode = buffer;
                            auto nd = n->fx.Def();
                            std::vector<float4> params = nd->DefaultParams();
                            // Keep values of params that still exist.
                            if (def)
                                for (size_t i = 0; i < nd->params.size(); ++i)
                                    for (size_t k = 0; k < def->params.size() && k < n->fx.params.size(); ++k)
                                        if (def->params[k].id == nd->params[i].id) params[i] = n->fx.params[k];
                            n->fx.params = params;
                            n->name = nd->name;
                            n->inputs.resize(NodeInputCount(*n), 0);
                            v->history.PushState("Modifier le code", before, d.S());
                        }
                        std::string err;
                        if (v->renderer.NodeFailed(n->id, &err)) {
                            ImGui::PushFont(ui::GetFonts().mono, 0.0f);
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
                            ImGui::TextWrapped("%s", err.c_str());
                            ImGui::PopStyleColor();
                            ImGui::PopFont();
                        } else if (def && !def->error.empty()) {
                            ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", def->error.c_str());
                        }
                    } else if (ImGui::Button("Convertir en HLSL éditable") && def) {
                        DocState before = d.S();
                        std::string text;
                        ReadTextFile(gpu::Shaders().ShaderDir() + "/effects/" + n->fx.effectId + ".slang", text);
                        n->fx.customCode = text.empty() ? kCustomEffectTemplate : text;
                        n->fx.effectId.clear();
                        v->history.PushState("Convertir en HLSL", before, d.S());
                    }
                    if (NodeInputCount(*n) != oldInputs) n->inputs.resize(NodeInputCount(*n), 0);
                    break;
                }
            }
        }
    }

    ImGui::SeparatorText("Document");
    ImGui::Text("%u x %u px, %s", d.Width(), d.Height(), d.GetPrecision() == Precision::Float32 ? "32 bits flottant" : "16 bits flottant");
    if (ImGui::Checkbox("Fusion en gamma (compatible Photoshop)", &d.blendInGamma)) d.dirty = true;
    ui::HelpTooltip("Activé : les modes de fusion sont calculés en sRGB comme Photoshop.\nDésactivé : fusion physiquement linéaire (rendu, PBR).");
    ImGui::PopItemWidth();
    ImGui::End();
}

// ------------------------------------------------------------------ color & brush

void App::DrawColor() {
    if (!ImGui::Begin("Couleur")) { ImGui::End(); return; }
    ImGui::RadioButton("Premier plan", !editBg) ? editBg = false : 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Arrière-plan", editBg)) editBg = true;
    float4& c = editBg ? bg : fg;
    ImGui::SetNextItemWidth(-1);
    ImGui::ColorPicker4("##picker", &c.x, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex);
    ImGui::End();
}

void App::DrawBrush() {
    if (!ImGui::Begin("Options")) { ImGui::End(); return; }
    ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.38f);
    DocView* v = Active();

    if (xf.active) {
        ImGui::SeparatorText(xf.moveOnly ? "Déplacement" : "Transformation manuelle");
        if (xf.boundsRb) {
            ImGui::TextDisabled("Calcul des limites...");
        } else if (!xf.moveOnly) {
            bool changed = false;
            if (!xf.freeCorners) {
                float wp = xf.w / xf.src.Width() * 100.0f, hp = xf.h / xf.src.Height() * 100.0f;
                if (ImGui::DragFloat("Largeur %", &wp, 0.25f, -1000, 1000, "%.1f")) { xf.w = wp / 100.0f * xf.src.Width(); changed = true; }
                if (ImGui::DragFloat("Hauteur %", &hp, 0.25f, -1000, 1000, "%.1f")) { xf.h = hp / 100.0f * xf.src.Height(); changed = true; }
                float deg = xf.angle * 180.0f / 3.14159265f;
                if (ImGui::DragFloat("Angle", &deg, 0.25f, -360, 360, "%.1f°")) { xf.angle = deg * 3.14159265f / 180.0f; changed = true; }
                if (ImGui::DragFloat2("Centre", &xf.center.x, 1.0f, -100000, 100000, "%.1f")) changed = true;
            } else {
                ImGui::TextDisabled("Mode déformation (coins libres)");
            }
            if (ImGui::Combo("Interpolation", &xf.interp, "Au plus proche\0Bilinéaire\0Bicubique\0")) changed = true;
            if (changed) {
                TransformUpdateCorners();
                xf.dirty = true;
            }
            ImGui::TextDisabled("Coin : échelle  (Maj : proportionnel, Alt : depuis le centre)");
            ImGui::TextDisabled("Ctrl+coin : perspective / déformation");
            ImGui::TextDisabled("Hors du cadre : rotation (Maj : 15°)");
        }
        if (ImGui::Button("Valider (Entrée)") && xf.view) TransformCommit(*xf.view);
        ImGui::SameLine();
        if (ImGui::Button("Annuler (Échap)") && xf.view) TransformCancel(*xf.view);
    } else if (tool == Tool::Move) {
        ImGui::SeparatorText("Déplacement");
        ImGui::TextWrapped("Glissez pour déplacer le calque actif (ou la sélection). Flèches : 1 px, Maj+flèches : 10 px. Ctrl+T : transformation manuelle.");
    } else if (tool == Tool::RectSelect || tool == Tool::EllipseSelect || tool == Tool::Lasso || tool == Tool::PolyLasso || tool == Tool::Wand) {
        ImGui::SeparatorText("Sélection");
        const char* modes[] = {"Nouvelle", "Ajouter", "Soustraire", "Intersection"};
        for (int i = 0; i < 4; ++i) {
            if (i) ImGui::SameLine();
            if (ImGui::RadioButton(modes[i], selModeOption == i)) selModeOption = i;
        }
        ImGui::TextDisabled("Maj : ajouter, Alt : soustraire, Maj+Alt : intersection");
        ImGui::SliderFloat("Contour progressif", &selFeather, 0, 250, "%.1f px", ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("Lissage (anti-crénelage)", &selAntialias);
        if (tool == Tool::Wand) {
            ImGui::SeparatorText("Baguette magique");
            ImGui::SliderFloat("Tolérance", &wandTolerance, 0, 255, "%.0f");
            ImGui::Checkbox("Pixels contigus", &wandContiguous);
            ImGui::Checkbox("Échantillonner tous les calques", &wandSampleMerged);
        }
        if (tool == Tool::PolyLasso) ImGui::TextDisabled("Clic : point. Double-clic / Entrée : fermer. Retour arrière : annuler un point.");
        if (v && v->doc->S().selection) {
            ImGui::Spacing();
            if (ImGui::Button("Désélectionner (Ctrl+D)")) v->renderer.Deselect(*v->doc, v->history);
            ImGui::SameLine();
            if (ImGui::Button("Intervertir")) v->renderer.SelectionFilter(*v->doc, *ctx, v->history, 3, 0, "Intervertir");
        }
        ImGui::Checkbox("Mode masque (Q)", &quickMask);
    } else if (tool == Tool::Gradient) {
        ImGui::SeparatorText("Dégradé");
        ImGui::Combo("Style", &gradientShape, "Linéaire\0" "Radial\0" "Angle\0" "Réfléchi\0" "Losange\0");
        ImGui::SliderFloat("Opacité", &gradientOpacity, 0, 1);
        ImGui::TextDisabled("Premier plan -> arrière-plan. Maj : 45°.");
    } else if (IsPaintTool(tool)) {
        BrushSettings& b = CurrentBrush();
        static const char* titles[] = {"", "", "", "", "", "", "Pinceau", "Gomme", "Tampon de duplication", "Correcteur", "", "",
                                       "Goutte", "Netteté", "Doigt", "Densité -", "Densité +", "Éponge"};
        ImGui::SeparatorText(titles[(int)tool]);
        if (IsRetouchTool(tool)) {
            ImGui::SliderFloat(tool == Tool::Clone || tool == Tool::Heal ? "Intensité (inutilisée)" : "Intensité", &retouchStrength, 0.01f, 1, "%.2f");
            if (tool == Tool::Dodge || tool == Tool::Burn) ImGui::Combo("Plage", &dodgeRange, "Tons foncés\0Tons moyens\0Tons clairs\0");
            if (tool == Tool::Sponge) {
                int m = spongeSaturate ? 1 : 0;
                if (ImGui::Combo("Mode", &m, "Désaturer\0Saturer\0")) spongeSaturate = m == 1;
            }
            if (tool == Tool::Clone || tool == Tool::Heal) {
                ImGui::Checkbox("Aligné", &cloneAligned);
                ImGui::TextDisabled(cloneSourceSet ? "Source définie (Alt+clic pour changer)" : "Alt+clic pour définir la source");
            }
            ImGui::Separator();
        }
        ImGui::SliderFloat("Taille", &b.size, 1, 2000, "%.0f px", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Dureté", &b.hardness, 0, 1, "%.2f");
        ImGui::SliderFloat("Opacité", &b.opacity, 0, 1, "%.2f");
        ImGui::SliderFloat("Flux", &b.flow, 0.01f, 1, "%.2f");
        ImGui::SliderFloat("Espacement", &b.spacing, 0.01f, 2.0f, "%.2f");
        ImGui::SliderFloat("Lissage", &b.smoothing, 0, 0.95f, "%.2f");
        ImGui::SeparatorText("Tablette");
        ImGui::Checkbox("Pression -> taille", &b.pressureSize);
        ImGui::Checkbox("Pression -> flux", &b.pressureOpacity);
        ImGui::TextDisabled(platform::PenActive() ? "Stylet détecté (pression %.2f)" : "Souris", platform::LastPressure());
        // Live preview of the dab profile.
        ImGui::Spacing();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x, h = 40 * dpi;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(20, 20, 24, 255), 4);
        const int N = 48;
        for (int i = 0; i < N; ++i) {
            float x = (float)i / (N - 1);
            float d = std::abs(x - 0.5f) * 2.0f;
            float inner = b.hardness;
            float cov = d <= inner ? 1.0f : 1.0f - std::clamp((d - inner) / std::max(1.0f - inner, 1e-3f), 0.0f, 1.0f);
            cov = cov * cov * (3 - 2 * cov);
            float bh = cov * (h - 8) * b.opacity;
            float x0 = p.x + 4 + x * (w - 8);
            dl->AddRectFilled(ImVec2(x0, p.y + h - 4 - bh), ImVec2(x0 + (w - 8) / N, p.y + h - 4), ui::col::Accent);
        }
        ImGui::Dummy(ImVec2(w, h));
    } else if (tool == Tool::Bucket) {
        ImGui::SeparatorText("Pot de peinture");
        ImGui::SliderFloat("Tolérance", &bucketTolerance, 0, 255, "%.0f");
        ImGui::Checkbox("Pixels contigus", &bucketContiguous);
        ImGui::Checkbox("Tous les calques", &bucketSampleMerged);
        ImGui::TextDisabled("Respecte la sélection. Alt+Retour : remplir toute la sélection.");
    } else {
        ImGui::TextDisabled("Aucune option pour cet outil.");
    }
    ImGui::PopItemWidth();
    ImGui::End();
}


}  // namespace sx
