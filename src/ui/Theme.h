#pragma once
#include <imgui.h>

namespace sx::ui {

struct Fonts {
    ImFont* ui = nullptr;
    ImFont* mono = nullptr;
};

void ApplyTheme(float dpiScale);
Fonts LoadFonts(float dpiScale);
Fonts& GetFonts();

// Palette shared by custom-drawn widgets (node editor, toolbar icons...).
namespace col {
constexpr ImU32 Accent = IM_COL32(124, 92, 255, 255);
constexpr ImU32 AccentSoft = IM_COL32(124, 92, 255, 90);
constexpr ImU32 Accent2 = IM_COL32(46, 196, 214, 255);
constexpr ImU32 Selection = IM_COL32(255, 140, 0, 255);  // selected graph nodes / links
constexpr ImU32 Text = IM_COL32(222, 224, 232, 255);
constexpr ImU32 TextDim = IM_COL32(140, 144, 158, 255);
constexpr ImU32 Panel = IM_COL32(30, 31, 36, 255);
constexpr ImU32 PanelLight = IM_COL32(42, 44, 51, 255);
constexpr ImU32 Border = IM_COL32(58, 60, 70, 255);
constexpr ImU32 Grid = IM_COL32(255, 255, 255, 10);
constexpr ImU32 GridMajor = IM_COL32(255, 255, 255, 22);
constexpr ImU32 Error = IM_COL32(240, 80, 90, 255);
}  // namespace col

// Small helpers
bool IconButton(const char* id, int icon, bool selected, float size, const char* tooltip);
void DrawIcon(ImDrawList* dl, int icon, ImVec2 center, float size, ImU32 color);
void HelpTooltip(const char* text);

enum Icon { IconBrush, IconEraser, IconPicker, IconHand, IconZoom, IconGradient, IconBucket, IconEye, IconEyeOff,
            IconPlus, IconTrash, IconUp, IconDown, IconMask, IconCopy, IconLock, IconFx, IconSwap,
            IconMove, IconRectSel, IconEllipseSel, IconLasso, IconPolyLasso, IconWand,
            IconStamp, IconHeal, IconDrop, IconSharpen, IconFinger, IconDodge, IconBurn, IconSponge, IconFolder, IconCrop, IconText, IconShape, IconLiquify, IconQuickSelect, IconPen, IconTransform, IconTrim, IconParticles, IconPolyFill, IconFluid };

}  // namespace sx::ui
