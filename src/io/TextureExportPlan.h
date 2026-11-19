#pragma once
#include "core/Common.h"
#include <array>

namespace sx::texexport {

struct Map {
    std::string suffix;
    std::array<std::string, 4> src;
};
struct Preset {
    std::string name, desc;
    bool opengl = false;
    std::vector<Map> maps;
};
struct Output {
    int part = 0, map = 0;
    std::string filename;
};

// A set suffix is empty for an atlas, "_Body", ".1001" or "_Body.1001".
std::pair<std::string, std::string> SetSuffixes(const std::string& suffix);
std::string PlanOutputs(const std::vector<std::pair<std::string, Rect>>& parts, const std::vector<Map>& maps,
                        const std::string& pattern, const std::string& base, const std::string& ext,
                        std::vector<Output>& outputs, const std::vector<std::vector<bool>>& enabledMaps = {});
std::string ValidateSetNames(const std::vector<std::pair<std::string, Rect>>& parts, const std::string& base);

struct PaddingPixel { int index; u8 neighbors; };
// Build the expansion rings once per UV tile. Every map uses the same neighbors and island boundaries.
std::vector<PaddingPixel> BuildPadding(std::vector<u8> covered, int w, int h, int radius);
void ApplyPadding(const std::vector<PaddingPixel>& padding, int width, std::vector<float>& rgba);

}  // namespace sx::texexport
