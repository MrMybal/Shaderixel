#include "doc/VectorPath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

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

// ------------------------------------------------------------------ mask tracing

namespace {
// Douglas-Peucker on pts[a..b] (inclusive), marks kept points.
void Simplify(const std::vector<float2>& pts, size_t a, size_t b, float tol, std::vector<bool>& keep) {
    if (b <= a + 1) return;
    float2 d = pts[b] - pts[a];
    float len = Length(d);
    float best = -1;
    size_t at = a;
    for (size_t i = a + 1; i < b; ++i) {
        float2 p = pts[i] - pts[a];
        float dist = len > 1e-6f ? std::fabs(p.x * d.y - p.y * d.x) / len : Length(p);
        if (dist > best) { best = dist; at = i; }
    }
    if (best <= tol) return;
    keep[at] = true;
    Simplify(pts, a, at, tol, keep);
    Simplify(pts, at, b, tol, keep);
}
}  // namespace

std::vector<SubPath> TraceMask(const u8* mask, int W, int H, float tolerance) {
    // Marching squares over pixel centers, padded by one empty pixel so every
    // contour closes. Edge ids: horizontal edge (gx,gy)-(gx+1,gy) = 2*(gy*G+gx),
    // vertical edge (gx,gy)-(gx,gy+1) = 2*(gy*G+gx)+1, with G = W+2.
    const int G = W + 2;
    auto val = [&](int x, int y) -> float {
        if (x < 0 || y < 0 || x >= W || y >= H) return 0.0f;
        return mask[(size_t)y * W + x] * (1.0f / 255.0f);
    };
    auto point = [&](u64 e) -> float2 {
        int idx = (int)(e >> 1);
        int gx = idx % G, gy = idx / G;
        int x0 = gx - 1, y0 = gy - 1, x1 = x0 + ((e & 1) ? 0 : 1), y1 = y0 + ((e & 1) ? 1 : 0);
        float a = val(x0, y0), b = val(x1, y1);
        float t = std::fabs(b - a) > 1e-6f ? std::clamp((0.5f - a) / (b - a), 0.0f, 1.0f) : 0.5f;
        return {x0 + 0.5f + (x1 - x0) * t, y0 + 0.5f + (y1 - y0) * t};
    };
    std::vector<std::array<u64, 2>> segs;
    for (int cy = -1; cy < H; ++cy)
        for (int cx = -1; cx < W; ++cx) {
            float tl = val(cx, cy), tr = val(cx + 1, cy), br = val(cx + 1, cy + 1), bl = val(cx, cy + 1);
            int c = (tl > 0.5f ? 1 : 0) | (tr > 0.5f ? 2 : 0) | (br > 0.5f ? 4 : 0) | (bl > 0.5f ? 8 : 0);
            if (c == 0 || c == 15) continue;
            const u64 gx = (u64)(cx + 1), gy = (u64)(cy + 1);
            const u64 T = 2 * (gy * G + gx), B = 2 * ((gy + 1) * G + gx), L = 2 * (gy * G + gx) + 1, R = 2 * (gy * G + gx + 1) + 1;
            bool center = (tl + tr + br + bl) * 0.25f > 0.5f;
            switch (c) {
                case 1: case 14: segs.push_back({L, T}); break;
                case 2: case 13: segs.push_back({T, R}); break;
                case 3: case 12: segs.push_back({L, R}); break;
                case 4: case 11: segs.push_back({R, B}); break;
                case 6: case 9: segs.push_back({T, B}); break;
                case 7: case 8: segs.push_back({L, B}); break;
                case 5:
                    if (center) { segs.push_back({T, R}); segs.push_back({B, L}); }
                    else { segs.push_back({L, T}); segs.push_back({R, B}); }
                    break;
                case 10:
                    if (center) { segs.push_back({L, T}); segs.push_back({R, B}); }
                    else { segs.push_back({T, R}); segs.push_back({B, L}); }
                    break;
            }
        }
    // Each crossed edge is shared by exactly two segments: chain them into loops.
    std::unordered_map<u64, std::array<int, 2>> at;
    at.reserve(segs.size() * 2);
    for (int i = 0; i < (int)segs.size(); ++i)
        for (u64 e : segs[i]) {
            auto [it, inserted] = at.try_emplace(e, std::array<int, 2>{i, -1});
            if (!inserted) it->second[1] = i;
        }
    std::vector<bool> used(segs.size(), false);
    std::vector<SubPath> out;
    for (int s0 = 0; s0 < (int)segs.size(); ++s0) {
        if (used[s0]) continue;
        std::vector<float2> loop;
        int s = s0;
        u64 e = segs[s0][1];
        used[s0] = true;
        loop.push_back(point(segs[s0][0]));
        for (;;) {
            loop.push_back(point(e));
            auto& pair = at[e];
            int next = pair[0] == s ? pair[1] : pair[0];
            if (next < 0 || used[next]) break;
            used[next] = true;
            e = segs[next][0] == e ? segs[next][1] : segs[next][0];
            s = next;
        }
        if (loop.size() < 4) continue;
        if (Length(loop.back() - loop.front()) < 1e-4f) loop.pop_back();
        // Tiny specks (less than ~3 px of area) are dropped.
        float area = 0;
        for (size_t i = 0; i < loop.size(); ++i) {
            const float2 &a = loop[i], &b = loop[(i + 1) % loop.size()];
            area += a.x * b.y - b.x * a.y;
        }
        if (std::fabs(area) * 0.5f < 3.0f) continue;
        // Simplify the closed loop: split at the point farthest from the first.
        size_t far = 0;
        for (size_t i = 1; i < loop.size(); ++i)
            if (Length(loop[i] - loop[0]) > Length(loop[far] - loop[0])) far = i;
        std::vector<float2> ring = loop;
        ring.push_back(loop[0]);
        std::vector<bool> keep(ring.size(), false);
        keep[0] = keep[far] = keep[ring.size() - 1] = true;
        Simplify(ring, 0, far, tolerance, keep);
        Simplify(ring, far, ring.size() - 1, tolerance, keep);
        std::vector<float2> pts;
        for (size_t i = 0; i + 1 < ring.size(); ++i) if (keep[i]) pts.push_back(ring[i]);
        if (pts.size() < 3) continue;
        // Bezier handles: smooth points follow the neighbor chord (Catmull-Rom),
        // turns sharper than ~55 degrees stay corners.
        SubPath sp;
        sp.closed = true;
        const size_t n = pts.size();
        for (size_t i = 0; i < n; ++i) {
            float2 p0 = pts[(i + n - 1) % n], p1 = pts[i], p2 = pts[(i + 1) % n];
            float2 a = p1 - p0, b = p2 - p1;
            float la = Length(a), lb = Length(b);
            PathAnchor an{p1, p1, p1};
            float cosTurn = la > 1e-6f && lb > 1e-6f ? (a.x * b.x + a.y * b.y) / (la * lb) : 1.0f;
            if (cosTurn > 0.57f) {
                float2 t = p2 - p0;
                float lt = Length(t);
                if (lt > 1e-6f) {
                    t = t / lt;
                    an.in = p1 - t * (la / 3.0f);
                    an.out = p1 + t * (lb / 3.0f);
                }
            }
            sp.pts.push_back(an);
        }
        out.push_back(std::move(sp));
    }
    return out;
}

}  // namespace sx
