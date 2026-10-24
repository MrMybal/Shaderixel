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
    // The bar (checker under it for the transparency) and its markers.
    const float w = std::max(120.0f, ImGui::GetContentRegionAvail().x), h = 22 * dpi, mk = 8 * dpi;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##gradbar", ImVec2(w, h + mk * 2));
    const bool barHovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int cx = 0; cx * 6 * dpi < w; ++cx)
        for (int cy = 0; cy * 6 * dpi < h; ++cy)
            if ((cx + cy) & 1)
                dl->AddRectFilled(ImVec2(p0.x + cx * 6 * dpi, p0.y + cy * 6 * dpi), ImVec2(std::min(p0.x + w, p0.x + (cx + 1) * 6 * dpi), std::min(p0.y + h, p0.y + (cy + 1) * 6 * dpi)),
                                  IM_COL32(200, 200, 200, 255));
            else
                dl->AddRectFilled(ImVec2(p0.x + cx * 6 * dpi, p0.y + cy * 6 * dpi), ImVec2(std::min(p0.x + w, p0.x + (cx + 1) * 6 * dpi), std::min(p0.y + h, p0.y + (cy + 1) * 6 * dpi)),
                                  IM_COL32(255, 255, 255, 255));
    auto order = S;
    std::stable_sort(order.begin(), order.end(), [](const GradStop& a, const GradStop& b) { return a.pos < b.pos; });
    auto colorAt = [&](float t) {
        float4 a = StopColor(order[0], fg, bg);
        float pa = order[0].pos;
        if (t <= pa) return a;
        for (size_t i = 1; i < order.size(); ++i) {
            const float4 b = StopColor(order[i], fg, bg);
            if (t <= order[i].pos) {
                const float k = order[i].pos > pa ? (t - pa) / (order[i].pos - pa) : 1.0f;
                return float4{a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k, a.w + (b.w - a.w) * k};
            }
            a = b;
            pa = order[i].pos;
        }
        return a;
    };
    const int steps = (int)(w / 3);
    for (int i = 0; i < steps; ++i) {
        float t = (i + 0.5f) / steps;
        if (gradReverse) t = 1 - t;
        const float4 c = colorAt(t);
        dl->AddRectFilled(ImVec2(p0.x + w * i / steps, p0.y), ImVec2(p0.x + w * (i + 1) / steps + 1, p0.y + h),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w)));
    }
    dl->AddRect(p0, ImVec2(p0.x + w, p0.y + h), IM_COL32(0, 0, 0, 120));
    static int dragStop = -1;
    int hoverStop = -1;
    const ImVec2 m = ImGui::GetIO().MousePos;
    for (int i = 0; i < (int)S.size(); ++i) {
        const float x = p0.x + w * S[i].pos;
        const ImVec2 tri[3] = {ImVec2(x, p0.y + h), ImVec2(x - mk, p0.y + h + mk * 1.6f), ImVec2(x + mk, p0.y + h + mk * 1.6f)};
        const float4 c = StopColor(S[i], fg, bg);
        dl->AddTriangleFilled(tri[0], tri[1], tri[2], ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, 1)));
        dl->AddTriangle(tri[0], tri[1], tri[2], i == gradSel ? ui::col::Accent : IM_COL32(40, 40, 40, 255), i == gradSel ? 2.0f : 1.0f);
        if (S[i].follow) dl->AddText(ImVec2(x - 3 * dpi, p0.y + h + mk * 1.7f), IM_COL32(200, 200, 200, 255), S[i].follow == 1 ? "P" : "A");
        if (std::abs(m.x - x) < mk && m.y > p0.y + h - 2 && m.y < p0.y + h + mk * 2) hoverStop = i;
    }
    if (barHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (hoverStop >= 0) {
            dragStop = gradSel = hoverStop;
        } else {
            // A new stop with the color the gradient has there.
            const float t = std::clamp((m.x - p0.x) / w, 0.0f, 1.0f);
            if (S.size() < 16) {
                S.push_back({t, colorAt(t), 0});
                gradSel = dragStop = (int)S.size() - 1;
            }
        }
    }
    if (dragStop >= 0) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && dragStop < (int)S.size()) S[dragStop].pos = std::clamp((m.x - p0.x) / w, 0.0f, 1.0f);
        else dragStop = -1;
    }
    if (barHovered && hoverStop >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && S.size() > 2) {
        S.erase(S.begin() + hoverStop);
        gradSel = std::min(gradSel, (int)S.size() - 1);
    }
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
