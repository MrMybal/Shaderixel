#pragma once
// Text layer rasterization with stb_truetype (system fonts): character runs,
// paragraph styles, text on a path.

#include "core/Common.h"

namespace sx {

// Named character style (Styles de caractère): applied to ranges of a text layer.
struct CharStyle {
    Id id = 0;
    std::string name = "Style de caractère";
    std::string font;            // "" = keep the layer's font
    float size = 0;              // 0 = keep the layer's size
    float4 color = {0, 0, 0, 1};
    bool useColor = true;
    float tracking = 0;          // 1/1000 em
    bool faux = false;           // faux bold
    float baseline = 0;          // baseline shift (px, up)
    bool underline = false;
    bool allCaps = false;
    u64 Hash() const;
};

// Named paragraph style (Styles de paragraphe): applied to whole paragraphs.
struct ParaStyle {
    Id id = 0;
    std::string name = "Style de paragraphe";
    int align = 0;               // 0 left, 1 center, 2 right
    float lineSpacing = 1.2f;
    float spaceBefore = 0;       // px
    float indentLeft = 0;        // px
    float indentFirst = 0;       // px, first line of the paragraph
    Id charStyle = 0;            // base character style of the paragraph (0 = layer)
    u64 Hash() const;
};

struct TextRun {
    int start = 0, end = 0;      // codepoint range [start, end)
    Id style = 0;                // character style
};

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
    std::vector<TextRun> runs;   // character styles on ranges (later runs win)
    std::vector<Id> paraStyles;  // paragraph style per paragraph (0 = none)
    // Text on a path: the baseline follows this polyline (document pixels), from pathOffset.
    std::vector<float2> path;
    float pathOffset = 0;
    bool pathFlip = false;       // other side of the path
    u64 Hash() const;
};

struct TextStyleTable {
    const std::vector<CharStyle>* chars = nullptr;
    const std::vector<ParaStyle>* paras = nullptr;
    const CharStyle* Char(Id id) const;
    const ParaStyle* Para(Id id) const;
    u64 Hash() const;
};

struct FontEntry {
    std::string name;
    std::string path;
};
const std::vector<FontEntry>& SystemFonts();
std::string DefaultFontPath();

// Rasterize into straight-alpha linear RGBA float pixels covering `rect` (document space).
bool RasterizeText(const TextStyle& t, int docW, int docH, std::vector<float>& rgba, Rect& rect, const TextStyleTable* styles = nullptr);

// Codepoint count of a UTF-8 string and conversion of a byte offset to a codepoint index.
int Utf8Length(const std::string& s);
int Utf8CodepointIndex(const std::string& s, int byteOffset);

}  // namespace sx
