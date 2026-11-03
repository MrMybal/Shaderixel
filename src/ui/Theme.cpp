#include "ui/Theme.h"

#include <cmath>
#include <filesystem>

namespace sx::ui {

namespace {
const char* SharedIcon(int icon) {
    switch(icon) {
        case IconPlus:return cyui::icon::add;
        case IconTrash:return cyui::icon::trash;
        case IconCopy:return cyui::icon::copy;
        case IconFolder:return cyui::icon::folder;
        case IconLink:return cyui::icon::link;
        case IconLock:return cyui::icon::lock;
        case IconUp:return cyui::icon::chevronUp;
        case IconDown:return cyui::icon::chevronDown;
        case IconZoom:return cyui::icon::zoomIn;
        case IconCrop:return cyui::icon::crop;
        case IconTrim:return cyui::icon::trim;
        case IconText:return cyui::icon::document;
        default:return nullptr;
    }
}
}

Fonts& GetFonts() {
    // A borrowed view of the fonts owned by the current CyUI/ImGui context.
    static thread_local Fonts fonts;
    fonts = {cyui::uiFont(), cyui::monoFont()};
    return fonts;
}

void ApplyTheme(float dpi, const char* theme, bool comfortable) {
    cyui::Config config;
    config.themeKey = theme;
    config.density = comfortable ? cyui::Density::comfortable : cyui::Density::compact;
    config.dpiScale = dpi;
    cyui::apply(config);
}

Fonts LoadFonts(float) {
    cyui::loadFonts();
    return GetFonts();
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
        case IconTrim: {
            // Sheet cut into strips of different heights.
            dl->AddRect(P(-0.7f, -0.7f), P(0.7f, 0.7f), col, 0, 0, t);
            for (float y : {-0.25f, 0.1f, 0.35f, 0.52f}) dl->AddLine(P(-0.7f, y), P(0.7f, y), col, t);
            dl->AddLine(P(0.0f, 0.52f), P(0.0f, 0.7f), col, t);
            break;
        }
        case IconParticles: {
            // Drops running down a surface, with their trails.
            dl->AddLine(P(-0.75f, 0.75f), P(0.75f, 0.75f), col, t);
            for (float x : {-0.45f, 0.05f, 0.5f}) {
                const float y = x == 0.05f ? 0.25f : -0.05f;
                dl->AddLine(P(x, -0.7f), P(x, y), col, t * 0.8f);
                dl->AddCircleFilled(P(x, y + 0.16f), 0.15f * (P(1, 0).x - P(0, 0).x), col);
            }
            break;
        }
        case IconLink: {
            // Two chain links, slanted.
            const float k = P(1, 0).x - P(0, 0).x;
            dl->AddRect(P(-0.95f, -0.42f), P(0.2f, 0.42f), col, 0.42f * k, 0, t * 1.7f);
            dl->AddRect(P(-0.2f, -0.42f), P(0.95f, 0.42f), col, 0.42f * k, 0, t * 1.7f);
            break;
        }
        case IconFluid: {
            // A stroke of paint at the top, its runs dripping down (different lengths).
            const float k = P(1, 0).x - P(0, 0).x;
            dl->AddRectFilled(P(-0.8f, -0.85f), P(0.8f, -0.45f), col, 0.18f * k);
            const float xs[3] = {-0.5f, 0.0f, 0.48f}, ys[3] = {0.35f, 0.7f, 0.05f};
            for (int i = 0; i < 3; ++i) {
                dl->AddLine(P(xs[i], -0.5f), P(xs[i], ys[i]), col, t * 2.2f);
                dl->AddCircleFilled(P(xs[i], ys[i]), 0.17f * k, col);
            }
            break;
        }
        case IconPolyFill: {
            // Mesh triangles, one of them filled.
            dl->AddTriangleFilled(P(-0.7f, 0.6f), P(0.0f, -0.6f), P(0.7f, 0.6f), (col & 0x00FFFFFF) | 0x50000000);
            dl->AddTriangleFilled(P(-0.35f, 0.0f), P(0.35f, 0.0f), P(0.0f, 0.6f), col);
            dl->AddTriangle(P(-0.7f, 0.6f), P(0.0f, -0.6f), P(0.7f, 0.6f), col, t);
            dl->AddLine(P(-0.35f, 0.0f), P(0.35f, 0.0f), col, t);
            dl->AddLine(P(-0.35f, 0.0f), P(0.0f, 0.6f), col, t);
            dl->AddLine(P(0.35f, 0.0f), P(0.0f, 0.6f), col, t);
            break;
        }
        case IconTransform: {
            // Bounding box with corner handles and a rotation arc.
            dl->AddRect(P(-0.6f, -0.6f), P(0.6f, 0.6f), col, 0, 0, t);
            for (float x : {-0.6f, 0.6f})
                for (float y : {-0.6f, 0.6f}) dl->AddRectFilled(P(x - 0.16f, y - 0.16f), P(x + 0.16f, y + 0.16f), col);
            dl->PathArcTo(P(0, 0), 0.32f * (P(1, 0).x - P(0, 0).x), 3.6f, 5.9f, 12);
            dl->PathStroke(col, 0, t);
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
        case IconStamp:
            dl->AddRectFilled(P(-0.75f, 0.45f), P(0.75f, 0.8f), col, 2);
            dl->AddRectFilled(P(-0.2f, -0.2f), P(0.2f, 0.45f), col);
            dl->AddCircleFilled(P(0, -0.5f), s * 0.18f, col);
            break;
        case IconHeal: {
            // diagonal bandage
            ImVec2 q[4] = {P(-0.85f, 0.35f), P(0.35f, -0.85f), P(0.85f, -0.35f), P(-0.35f, 0.85f)};
            dl->AddPolyline(q, 4, col, ImDrawFlags_Closed, t);
            dl->AddQuadFilled(P(-0.25f, -0.25f), P(0.0f, -0.5f), P(0.5f, 0.0f), P(0.25f, 0.25f), col);
            break;
        }
        case IconDrop:
            dl->PathArcTo(P(0, 0.25f), s * 0.28f, 0.0f - 0.35f, 3.14159f + 0.35f);
            dl->PathLineTo(P(0, -0.85f));
            dl->PathFillConvex(col);
            break;
        case IconSharpen:
            dl->AddTriangleFilled(P(0, -0.85f), P(0.6f, 0.8f), P(-0.6f, 0.8f), col);
            break;
        case IconFinger:
            dl->AddLine(P(0.2f, -0.85f), P(0.2f, 0.2f), col, t * 2.4f);
            dl->AddCircleFilled(P(0.05f, 0.45f), s * 0.24f, col);
            dl->AddLine(P(-0.6f, 0.1f), P(-0.25f, 0.45f), col, t * 2.0f);
            break;
        case IconDodge:
            dl->AddCircle(P(0.2f, -0.35f), s * 0.25f, col, 0, t * 1.2f);
            dl->AddLine(P(0.0f, -0.15f), P(-0.7f, 0.85f), col, t * 1.4f);
            break;
        case IconBurn:
            dl->AddCircleFilled(P(-0.05f, 0.1f), s * 0.38f, col);
            dl->AddCircleFilled(P(0.18f, 0.1f), s * 0.16f, IM_COL32(30, 31, 36, 255));
            dl->AddLine(P(-0.6f, -0.2f), P(-0.6f, -0.8f), col, t * 1.6f);
            break;
        case IconSponge:
            dl->AddRectFilled(P(-0.75f, -0.5f), P(0.75f, 0.6f), col, s * 0.12f);
            for (int i = 0; i < 3; ++i) dl->AddCircleFilled(P(-0.4f + i * 0.4f, 0.05f + (i % 2) * 0.2f - 0.1f), s * 0.08f, IM_COL32(30, 31, 36, 255));
            break;
        case IconPen: {
            // Fountain pen nib.
            ImVec2 q[5] = {P(0.0f, -0.85f), P(0.5f, 0.05f), P(0.25f, 0.6f), P(-0.25f, 0.6f), P(-0.5f, 0.05f)};
            dl->AddConvexPolyFilled(q, 5, col);
            dl->AddLine(P(0.0f, -0.6f), P(0.0f, 0.15f), IM_COL32(30, 31, 36, 255), t * 1.2f);
            dl->AddCircleFilled(P(0.0f, 0.15f), s * 0.09f, IM_COL32(30, 31, 36, 255));
            dl->AddRectFilled(P(-0.3f, 0.68f), P(0.3f, 0.9f), col, s * 0.04f);
            break;
        }
        case IconQuickSelect: {
            // Dashed circle (selection) with a brush tip.
            for (int i = 0; i < 8; ++i) {
                float a0 = i * 0.785f, a1 = a0 + 0.45f;
                dl->AddLine(P(-0.2f + std::cos(a0) * 0.55f, -0.2f + std::sin(a0) * 0.55f), P(-0.2f + std::cos(a1) * 0.55f, -0.2f + std::sin(a1) * 0.55f), col, t * 1.2f);
            }
            dl->AddLine(P(0.25f, 0.25f), P(0.85f, 0.85f), col, t * 2.2f);
            break;
        }
        case IconLiquify: {
            // Spiral.
            dl->PathClear();
            for (int i = 0; i <= 28; ++i) {
                float a = i * 0.42f, r = 0.08f + i * 0.028f;
                dl->PathLineTo(P(std::cos(a) * r, std::sin(a) * r));
            }
            dl->PathStroke(col, 0, t * 1.5f);
            break;
        }
        case IconShape:
            dl->AddRectFilled(P(-0.85f, -0.2f), P(0.15f, 0.8f), col, 2);
            dl->AddCircle(P(0.35f, -0.35f), s * 0.25f, col, 0, t * 1.2f);
            break;
        case IconText:
            dl->AddLine(P(-0.7f, -0.75f), P(0.7f, -0.75f), col, t * 1.8f);
            dl->AddLine(P(0, -0.75f), P(0, 0.8f), col, t * 1.8f);
            dl->AddLine(P(-0.3f, 0.8f), P(0.3f, 0.8f), col, t * 1.2f);
            break;
        case IconCrop:
            dl->AddLine(P(-0.5f, -0.9f), P(-0.5f, 0.5f), col, t * 1.4f);
            dl->AddLine(P(-0.5f, 0.5f), P(0.9f, 0.5f), col, t * 1.4f);
            dl->AddLine(P(-0.9f, -0.5f), P(0.5f, -0.5f), col, t * 1.4f);
            dl->AddLine(P(0.5f, -0.5f), P(0.5f, 0.9f), col, t * 1.4f);
            break;
        case IconFolder:
            dl->AddRectFilled(P(-0.85f, -0.6f), P(-0.15f, -0.35f), col, s * 0.06f);
            dl->AddRectFilled(P(-0.85f, -0.4f), P(0.85f, 0.65f), col, s * 0.08f);
            break;
        case IconSwap:
            dl->AddBezierQuadratic(P(-0.6f, 0.4f), P(-0.6f, -0.6f), P(0.4f, -0.6f), col, t);
            dl->AddTriangleFilled(P(0.3f, -0.85f), P(0.75f, -0.6f), P(0.3f, -0.35f), col);
            dl->AddTriangleFilled(P(-0.85f, 0.3f), P(-0.6f, 0.75f), P(-0.35f, 0.3f), col);
            break;
    }
}

bool IconButton(const char* id, int icon, bool selected, float size, const char* tooltip) {
    cyui::ScopedID scope(id);
    if (const auto* glyph=SharedIcon(icon))
        return cyui::iconButton(glyph, tooltip ? tooltip : id, selected, 0, ImVec2(size,size));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = cyui::button("##ib", selected ? cyui::Variant::primary : cyui::Variant::ghost, ImVec2(size, size));
    const auto& p = cyui::palette();
    DrawIcon(ImGui::GetWindowDrawList(), icon, ImVec2(pos.x + size * .5f, pos.y + size * .5f),
             size * .55f, selected ? p.accentText : ImGui::IsItemHovered() ? p.text : p.textDim);
    if (tooltip) cyui::tooltip(tooltip);
    return pressed;
}

void HelpTooltip(const char* text) { cyui::tooltip(text); }

}  // namespace sx::ui
