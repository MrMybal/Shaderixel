// Gradient tool's editor: a bar with its stops (click to add one, drag to move, right click to remove), the selected
// stop's color and opacity (or the foreground / background colors it follows), presets, reverse, dither,
// perceptual interpolation.
#include "ui/App.h"
#include "ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace sx {

void App::GradientEditorUI() {
    auto StopColor = [](const GradStop& s, float4 fg, float4 bg) {
        return s.follow == 1 ? float4{fg.x, fg.y, fg.z, s.color.w} : s.follow == 2 ? float4{bg.x, bg.y, bg.z, s.color.w} : s.color;
    };
    auto& S = gradStops;
    if (S.size() < 2) S = {{0, {0, 0, 0, 1}, 1}, {1, {1, 1, 1, 1}, 2}};
    gradSel = std::clamp(gradSel, 0, (int)S.size() - 1);
    // Presets.
    ImGui::SetNextItemWidth(200 * dpi);
    if (ImGui::BeginCombo("Préréglage", "Choisir...")) {
        struct P { const char* name; std::vector<GradStop> stops; };
        const P presets[] = {
            {"Premier plan -> arrière-plan", {{0, {0, 0, 0, 1}, 1}, {1, {1, 1, 1, 1}, 2}}},
            {"Premier plan -> transparent", {{0, {0, 0, 0, 1}, 1}, {1, {0, 0, 0, 0}, 1}}},
            {"Noir -> blanc", {{0, {0, 0, 0, 1}}, {1, {1, 1, 1, 1}}}},
            {"Arc-en-ciel", {{0, {1, 0, 0, 1}}, {0.17f, {1, 0.5f, 0, 1}}, {0.33f, {1, 1, 0, 1}}, {0.5f, {0, 0.8f, 0, 1}}, {0.67f, {0, 0.6f, 1, 1}},
                             {0.83f, {0.3f, 0, 0.8f, 1}}, {1, {0.6f, 0, 0.6f, 1}}}},
            {"Coucher de soleil", {{0, {0.15f, 0.05f, 0.35f, 1}}, {0.45f, {0.85f, 0.25f, 0.35f, 1}}, {0.75f, {1, 0.6f, 0.2f, 1}}, {1, {1, 0.9f, 0.55f, 1}}}},
            {"Chrome", {{0, {0.2f, 0.25f, 0.3f, 1}}, {0.45f, {0.95f, 0.97f, 1, 1}}, {0.5f, {0.35f, 0.3f, 0.25f, 1}}, {0.65f, {0.6f, 0.55f, 0.45f, 1}}, {1, {0.95f, 0.95f, 0.9f, 1}}}},
            {"Cuivre", {{0, {0.45f, 0.2f, 0.1f, 1}}, {0.4f, {0.95f, 0.65f, 0.45f, 1}}, {0.7f, {0.6f, 0.3f, 0.15f, 1}}, {1, {0.9f, 0.6f, 0.4f, 1}}}},
            {"Transparent -> noir", {{0, {0, 0, 0, 0}}, {1, {0, 0, 0, 1}}}},
        };
        for (auto& p : presets)
            if (ImGui::Selectable(p.name)) {
                S = p.stops;
                gradSel = 0;
            }
        ImGui::EndCombo();
    }
    // Shared gradient view; Shaderixel retains follower colors, interpolation and presets.
    std::vector<cyui::GradientStop> model;
    for (auto& stop : S) {
        if (!stop.uiId) stop.uiId = gradUiNext++;
        const auto color = StopColor(stop, fg, bg);
        model.push_back({stop.uiId, gradReverse ? 1.0 - stop.pos : stop.pos,
                         ImVec4(color.x, color.y, color.z, color.w)});
    }
    const auto sameModel = [](const auto& a, const auto& b) {
        if (a.size() != b.size()) return false;
        for (size_t i=0; i<a.size(); ++i) {
            if (a[i].id != b[i].id || a[i].position != b[i].position) return false;
            const auto& x=a[i].color; const auto& y=b[i].color;
            if (x.x!=y.x || x.y!=y.y || x.z!=y.z || x.w!=y.w) return false;
        }
        return true;
    };
    if (!sameModel(model, gradUiModel)) { ++gradUiRevision; gradUiModel = model; }
    if (!gradUi.active) { gradUi.selected = S[gradSel].uiId; gradUi.hasSelection = true; }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 room(std::max(cyui::dp(120.f), ImGui::GetContentRegionAvail().x), cyui::dp(48.f));
    auto edit = cyui::gradientEditor("##gradbar", gradUi, model, room, !gradPerceptual, gradUiRevision);
    // Preserve the existing single-click-to-add gesture (CyUI also supports double-click).
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0) && !gradUi.active &&
        edit.phase == cyui::EditPhase::none && S.size() < 16) {
        const double position = std::clamp(double((ImGui::GetIO().MousePos.x-origin.x)/room.x), 0., 1.);
        const auto id = gradUiNext++;
        gradUi.before = model;
        gradUi.preview = model;
        gradUi.preview.push_back({id, position, cyui::sampleGradient(model, position, !gradPerceptual)});
        gradUi.selected = gradUi.dragging = id;
        gradUi.hasSelection = gradUi.active = true;
        gradUi.revision = gradUiRevision;
    }
    if (edit.committed() && edit.value.size() >= 2 && edit.value.size() <= 16) {
        std::vector<GradStop> accepted;
        for (const auto& proposal : edit.value) {
            auto old = std::find_if(S.begin(), S.end(), [&](const auto& stop) { return stop.uiId == proposal.id; });
            GradStop stop;
            if (old != S.end()) stop = *old;
            else { stop.color = {proposal.color.x, proposal.color.y, proposal.color.z, proposal.color.w}; stop.uiId = proposal.id; }
            stop.pos = (float)(gradReverse ? 1-proposal.position : proposal.position);
            accepted.push_back(stop);
            gradUiNext = std::max(gradUiNext, proposal.id+1);
        }
        S = std::move(accepted);
    }
    for (int i=0; i<(int)S.size(); ++i) if (S[i].uiId == gradUi.selected) gradSel=i;
    gradSel = std::clamp(gradSel, 0, (int)S.size()-1);
    ui::HelpTooltip("Clic sur la barre : ajouter un arrêt. Glisser un repère : le déplacer. Clic droit : le supprimer.");
    // The selected stop.
    GradStop& st = S[gradSel];
    ImGui::SetNextItemWidth(160 * dpi);
    ImGui::Combo("Couleur de l'arrêt##gsrc", &st.follow, "Couleur choisie\0Premier plan\0Arrière-plan\0");
    if (st.follow == 0) ImGui::ColorEdit3("##gstopc", &st.color.x);
    ImGui::SetNextItemWidth(160 * dpi);
    ImGui::SliderFloat("Opacité de l'arrêt", &st.color.w, 0, 1, "%.2f");
    ImGui::SetNextItemWidth(160 * dpi);
    ImGui::SliderFloat("Position", &st.pos, 0, 1, "%.3f");
    ImGui::Checkbox("Inverser", &gradReverse);
    ImGui::SameLine();
    ImGui::Checkbox("Tramage", &gradDither);
    ImGui::SameLine();
    ImGui::Checkbox("Perceptuel", &gradPerceptual);
    ui::HelpTooltip("Mélange des couleurs en sRGB, comme Photoshop (sinon en lumière linéaire, plus physique).");
}

}  // namespace sx
