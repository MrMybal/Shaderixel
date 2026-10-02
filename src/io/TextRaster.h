#pragma once
// Text layer rasterization with stb_truetype (system fonts).

#include "core/Common.h"

namespace sx {

struct TextStyle {
    std::string content = "Texte";
    std::string font;            // font file path ("" = default)
    float size = 72.0f;          // pixels (em height)
    float4 color = {0, 0, 0, 1}; // sRGB straight
    float2 pos;                  // anchor: left/center/right of the first line's top
    int align = 0;               // 0 left, 1 center, 2 right
    float lineSpacing = 1.2f;
    float tracking = 0.0f;       // extra advance in 1/1000 em
    bool faux = false;           // faux bold (dilate)
    u64 Hash() const;
};

struct FontEntry {
    std::string name;
    std::string path;
};
const std::vector<FontEntry>& SystemFonts();
std::string DefaultFontPath();

// Rasterize into straight-alpha linear RGBA float pixels covering `rect` (document space).
bool RasterizeText(const TextStyle& t, int docW, int docH, std::vector<float>& rgba, Rect& rect);

}  // namespace sx
