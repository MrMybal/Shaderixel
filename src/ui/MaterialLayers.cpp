// Material layers (Substance Painter style). A material layer is one fill layer
// per PBR channel group (base color, roughness, metal, height, emissive), all
// tagged with the same material id and sharing ONE mask: painting the mask (in
// 2D or on the 3D model) applies every channel of the material at once.
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sx {

namespace {
// Channels a material layer can drive (normal / occlusion stay per-channel layers).
const PbrChannel kMatChannels[] = {PbrChannel::BaseColor, PbrChannel::Roughness, PbrChannel::Metallic, PbrChannel::Height, PbrChannel::Emissive};

bool IsColorChannel(PbrChannel c) { return c == PbrChannel::BaseColor || c == PbrChannel::Emissive; }

struct MatPreset {
    const char* name;
    float3 color;  // sRGB
    float rough, metal, height;
    bool hasColor = true, hasRough = true, hasMetal = true, hasHeight = false;
};

const std::vector<MatPreset>& Presets() {
    static const std::vector<MatPreset> p = {
        {"Peinture rouge", {0.70f, 0.08f, 0.06f}, 0.35f, 0.0f, 0.5f},
        {"Plastique blanc", {0.86f, 0.86f, 0.84f}, 0.42f, 0.0f, 0.5f},
        {"Caoutchouc", {0.05f, 0.05f, 0.05f}, 0.92f, 0.0f, 0.5f},
        {"Bois", {0.46f, 0.29f, 0.15f}, 0.72f, 0.0f, 0.5f},
        {"Or", {1.00f, 0.78f, 0.34f}, 0.25f, 1.0f, 0.5f},
        {"Cuivre", {0.95f, 0.64f, 0.54f}, 0.32f, 1.0f, 0.5f},
        {"Chrome", {0.92f, 0.92f, 0.94f}, 0.06f, 1.0f, 0.5f},
        {"Fer brut", {0.56f, 0.57f, 0.58f}, 0.55f, 1.0f, 0.5f},
        {"Rouille", {0.45f, 0.20f, 0.08f}, 0.88f, 0.15f, 0.56f, true, true, true, true},
        {"Boue", {0.25f, 0.18f, 0.11f}, 0.95f, 0.0f, 0.6f, true, true, true, true},
        {"Gravure (hauteur)", {0, 0, 0}, 0.6f, 0.0f, 0.3f, false, false, false, true},
        {"Relief (hauteur)", {0, 0, 0}, 0.6f, 0.0f, 0.75f, false, false, false, true},
        {"Brillance", {0, 0, 0}, 0.08f, 0.0f, 0.5f, false, true, false, false},
    };
    return p;
}

float4 ChannelValue(PbrChannel c, float3 color, float v) {
    if (IsColorChannel(c)) return {SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z), 1};
    float l = SrgbToLinear(v);  // data channels hold visual gray levels (see CreatePbrChannels)
    return {l, l, l, 1};
}
}  // namespace

std::vector<Id> App::MaterialIds(Document& d) {
    std::vector<Id> ids;
    for (int i = (int)d.S().layers.size() - 1; i >= 0; --i) {
        Id m = d.S().layers[i].material;
        if (m && std::find(ids.begin(), ids.end(), m) == ids.end()) ids.push_back(m);
    }
    return ids;
}

Layer* App::MaterialChannelLayer(Document& d, Id material, PbrChannel c) {
    Id g = FindChannelGroup(d, c);
    for (auto& l : d.S().layers)
        if (l.material == material && l.parent == g) return &l;
    return nullptr;
}

Layer* App::AnyMaterialLayer(Document& d, Id material) {
    for (auto& l : d.S().layers)
        if (l.material == material) return &l;
    return nullptr;
}

// Fill layer of `material` in channel group `c` (created on top of the group).
Layer* App::AddMaterialChannel(Document& d, Id material, PbrChannel c, const std::string& name, Id mask, float4 value, float opacity) {
    Id g = FindChannelGroup(d, c);
    auto solid = Effects().Find("solid_color");
    if (!g || !solid) return nullptr;
    Layer f;
    f.id = d.NewId();
    f.kind = LayerKind::Fill;
    f.name = name;
    f.parent = g;
    f.material = material;
    f.mask = mask;
    f.opacity = opacity;
    f.effect.effectId = "solid_color";
    f.effect.params = solid->DefaultParams();
    f.effect.params[0] = value;
    auto& L = d.S().layers;
    int gi = d.LayerIndex(g);
    L.insert(L.begin() + gi, f);  // just under the group entry = top of its children
    return &L[gi];
}

void App::AddMaterialLayer(DocView& v, int preset) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) { Status("Les matériaux utilisent le mode calques"); return; }
    if (!FindChannelGroup(d, PbrChannel::Roughness)) CreatePbrChannels(v);
    const MatPreset& p = Presets()[std::clamp(preset, 0, (int)Presets().size() - 1)];
    Edit e(d, v.history, "Nouveau matériau");
    Id material = d.NewId();
    Id mask = d.CreateSurface({0, 0, 0, 1});  // black: nothing until painted (like a Substance fill + black mask)
    Layer* first = nullptr;
    for (PbrChannel c : kMatChannels) {
        bool on = c == PbrChannel::BaseColor ? p.hasColor : c == PbrChannel::Roughness ? p.hasRough : c == PbrChannel::Metallic ? p.hasMetal
                  : c == PbrChannel::Height ? p.hasHeight : false;
        if (!on) continue;
        float val = c == PbrChannel::Roughness ? p.rough : c == PbrChannel::Metallic ? p.metal : p.height;
        Layer* l = AddMaterialChannel(d, material, c, p.name, mask, ChannelValue(c, p.color, val), 1.0f);
        if (l && !first) first = l;
    }
    if (Layer* l = AnyMaterialLayer(d, material)) {
        d.S().activeLayer = l->id;
        d.S().editMask = true;  // brushes paint the shared mask
    }
    d.SyncGraph();
    e.Commit();
    materialSel = material;
    Status(std::string("Matériau « ") + p.name + " » : peignez-le sur le modèle ou le canevas (masque partagé)");
}

void App::SelectMaterial(Document& d, Id material) {
    materialSel = material;
    if (Layer* l = AnyMaterialLayer(d, material)) {
        d.S().activeLayer = l->id;
        d.S().editMask = l->mask != 0;
    }
}

void App::ApplyPaint3DLayout() {
    // Center split: 3D view on the left, the document (UV texture) on the right.
    ImGuiWindow* cw = ImGui::FindWindowByName("Canevas");
    ImGuiDockNode* node = cw ? cw->DockNode : nullptr;
    if (!node) return;
    ImGuiID left = 0, right = 0;
    ImGui::DockBuilderSplitNode(node->ID, ImGuiDir_Left, 0.56f, &left, &right);
    ImGui::DockBuilderDockWindow("Vue 3D", left);
    ImGui::DockBuilderDockWindow("Canevas", right);
    ImGuiDockNode* root = node;
    while (root->ParentNode) root = root->ParentNode;
    ImGui::DockBuilderFinish(root->ID);
}

void App::DrawMaterialPanel() {
    if (!ImGui::Begin("Matériaux")) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v) { ImGui::TextDisabled("Aucun document"); ImGui::End(); return; }
    Document& d = *v->doc;
    if (d.Mode() != DocMode::Layers) { ImGui::TextDisabled("Les matériaux utilisent le mode calques."); ImGui::End(); return; }

    // Workspace: 3D view next to the UV texture, mesh.
    if (ImGui::Button("Espace peinture 3D")) {
        paint3DLayoutRequest = true;
        if (!v->mesh) AttachMesh(*v, MakeSphere());
    }
    ui::HelpTooltip("Vue 3D à gauche, texture (UV) à droite ; peignez sur le modèle.");
    ImGui::SameLine();
    if (!FindChannelGroup(d, PbrChannel::Roughness) && ImGui::Button("Créer les canaux PBR")) CreatePbrChannels(*v);

    // Shelf: presets.
    ImGui::SeparatorText("Nouveau matériau");
    const float fs = ImGui::GetFontSize();
    const float sw = 74 * dpi;
    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + 6 * dpi) / (sw + 6 * dpi)));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < (int)Presets().size(); ++i) {
        const MatPreset& p = Presets()[i];
        if (i % cols) ImGui::SameLine(0, 6 * dpi);
        ImGui::PushID(i);
        ImVec2 a = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##preset", ImVec2(sw, sw + fs * 1.3f))) AddMaterialLayer(*v, i);
        bool hov = ImGui::IsItemHovered();
        // Shaded ball: base color, highlight by roughness, metal tint.
        ImVec2 c(a.x + sw * 0.5f, a.y + sw * 0.5f);
        float r = sw * 0.42f;
        float3 base = p.hasColor ? p.color : float3{0.5f, 0.5f, 0.5f};
        for (int k = 12; k >= 1; --k) {
            float t = k / 12.0f;
            float shade = 0.35f + 0.65f * (1.0f - t * t);
            ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x * shade, base.y * shade, base.z * shade, 1));
            dl->AddCircleFilled(ImVec2(c.x - r * 0.18f * (1 - t), c.y - r * 0.18f * (1 - t)), r * t, col, 32);
        }
        float spec = (1.0f - p.rough) * (p.metal > 0.5f ? 0.9f : 0.6f);
        dl->AddCircleFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.35f), r * (0.08f + 0.3f * p.rough), IM_COL32(255, 255, 255, (int)(spec * 200)), 20);
        if (p.hasHeight && !p.hasColor) dl->AddCircle(c, r * 0.6f, IM_COL32(220, 220, 230, 160), 24, 2.0f);
        if (hov) dl->AddCircle(c, r + 2, ui::col::Accent, 32, 2.0f);
        ImVec4 clip(a.x, a.y, a.x + sw, a.y + sw + fs * 1.3f);
        ImVec2 ts = ImGui::CalcTextSize(p.name);
        dl->AddText(nullptr, 0, ImVec2(a.x + std::max(0.0f, (sw - ts.x) * 0.5f), a.y + sw), ImGui::GetColorU32(ImGuiCol_Text), p.name, nullptr, 0, &clip);
        if (hov) ImGui::SetTooltip("%s\nRugosité %.2f  Métal %.2f%s\nClic : nouveau calque de matériau (masque noir, à peindre)", p.name, p.rough, p.metal,
                                   p.hasHeight ? "  + hauteur" : "");
        ImGui::PopID();
    }

    // Material layer stack (top first).
    ImGui::SeparatorText("Calques de matériau");
    std::vector<Id> mats = MaterialIds(d);
    if (mats.empty()) ImGui::TextDisabled("Aucun : choisissez un matériau ci-dessus.");
    if (Layer* al = d.ActiveLayer(); al && al->material) materialSel = al->material;
    Id remove = 0;
    for (Id m : mats) {
        Layer* any = AnyMaterialLayer(d, m);
        if (!any) continue;
        ImGui::PushID((int)m);
        bool sel = materialSel == m;
        bool vis = any->visible;
        if (ImGui::Checkbox("##vis", &vis)) {
            Edit e(d, v->history, "Visibilité du matériau");
            for (auto& l : d.S().layers) if (l.material == m) l.visible = vis;
            d.SyncGraph();
            e.Commit();
        }
        ImGui::SameLine();
        // Swatch of the base color (or gray).
        Layer* bc = MaterialChannelLayer(d, m, PbrChannel::BaseColor);
        float4 col = bc && !bc->effect.params.empty() ? bc->effect.params[0] : float4{0.4f, 0.4f, 0.4f, 1};
        ImVec2 sp = ImGui::GetCursorScreenPos();
        float hh = ImGui::GetFrameHeight();
        dl->AddRectFilled(sp, ImVec2(sp.x + hh, sp.y + hh),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(LinearToSrgb(col.x), LinearToSrgb(col.y), LinearToSrgb(col.z), 1)), 3);
        ImGui::Dummy(ImVec2(hh, hh));
        ImGui::SameLine();
        if (ImGui::Selectable(any->name.c_str(), sel, 0, ImVec2(0, hh))) SelectMaterial(d, m);
        if (ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem("Supprimer le matériau")) remove = m;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (remove) {
        Edit e(d, v->history, "Supprimer le matériau");
        std::vector<Id> ids;
        for (auto& l : d.S().layers) if (l.material == remove) ids.push_back(l.id);
        for (Id id : ids) d.RemoveLayer(id);
        e.Commit();
        materialSel = 0;
    }

    // Properties of the selected material: one row per channel.
    Layer* any = materialSel ? AnyMaterialLayer(d, materialSel) : nullptr;
    if (any) {
        ImGui::SeparatorText("Propriétés du matériau");
        bool changed = false;
        std::string name = any->name;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##mname", &name)) {
            for (auto& l : d.S().layers) if (l.material == materialSel) l.name = name;
            changed = true;
        }
        float opacity = any->opacity * 100.0f;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##mop", &opacity, 0, 100, "Opacité %.0f %%")) {
            for (auto& l : d.S().layers) if (l.material == materialSel) l.opacity = opacity / 100.0f;
            changed = true;
        }
        for (PbrChannel c : kMatChannels) {
            ImGui::PushID((int)c);
            Layer* l = MaterialChannelLayer(d, materialSel, c);
            bool on = l != nullptr;
            if (ImGui::Checkbox(PbrChannelName(c), &on)) {
                Edit e(d, v->history, "Canal du matériau");
                if (on) AddMaterialChannel(d, materialSel, c, any->name, any->mask, ChannelValue(c, {0.5f, 0.5f, 0.5f}, 0.5f), any->opacity);
                else if (l) d.RemoveLayer(l->id);
                d.SyncGraph();
                e.Commit();
                l = MaterialChannelLayer(d, materialSel, c);
                any = AnyMaterialLayer(d, materialSel);
            }
            if (l && !l->effect.params.empty()) {
                ImGui::SameLine(150 * dpi);
                ImGui::SetNextItemWidth(-FLT_MIN);
                float4& p = l->effect.params[0];
                if (IsColorChannel(c)) {
                    float s[3] = {LinearToSrgb(p.x), LinearToSrgb(p.y), LinearToSrgb(p.z)};
                    if (ImGui::ColorEdit3("##val", s)) {
                        p = {SrgbToLinear(s[0]), SrgbToLinear(s[1]), SrgbToLinear(s[2]), 1};
                        changed = true;
                    }
                } else {
                    float g = LinearToSrgb(p.x);
                    if (ImGui::SliderFloat("##val", &g, 0, 1, "%.2f")) {
                        float lin = SrgbToLinear(g);
                        p = {lin, lin, lin, 1};
                        changed = true;
                    }
                }
            }
            ImGui::PopID();
            if (!any) break;
        }
        if (any && any->mask) {
            ImGui::TextDisabled("Masque partagé : peindre en blanc applique le matériau.");
            if (ImGui::SmallButton("Remplir tout"))
                v->renderer.FillSelection(d, *ctx, &v->history, any->mask, 0, {1, 1, 1, 1}, 1.0f, "Remplir le masque", nullptr, nullptr, false);
            ImGui::SameLine();
            if (ImGui::SmallButton("Vider"))
                v->renderer.FillSelection(d, *ctx, &v->history, any->mask, 0, {0, 0, 0, 1}, 1.0f, "Vider le masque", nullptr, nullptr, false);
            ImGui::SameLine();
            if (ImGui::SmallButton("Peindre le masque")) SelectMaterial(d, materialSel);
        }
        if (changed) {
            d.SyncGraph();
            if (!materialEditBefore) materialEditBefore = frameBefore;
        }
        if (materialEditBefore && !ImGui::IsAnyItemActive()) {
            v->history.PushState("Modifier le matériau", *materialEditBefore, d.S());
            materialEditBefore.reset();
        }
    }
    ImGui::End();
}

}  // namespace sx
