#pragma once
#include <cyui/Core.h>

namespace sx::ui {

struct Fonts {
    ImFont* ui = nullptr;
    ImFont* mono = nullptr;
};

void ApplyTheme(float dpiScale, const char* theme = "violet", bool comfortable = true);
Fonts LoadFonts(float dpiScale);
Fonts& GetFonts();

// Small helpers
bool IconButton(const char* id, int icon, bool selected, float size, const char* tooltip);
void DrawIcon(ImDrawList* dl, int icon, ImVec2 center, float size, ImU32 color);
void HelpTooltip(const char* text);

enum Icon { IconBrush, IconEraser, IconPicker, IconHand, IconZoom, IconGradient, IconBucket, IconEye, IconEyeOff,
            IconPlus, IconTrash, IconUp, IconDown, IconMask, IconCopy, IconLock, IconFx, IconSwap,
            IconMove, IconRectSel, IconEllipseSel, IconLasso, IconPolyLasso, IconWand,
            IconStamp, IconHeal, IconDrop, IconSharpen, IconFinger, IconDodge, IconBurn, IconSponge, IconFolder, IconCrop, IconText, IconShape, IconLiquify, IconQuickSelect, IconPen, IconTransform, IconTrim, IconParticles, IconPolyFill, IconFluid, IconLink };

}  // namespace sx::ui
