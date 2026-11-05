#include "ui/BlendIf.h"
#include "ui/CyUiState.h"
#include "ui/Theme.h"

namespace sx::ui {
// Photoshop-style Blend If bar: black-to-white gradient with 4 handles
// (lo0, lo1, hi0, hi1). Each pair moves together until split with Alt-drag.
bool BlendIfSlider(const char* id, u8 v[4], ImU32 hiColor, const std::function<void()>& track) {
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    const float dpi = ImGui::GetFontSize() / 16.0f;
    const float w = ImGui::GetContentRegionAvail().x, barH = 10 * dpi, handle = 6 * dpi;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float x0 = p0.x + handle, x1 = p0.x + w - handle;
    ImGui::InvisibleButton(id, ImVec2(w, barH + handle * 2 + ImGui::GetFontSize()));
    ImGuiID gid = ImGui::GetItemID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(ImVec2(x0, p0.y), ImVec2(x1, p0.y + barH), IM_COL32(0, 0, 0, 255), hiColor, hiColor, IM_COL32(0, 0, 0, 255));
    auto xOf = [&](int val) { return x0 + (x1 - x0) * val / 255.0f; };
    struct DragState { int handle = -1; bool pair = true; };
    auto& state = CyUiViewState<DragState>(gid);
    int& dragging = state.handle;
    bool& dragPair = state.pair;
    bool changed = false;
    if (ImGui::IsItemActivated()) {
        float mx = ImGui::GetIO().MousePos.x, best = 1e9f;
        for (int i = 0; i < 4; ++i) {
            float dd = std::abs(xOf(v[i]) - mx);
            // On ties prefer the outer handle in the direction of the click.
            if (dd < best - 0.01f || (dd < best + 0.01f && ((i < 2 && mx < xOf(v[i])) || (i >= 2 && mx > xOf(v[i]))))) { best = dd; dragging = i; }
        }
        dragPair = !ImGui::GetIO().KeyAlt;
    }
    if (ImGui::IsItemActive() && dragging >= 0) {
        int val = (int)std::lround((ImGui::GetIO().MousePos.x - x0) / (x1 - x0) * 255.0f);
        int i = dragging, j = i ^ 1;  // partner handle of the same pair
        int lo = i < 2 ? 0 : v[1], hi = i < 2 ? v[2] : 255;
        if (dragPair && v[i] == v[j]) {
            int delta = std::clamp(val, lo, hi) - v[i];
            int a = std::clamp(v[i] + delta, lo, hi), b = std::clamp(v[j] + delta, lo, hi);
            if (a != v[i] || b != v[j]) { v[i] = (u8)a; v[j] = (u8)b; changed = true; }
        } else {
            // Split: keep lo0 <= lo1 and hi0 <= hi1.
            int mn = (i == 1) ? v[0] : (i == 3 ? v[2] : lo), mx = (i == 0) ? v[1] : (i == 2 ? v[3] : hi);
            mn = std::max(mn, lo);
            mx = std::min(mx, hi);
            int a = std::clamp(val, mn, std::max(mn, mx));
            if (a != v[i]) { v[i] = (u8)a; changed = true; }
        }
    }
    if (!ImGui::IsItemActive()) dragging = -1;
    for (int i = 0; i < 4; ++i) {
        float x = xOf(v[i]), y = p0.y + barH + 1;
        bool white = i >= 2;
        ImU32 fill = white ? IM_COL32(240, 240, 240, 255) : IM_COL32(20, 20, 20, 255);
        // Split handles are drawn as half triangles.
        bool split = v[(i & 2)] != v[(i & 2) + 1];
        float l = split && (i & 1) == 0 ? 0 : handle, r = split && (i & 1) == 1 ? 0 : handle;
        if (!split && (i & 1)) continue;  // joined pair: draw once
        dl->AddTriangleFilled(ImVec2(x, y), ImVec2(x + r, y + handle * 1.6f), ImVec2(x - l, y + handle * 1.6f), fill);
        dl->AddTriangle(ImVec2(x, y), ImVec2(x + r, y + handle * 1.6f), ImVec2(x - l, y + handle * 1.6f), IM_COL32(128, 128, 128, 255));
    }
    char txt[64];
    if (v[0] == v[1]) snprintf(txt, sizeof(txt), "%d", v[0]);
    else snprintf(txt, sizeof(txt), "%d / %d", v[0], v[1]);
    dl->AddText(ImVec2(x0, p0.y + barH + handle * 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), txt);
    if (v[2] == v[3]) snprintf(txt, sizeof(txt), "%d", v[3]);
    else snprintf(txt, sizeof(txt), "%d / %d", v[2], v[3]);
    dl->AddText(ImVec2(x1 - ImGui::CalcTextSize(txt).x, p0.y + barH + handle * 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), txt);
    if (changed) ImGui::MarkItemEdited(gid);
    if (track) track();
    // Four explicit numeric fields complement the bar and split handles.
    ImGui::PushID(id);
    ImGui::PushMultiItemsWidths(4, ImGui::GetContentRegionAvail().x);
    const char* hints[] = {"Début des tons foncés (0–255)", "Fin des tons foncés (0–255)",
                           "Début des tons clairs (0–255)", "Fin des tons clairs (0–255)"};
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        int value = v[i];
        if (ImGui::DragInt("##value", &value, 1, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            v[i] = (u8)std::clamp(value, 0, 255);
            // Preserve the typed threshold, moving neighbours only if needed.
            for (int j = i - 1; j >= 0; --j) v[j] = std::min(v[j], v[j + 1]);
            for (int j = i + 1; j < 4; ++j) v[j] = std::max(v[j], v[j - 1]);
            changed = true;
        }
        if (track) track();
        HelpTooltip(hints[i]);
        ImGui::PopID(); ImGui::PopItemWidth();
        if (i != 3) ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    }
    ImGui::PopID();
    (void)win;
    return changed;
}
}
