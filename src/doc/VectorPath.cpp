#include "doc/VectorPath.h"

#include <algorithm>
#include <cmath>

namespace sx {

bool VectorPath::Empty() const {
    for (auto& s : subpaths) if (!s.pts.empty()) return false;
    return true;
}

namespace {
void FlattenCubic(float2 a, float2 b, float2 c, float2 d, float tol, std::vector<float2>& out) {
    // Uniform subdivision sized from the control polygon length.
    float len = Length(b - a) + Length(c - b) + Length(d - c);
    int n = std::clamp((int)std::ceil(std::sqrt(len / std::max(tol, 0.01f)) * 0.75f), 1, 512);
    if (Length(b - a) < 1e-4f && Length(d - c) < 1e-4f) n = 1;  // straight segment
    for (int i = 1; i <= n; ++i) {
        float t = (float)i / n, u = 1 - t;
        out.push_back(a * (u * u * u) + b * (3 * u * u * t) + c * (3 * u * t * t) + d * (t * t * t));
    }
}
}  // namespace

std::vector<std::vector<float2>> FlattenPath(const VectorPath& path, float tolerance) {
    std::vector<std::vector<float2>> out;
    for (const SubPath& s : path.subpaths) {
        if (s.pts.empty()) continue;
        std::vector<float2> poly{s.pts[0].p};
        size_t n = s.pts.size();
        size_t segs = s.closed ? n : n - 1;
        for (size_t i = 0; i < segs; ++i) {
            const PathAnchor& a = s.pts[i];
            const PathAnchor& b = s.pts[(i + 1) % n];
            FlattenCubic(a.p, a.out, b.in, b.p, tolerance, poly);
        }
        if (s.closed && poly.size() > 1) poly.pop_back();  // last point == first
        out.push_back(std::move(poly));
    }
    return out;
}

void RasterizeContours(const std::vector<std::vector<float2>>& contours, int W, int H, bool aa, Rect& r, std::vector<u8>& cov) {
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (auto& c : contours)
        for (auto& p : c) {
            minx = std::min(minx, p.x); miny = std::min(miny, p.y);
            maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y);
        }
    cov.clear();
    r = {};
    if (minx > maxx) return;
    r = Rect{(int)std::floor(minx), (int)std::floor(miny), (int)std::ceil(maxx) + 1, (int)std::ceil(maxy) + 1}.Clipped(W, H);
    if (r.Empty()) return;
    const int w = r.Width(), h = r.Height();
    const int SS = aa ? 4 : 1;
    std::vector<float> acc((size_t)w);
    cov.resize((size_t)w * h);
    std::vector<float> xs;
    for (int y = 0; y < h; ++y) {
        std::fill(acc.begin(), acc.end(), 0.0f);
        for (int s = 0; s < SS; ++s) {
            float sy = r.y0 + y + (s + 0.5f) / SS;
            xs.clear();
            for (auto& c : contours) {
                if (c.size() < 3) continue;
                for (size_t i = 0; i < c.size(); ++i) {
                    float2 a = c[i], b = c[(i + 1) % c.size()];
                    if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) xs.push_back(a.x + (sy - a.y) * (b.x - a.x) / (b.y - a.y));
                }
            }
            std::sort(xs.begin(), xs.end());
            for (size_t k = 0; k + 1 < xs.size(); k += 2) {
                float xa = std::max(xs[k] - r.x0, 0.0f), xb = std::min(xs[k + 1] - r.x0, (float)w);
                if (xb <= xa) continue;
                if (!aa) { xa = std::round(xa); xb = std::round(xb); }
                for (int px = (int)std::floor(xa); px < (int)std::ceil(xb) && px < w; ++px)
                    acc[px] += (std::min(xb, px + 1.0f) - std::max(xa, (float)px)) / SS;
            }
        }
        for (int x = 0; x < w; ++x) cov[(size_t)y * w + x] = (u8)std::clamp((int)std::lround(acc[x] * 255.0f), 0, 255);
    }
}

}  // namespace sx
