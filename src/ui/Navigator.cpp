// Navigator panel (like Photoshop's): the whole image small, the part shown in the canvas framed in red; click or
// drag in it to look elsewhere, zoom slider and buttons below (zoom around the view's center).
#include "ui/App.h"
#include "ui/Theme.h"

#include <cmath>

namespace sx {

namespace {
// Zoom about the view's center (the same document point stays in the middle of the canvas).
void ZoomAboutCenter(DocView& v, float z) {
    z = std::clamp(z, 0.01f, 128.0f);  // (the canvas' own range)
    const float2 half = v.viewSize * 0.5f;
    const float2 c = (half - v.pan) * (1.0f / v.zoom);
    v.zoom = z;
    v.pan = half - c * z;
    v.fitHold = 0;  // (a fit still settling would undo the user's zoom)
}
// Transparency checkerboard under the image.
void Checker(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float cell) {
    dl->AddRectFilled(p0, p1, IM_COL32(204, 204, 204, 255));
    int row = 0;
    for (float y = p0.y; y < p1.y; y += cell, ++row)
        for (float x = p0.x + ((row & 1) ? cell : 0); x < p1.x; x += cell * 2)
            dl->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + cell, p1.x), std::min(y + cell, p1.y)), IM_COL32(255, 255, 255, 255));
}
}  // namespace

void App::DrawNavigator() {
    // (no scrolling with the wheel: it zooms here)
    if (!ImGui::Begin("Navigateur", PanelFlag("Navigateur"), ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }
    DocView* v = Active();
    if (!v || !v->lastImage) {
        ImGui::TextDisabled("Aucun document ouvert");
        ImGui::End();
        return;
    }
    if (v->viewSize.x <= 0) {  // (opened while the canvas tab is hidden: its size is not known yet)
        ImGui::TextDisabled("Affichez le canevas pour naviguer");
        ImGui::End();
        return;
    }
    Document& d = *v->doc;
    const float W = (float)d.Width(), H = (float)d.Height();
    const float footer = ImGui::GetFrameHeightWithSpacing() + 4 * dpi;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float boxW = std::max(40.0f, avail.x), boxH = std::max(40.0f, std::min(avail.y - footer, boxW * H / W));  // (the zoom row right below)
    const float s = std::min(boxW / W, boxH / H);
    const float tw = std::max(1.0f, std::floor(W * s)), th = std::max(1.0f, std::floor(H * s));
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 p0(start.x + std::floor((boxW - tw) * 0.5f), start.y + std::floor((boxH - th) * 0.5f));
    const ImVec2 p1(p0.x + tw, p0.y + th);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // The image (refreshed a few times a second: it is small).
    Checker(dl, p0, p1, 6 * dpi);
    const u64 hash = (u64)(uintptr_t)v->lastImage ^ (v->renderer.ImageVersion() << 1);  // (redrawn when the image changes)
    if (gpu::Texture* t = v->renderer.Thumbnail(*ctx, 0xA5A5000000000000ull, v->lastImage, hash, (u32)tw, (u32)th))
        dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), p0, p1);
    dl->AddRect(p0, p1, ui::col::Border);
    // The canvas' visible part, in document pixels.
    const float iz = 1.0f / v->zoom;
    const float2 a = {-v->pan.x * iz, -v->pan.y * iz};
    const float2 b = a + v->viewSize * iz;
    const ImVec2 r0(p0.x + std::clamp(a.x, 0.0f, W) * s, p0.y + std::clamp(a.y, 0.0f, H) * s);
    const ImVec2 r1(p0.x + std::clamp(b.x, 0.0f, W) * s, p0.y + std::clamp(b.y, 0.0f, H) * s);
    if (r1.x > r0.x && r1.y > r0.y) {
        dl->AddRect(ImVec2(r0.x - 1, r0.y - 1), ImVec2(r1.x + 1, r1.y + 1), IM_COL32(0, 0, 0, 150), 0, 0, 3.0f);
        dl->AddRect(r0, r1, IM_COL32(235, 60, 50, 255), 0, 0, 2.0f);
    }
    // Click / drag: the view centered there.
    ImGui::SetCursorScreenPos(start);
    ImGui::InvisibleButton("##nav", ImVec2(boxW, boxH));
    if (ImGui::IsItemActive()) {
        const ImVec2 m = ImGui::GetMousePos();
        const float2 c = {std::clamp((m.x - p0.x) / s, 0.0f, W), std::clamp((m.y - p0.y) / s, 0.0f, H)};
        v->pan = v->viewSize * 0.5f - c * v->zoom;
        v->fitHold = 0;  // (the user placed the view)
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        if (float wh = ImGui::GetIO().MouseWheel; wh != 0) ZoomAboutCenter(*v, v->zoom * std::pow(1.2f, wh));
    }
    // Zoom: buttons and a logarithmic slider (1 % .. 6400 %).
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + boxH + 4 * dpi));
    if (ImGui::Button("-", ImVec2(ImGui::GetFrameHeight(), 0))) ZoomAboutCenter(*v, v->zoom / 1.25f);
    ImGui::SameLine();
    float pct = v->zoom * 100.0f;
    const float side = ImGui::GetFrameHeight() * 2 + ImGui::CalcTextSize("Ajuster").x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x * 3;
    ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x - side));
    if (ImGui::SliderFloat("##zoom", &pct, 1.0f, 12800.0f, "%.0f %%", ImGuiSliderFlags_Logarithmic)) ZoomAboutCenter(*v, pct / 100.0f);
    ImGui::SameLine();
    if (ImGui::Button("+", ImVec2(ImGui::GetFrameHeight(), 0))) ZoomAboutCenter(*v, v->zoom * 1.25f);
    ImGui::SameLine();
    if (ImGui::Button("Ajuster")) v->fitPending = true;
    ImGui::SetItemTooltip("Toute l'image dans le canevas");
    ImGui::End();
}

}  // namespace sx
