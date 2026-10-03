#pragma once
// Readers for Adobe resource files: brushes (.abr, sampled tips of versions 1, 2
// and 6 / 10), patterns (.pat) and swatches (.aco, .ase), plus GIMP palettes (.gpl).
#include "core/Common.h"

#include <string>
#include <vector>

namespace sx {

struct AbrTip {
    std::string name;
    int w = 0, h = 0;
    std::vector<u8> gray;   // 0 = no paint, 255 = full
    float spacing = 0.25f;  // fraction of the diameter
};
bool ReadAbr(const std::string& path, std::vector<AbrTip>& tips, std::string* error);

struct PatternImage {
    std::string name;
    int w = 0, h = 0;
    std::vector<u8> rgba;   // straight, sRGB
};
bool ReadPat(const std::string& path, std::vector<PatternImage>& out, std::string* error);

struct Swatch {
    std::string name;
    float4 srgb = {0, 0, 0, 1};
};
bool ReadSwatches(const std::string& path, std::vector<Swatch>& out, std::string* error);  // .aco .ase .gpl
bool WriteAco(const std::string& path, const std::vector<Swatch>& swatches);

}  // namespace sx
