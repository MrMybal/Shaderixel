#pragma once
// Image alignment on the CPU: Harris corners, normalized patch descriptors,
// mutual best matches with a ratio test, RANSAC + least squares for a
// translation, similarity or homography. Used by auto-align, panoramas and
// HDR merging. All coordinates are full resolution pixels.
#include "core/Common.h"

#include <array>
#include <vector>

namespace sx::align {

using Mat3 = std::array<double, 9>;  // row-major, maps image -> reference
inline Mat3 Identity() { return {1, 0, 0, 0, 1, 0, 0, 0, 1}; }
Mat3 Mul(const Mat3& a, const Mat3& b);
bool Invert(const Mat3& m, Mat3& out);
float2 Apply(const Mat3& m, float2 p);

struct Gray {
    int w = 0, h = 0;
    float scale = 1;          // full resolution pixels per gray pixel
    std::vector<float> px;    // 0..1 luminance (gamma encoded)
};
// From 8-bit RGBA (straight alpha); downsampled so the longest side is <= maxDim.
Gray ToGray(const u8* rgba, int w, int h, int maxDim = 1200);

struct Feature {
    float x = 0, y = 0;       // full resolution
    std::array<float, 64> d;  // zero-mean unit descriptor
};
std::vector<Feature> Detect(const Gray& g, int maxCount = 1500);

enum class Model { Translation, Similarity, Homography };
struct Result {
    bool ok = false;
    Mat3 m = Identity();      // image b -> image a
    int matches = 0, inliers = 0;
};
// Transform of b onto a (warp b with m to overlay a).
Result Estimate(const std::vector<Feature>& a, const std::vector<Feature>& b, Model model, float threshold = 3.0f);

// Cylindrical projection with focal f (pixels) around the image center (cx, cy), and its inverse.
float2 ToCylinder(float2 p, float f, float cx, float cy);
float2 FromCylinder(float2 q, float f, float cx, float cy);

}  // namespace sx::align
