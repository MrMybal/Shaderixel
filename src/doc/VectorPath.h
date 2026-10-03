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

// Outline of a coverage mask (R8, 50% threshold) as closed Bezier subpaths:
// sub-pixel contour, simplified to `tolerance` pixels, smooth points get
// tangent handles and sharp turns stay corners (Photoshop "Make Work Path").
std::vector<SubPath> TraceMask(const u8* mask, int W, int H, float tolerance);

// Photoshop path resource data (image resources 2000-2997): 26-byte records,
// coordinates as 8.24 fixed-point fractions of the image size.
std::vector<u8> EncodePsdPath(const VectorPath& path, int W, int H);
bool DecodePsdPath(const u8* data, size_t size, int W, int H, VectorPath& path);

}  // namespace sx
