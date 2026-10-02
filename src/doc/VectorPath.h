#pragma once
// Vector paths (Pen tool): cubic Bézier subpaths stored in the document.
// Handles are absolute positions; a corner anchor has in == out == p.

#include "core/Common.h"

namespace sx {

struct PathAnchor {
    float2 p, in, out;
};

struct SubPath {
    std::vector<PathAnchor> pts;
    bool closed = false;
};

struct VectorPath {
    Id id = 0;
    std::string name;
    std::vector<SubPath> subpaths;
    bool Empty() const;
};

// Polylines approximating each subpath (open subpaths are returned as-is; callers
// that fill close them implicitly). `tolerance` is the max deviation in pixels.
std::vector<std::vector<float2>> FlattenPath(const VectorPath& path, float tolerance = 0.25f);

// Even-odd scanline coverage of several closed contours (4x vertical supersampling,
// exact horizontal coverage). Returns the covered rectangle and its coverage.
void RasterizeContours(const std::vector<std::vector<float2>>& contours, int W, int H, bool aa, Rect& r, std::vector<u8>& cov);

}  // namespace sx
