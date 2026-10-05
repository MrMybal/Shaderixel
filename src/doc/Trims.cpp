// Trim sheet layouts (see TrimSheet in Document.h).
#include "doc/Document.h"

#include <cmath>

namespace sx {

std::vector<Trim> TrimStrips(u32 width, u32 height, const std::vector<int>& heights, int padding) {
    std::vector<Trim> out;
    int y = 0;
    const int pad = std::max(padding, 0);
    for (size_t i = 0; i < heights.size() && y < (int)height; ++i) {
        int h = heights[i] > 0 ? heights[i] : (int)height - y;
        h = std::min(h, (int)height - y);
        if (h <= 0) break;
        Trim t;
        t.name = "Bande " + std::to_string(i + 1);
        t.r = {0, y, (int)width, y + h};
        t.tileX = true;
        out.push_back(t);
        y += h + pad;
    }
    return out;
}

const std::vector<TrimPreset>& TrimPresets() {
    static const std::vector<TrimPreset> presets = {
        {"Architecture (larges + fines)", {1 / 4.f, 1 / 8.f, 1 / 8.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 32.f, 1 / 32.f, 1 / 32.f, 1 / 32.f}},
        {"Détails (beaucoup de fines)", {1 / 8.f, 1 / 8.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 32.f, 1 / 32.f, 1 / 32.f, 1 / 32.f,
                                         1 / 32.f, 1 / 32.f, 1 / 32.f, 1 / 32.f, 1 / 16.f, 1 / 16.f}},
        {"Bandes égales x4", {1 / 4.f, 1 / 4.f, 1 / 4.f, 1 / 4.f}},
        {"Bandes égales x8", {1 / 8.f, 1 / 8.f, 1 / 8.f, 1 / 8.f, 1 / 8.f, 1 / 8.f, 1 / 8.f, 1 / 8.f}},
        {"Bandes égales x16", std::vector<float>(16, 1 / 16.f)},
        {"Moitié bandes, moitié tuiles", {1 / 8.f, 1 / 8.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 1 / 16.f, 0.5f}},
    };
    return presets;
}

int TrimAt(const TrimSheet& t, float2 p) {
    for (int i = (int)t.trims.size() - 1; i >= 0; --i) {
        const Rect& r = t.trims[i].r;
        if (p.x >= r.x0 && p.y >= r.y0 && p.x < r.x1 && p.y < r.y1) return i;
    }
    return -1;
}

}  // namespace sx
