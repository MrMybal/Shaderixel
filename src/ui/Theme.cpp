#include "ui/Theme.h"

#include <cmath>
#include <filesystem>

namespace sx::ui {

namespace {
Fonts g_fonts;

ImVec4 C(int r, int g, int b, int a = 255) { return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f); }
}  // namespace

Fonts& GetFonts() { return g_fonts; }

void ApplyTheme(float dpi) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowPadding = ImVec2(10, 10);
    s.FramePadding = ImVec2(8, 5);
    s.CellPadding = ImVec2(6, 4);
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.IndentSpacing = 16;
    s.ScrollbarSize = 11;
    s.GrabMinSize = 10;
    s.WindowBorderSize = 1;
    s.ChildBorderSize = 1;
    s.PopupBorderSize = 1;
    s.FrameBorderSize = 0;
    s.TabBorderSize = 0;
    s.WindowRounding = 6;
    s.ChildRounding = 5;
    s.FrameRounding = 5;
    s.PopupRounding = 6;
    s.ScrollbarRounding = 8;
    s.GrabRounding = 4;
    s.TabRounding = 5;
    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.SeparatorTextBorderSize = 1;
    s.DockingSeparatorSize = 3;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = C(222, 224, 232);
    c[ImGuiCol_TextDisabled] = C(120, 124, 138);
    c[ImGuiCol_WindowBg] = C(27, 28, 33);
    c[ImGuiCol_ChildBg] = C(23, 24, 28);
    c[ImGuiCol_PopupBg] = C(32, 33, 39, 250);
    c[ImGuiCol_Border] = C(48, 50, 58);
    c[ImGuiCol_BorderShadow] = C(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = C(40, 42, 50);
    c[ImGuiCol_FrameBgHovered] = C(52, 54, 64);
    c[ImGuiCol_FrameBgActive] = C(60, 58, 84);
    c[ImGuiCol_TitleBg] = C(22, 23, 27);
    c[ImGuiCol_TitleBgActive] = C(24, 25, 30);
    c[ImGuiCol_TitleBgCollapsed] = C(22, 23, 27);
    c[ImGuiCol_MenuBarBg] = C(22, 23, 27);
    c[ImGuiCol_ScrollbarBg] = C(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = C(64, 66, 78);
    c[ImGuiCol_ScrollbarGrabHovered] = C(84, 86, 100);
    c[ImGuiCol_ScrollbarGrabActive] = C(124, 92, 255);
    c[ImGuiCol_CheckMark] = C(150, 124, 255);
    c[ImGuiCol_SliderGrab] = C(124, 92, 255);
    c[ImGuiCol_SliderGrabActive] = C(150, 124, 255);
    c[ImGuiCol_Button] = C(44, 46, 55);
    c[ImGuiCol_ButtonHovered] = C(60, 62, 76);
    c[ImGuiCol_ButtonActive] = C(124, 92, 255);
    c[ImGuiCol_Header] = C(124, 92, 255, 70);
    c[ImGuiCol_HeaderHovered] = C(124, 92, 255, 110);
    c[ImGuiCol_HeaderActive] = C(124, 92, 255, 160);
    c[ImGuiCol_Separator] = C(48, 50, 58);
    c[ImGuiCol_SeparatorHovered] = C(124, 92, 255, 160);
    c[ImGuiCol_SeparatorActive] = C(124, 92, 255);
    c[ImGuiCol_ResizeGrip] = C(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered] = C(124, 92, 255, 120);
    c[ImGuiCol_ResizeGripActive] = C(124, 92, 255);
    c[ImGuiCol_Tab] = C(30, 31, 37);
    c[ImGuiCol_TabHovered] = C(64, 56, 110);
    c[ImGuiCol_TabSelected] = C(40, 41, 50);
    c[ImGuiCol_TabSelectedOverline] = C(124, 92, 255);
    c[ImGuiCol_TabDimmed] = C(26, 27, 32);
    c[ImGuiCol_TabDimmedSelected] = C(34, 35, 42);
    c[ImGuiCol_TabDimmedSelectedOverline] = C(80, 70, 140);
    c[ImGuiCol_DockingPreview] = C(124, 92, 255, 120);
    c[ImGuiCol_DockingEmptyBg] = C(18, 18, 22);
    c[ImGuiCol_PlotLines] = C(46, 196, 214);
    c[ImGuiCol_PlotHistogram] = C(124, 92, 255);
    c[ImGuiCol_TableHeaderBg] = C(34, 35, 42);
    c[ImGuiCol_TableBorderStrong] = C(48, 50, 58);
    c[ImGuiCol_TableBorderLight] = C(40, 42, 48);
    c[ImGuiCol_TextSelectedBg] = C(124, 92, 255, 90);
    c[ImGuiCol_DragDropTarget] = C(46, 196, 214);
    c[ImGuiCol_NavCursor] = C(124, 92, 255);
    c[ImGuiCol_ModalWindowDimBg] = C(0, 0, 0, 140);

    s.ScaleAllSizes(dpi);
    s.FontScaleDpi = dpi;
}

Fonts LoadFonts(float dpi) {
    ImGuiIO& io = ImGui::GetIO();
    namespace fs = std::filesystem;
    auto tryFont = [&](const char* path, float size, bool merge) -> ImFont* {
        if (!fs::exists(path)) return nullptr;
        ImFontConfig cfg;
        cfg.MergeMode = merge;
        cfg.OversampleH = 2;
        return io.Fonts->AddFontFromFileTTF(path, size, &cfg);
    };
    (void)dpi;
    g_fonts.ui = tryFont("C:/Windows/Fonts/segoeui.ttf", 16.0f, false);
    if (g_fonts.ui) tryFont("C:/Windows/Fonts/seguisym.ttf", 16.0f, true);  // arrows & symbols
    else g_fonts.ui = io.Fonts->AddFontDefault();
    g_fonts.mono = tryFont("C:/Windows/Fonts/consola.ttf", 15.0f, false);
    if (!g_fonts.mono) g_fonts.mono = g_fonts.ui;
    return g_fonts;
}

void DrawIcon(ImDrawList* dl, int icon, ImVec2 c, float s, ImU32 col) {
    const float t = std::max(1.2f, s * 0.085f);  // stroke thickness
    auto P = [&](float x, float y) { return ImVec2(c.x + x * s * 0.5f, c.y + y * s * 0.5f); };
    switch (icon) {
        case IconBrush:
            dl->AddLine(P(0.75f, -0.8f), P(-0.05f, 0.1f), col, t * 1.3f);
            dl->AddCircleFilled(P(-0.3f, 0.4f), s * 0.2f, col);
            dl->AddTriangleFilled(P(-0.45f, 0.5f), P(-0.85f, 0.85f), P(-0.15f, 0.65f), col);
            break;
        case IconEraser: {
            ImVec2 q[4] = {P(-0.2f, -0.75f), P(0.8f, 0.2f), P(0.3f, 0.75f), P(-0.75f, -0.2f)};
            dl->AddPolyline(q, 4, col, ImDrawFlags_Closed, t);
            dl->AddQuadFilled(P(-0.75f, -0.2f), P(-0.2f, -0.75f), P(0.25f, -0.3f), P(-0.3f, 0.25f), col);
            dl->AddLine(P(-0.3f, 0.85f), P(0.8f, 0.85f), col, t);
            break;
        }
        case IconPicker:
            dl->AddLine(P(-0.8f, 0.8f), P(0.2f, -0.2f), col, t * 1.2f);
            dl->AddCircleFilled(P(0.45f, -0.45f), s * 0.2f, col);
            dl->AddLine(P(0.05f, -0.75f), P(0.75f, -0.05f), col, t * 1.5f);
            break;
        case IconHand:
            for (int i = 0; i < 4; ++i) {
                float x = -0.45f + i * 0.3f;
                dl->AddLine(P(x, -0.75f + (i == 0 || i == 3 ? 0.2f : 0.0f)), P(x, 0.1f), col, t * 1.6f);
            }
            dl->AddRectFilled(P(-0.55f, 0.0f), P(0.55f, 0.75f), col, s * 0.12f);
            dl->AddLine(P(-0.55f, 0.25f), P(-0.85f, -0.1f), col, t * 1.6f);
            break;
        case IconZoom:
            dl->AddCircle(P(-0.15f, -0.15f), s * 0.3f, col, 0, t * 1.2f);
            dl->AddLine(P(0.3f, 0.3f), P(0.8f, 0.8f), col, t * 1.8f);
            break;
        case IconGradient: {
            ImVec2 a = P(-0.8f, -0.6f), b = P(0.8f, 0.6f);
            dl->AddRectFilledMultiColor(a, b, IM_COL32(20, 20, 24, 255), col, col, IM_COL32(20, 20, 24, 255));
            dl->AddRect(a, b, col, 2, 0, t * 0.8f);
            break;
        }
        case IconBucket: {
            ImVec2 q[4] = {P(-0.6f, -0.2f), P(0.0f, -0.8f), P(0.6f, -0.2f), P(0.0f, 0.4f)};
            dl->AddPolyline(q, 4, col, ImDrawFlags_Closed, t * 1.2f);
            dl->AddTriangleFilled(P(-0.6f, -0.2f), P(0.6f, -0.2f), P(0.0f, 0.4f), col);
            dl->AddCircleFilled(P(0.75f, 0.55f), s * 0.12f, col);
            break;
        }
        case IconEye:
        case IconEyeOff: {
            ImVec2 pts[24];
            for (int i = 0; i < 12; ++i) {
                float a = 3.14159f * i / 11;
                pts[i] = P(-std::cos(a) * 0.85f, -std::sin(a) * 0.5f);
                pts[12 + i] = P(std::cos(a) * 0.85f, std::sin(a) * 0.5f);
            }
            dl->AddPolyline(pts, 24, col, ImDrawFlags_Closed, t);
            if (icon == IconEye) dl->AddCircleFilled(c, s * 0.18f, col);
            else dl->AddLine(P(-0.8f, 0.7f), P(0.8f, -0.7f), col, t * 1.3f);
            break;
        }
        case IconPlus:
            dl->AddLine(P(0, -0.7f), P(0, 0.7f), col, t * 1.4f);
            dl->AddLine(P(-0.7f, 0), P(0.7f, 0), col, t * 1.4f);
            break;
        case IconTrash:
            dl->AddRect(P(-0.5f, -0.45f), P(0.5f, 0.85f), col, 2, 0, t);
            dl->AddLine(P(-0.75f, -0.55f), P(0.75f, -0.55f), col, t * 1.2f);
            dl->AddLine(P(-0.2f, -0.8f), P(0.2f, -0.8f), col, t * 1.2f);
            dl->AddLine(P(0, -0.2f), P(0, 0.6f), col, t);
            break;
        case IconUp:
            dl->AddTriangleFilled(P(0, -0.7f), P(0.7f, 0.4f), P(-0.7f, 0.4f), col);
            break;
        case IconDown:
            dl->AddTriangleFilled(P(-0.7f, -0.4f), P(0.7f, -0.4f), P(0, 0.7f), col);
            break;
        case IconMask:
            dl->AddRect(P(-0.8f, -0.7f), P(0.8f, 0.7f), col, 2, 0, t);
            dl->AddCircleFilled(c, s * 0.28f, col);
            break;
        case IconCopy:
            dl->AddRect(P(-0.75f, -0.75f), P(0.35f, 0.35f), col, 2, 0, t);
            dl->AddRect(P(-0.35f, -0.35f), P(0.75f, 0.75f), col, 2, 0, t);
            break;
        case IconLock:
            dl->AddRectFilled(P(-0.6f, -0.05f), P(0.6f, 0.8f), col, 2);
            dl->PathArcTo(P(0, -0.15f), s * 0.22f, 3.14159f, 6.28318f);
            dl->PathStroke(col, 0, t * 1.2f);
            break;
        case IconFx:
            dl->AddText(ImVec2(c.x - s * 0.38f, c.y - ImGui::GetFontSize() * 0.5f), col, "fx");
            break;
        case IconMove: {
            dl->AddLine(P(-0.85f, 0), P(0.85f, 0), col, t);
            dl->AddLine(P(0, -0.85f), P(0, 0.85f), col, t);
            const float a = 0.3f;
            dl->AddTriangleFilled(P(0.85f, 0), P(0.85f - a, -a), P(0.85f - a, a), col);
            dl->AddTriangleFilled(P(-0.85f, 0), P(-0.85f + a, a), P(-0.85f + a, -a), col);
            dl->AddTriangleFilled(P(0, -0.85f), P(a, -0.85f + a), P(-a, -0.85f + a), col);
            dl->AddTriangleFilled(P(0, 0.85f), P(-a, 0.85f - a), P(a, 0.85f - a), col);
            break;
        }
        case IconRectSel:
        case IconEllipseSel: {
            // dashed outline
            const int n = 16;
            ImVec2 pts[n + 1];
            for (int i = 0; i <= n; ++i) {
                float a = 6.28318f * i / n;
                if (icon == IconEllipseSel) pts[i] = P(std::cos(a) * 0.8f, std::sin(a) * 0.65f);
                else {
                    float u = (float)i / n * 4.0f;
                    int side = std::min((int)u, 3);
                    float f = u - side;
                    ImVec2 c4[5] = {P(-0.8f, -0.65f), P(0.8f, -0.65f), P(0.8f, 0.65f), P(-0.8f, 0.65f), P(-0.8f, -0.65f)};
                    pts[i] = ImVec2(c4[side].x + (c4[side + 1].x - c4[side].x) * f, c4[side].y + (c4[side + 1].y - c4[side].y) * f);
                }
            }
            for (int i = 0; i < n; i += 2) dl->AddLine(pts[i], pts[i + 1], col, t);
            break;
        }
        case IconLasso: {
            ImVec2 pts[14];
            for (int i = 0; i < 14; ++i) {
                float a = 6.28318f * i / 13;
                float r = 0.75f + 0.12f * std::sin(a * 3);
                pts[i] = P(std::cos(a) * r, std::sin(a) * r * 0.6f - 0.15f);
            }
            dl->AddPolyline(pts, 14, col, 0, t);
            dl->AddBezierQuadratic(P(-0.3f, 0.4f), P(-0.5f, 0.9f), P(0.1f, 0.85f), col, t);
            break;
        }
        case IconPolyLasso: {
            ImVec2 pts[6] = {P(-0.8f, -0.2f), P(-0.2f, -0.8f), P(0.75f, -0.5f), P(0.5f, 0.3f), P(-0.1f, 0.75f), P(-0.8f, -0.2f)};
            dl->AddPolyline(pts, 6, col, 0, t);
            for (auto& p : pts) dl->AddCircleFilled(p, s * 0.07f, col);
            break;
        }
        case IconWand:
            dl->AddLine(P(-0.8f, 0.8f), P(0.25f, -0.25f), col, t * 1.6f);
            for (int i = 0; i < 4; ++i) {
                float a = 1.5708f * i + 0.785f;
                dl->AddLine(P(0.5f, -0.5f), P(0.5f + std::cos(a) * 0.38f, -0.5f + std::sin(a) * 0.38f), col, t);
            }
            break;
        case IconSwap:
            dl->AddBezierQuadratic(P(-0.6f, 0.4f), P(-0.6f, -0.6f), P(0.4f, -0.6f), col, t);
            dl->AddTriangleFilled(P(0.3f, -0.85f), P(0.75f, -0.6f), P(0.3f, -0.35f), col);
            dl->AddTriangleFilled(P(-0.85f, 0.3f), P(-0.6f, 0.75f), P(-0.35f, 0.3f), col);
            break;
    }
}

bool IconButton(const char* id, int icon, bool selected, float size, const char* tooltip) {
    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##ib", ImVec2(size, size));
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = selected ? col::Accent : hovered ? IM_COL32(255, 255, 255, 22) : IM_COL32(0, 0, 0, 0);
    dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), bg, 5.0f);
    DrawIcon(dl, icon, ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), size * 0.55f, selected ? IM_COL32_WHITE : (hovered ? col::Text : col::TextDim));
    if (tooltip && hovered) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();
    return pressed;
}

void HelpTooltip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", text);
}

}  // namespace sx::ui
