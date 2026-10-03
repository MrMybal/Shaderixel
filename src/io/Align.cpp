// Feature based alignment (see Align.h).
#include "io/Align.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace sx::align {

Mat3 Mul(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) r[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
    return r;
}

bool Invert(const Mat3& m, Mat3& o) {
    double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
    double A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    double det = a * A + b * B + c * C;
    if (std::abs(det) < 1e-15) return false;
    double r = 1.0 / det;
    o = {A * r, -(b * i - c * h) * r, (b * f - c * e) * r, B * r, (a * i - c * g) * r, -(a * f - c * d) * r, C * r, -(a * h - b * g) * r, (a * e - b * d) * r};
    return true;
}

float2 Apply(const Mat3& m, float2 p) {
    double x = m[0] * p.x + m[1] * p.y + m[2], y = m[3] * p.x + m[4] * p.y + m[5], z = m[6] * p.x + m[7] * p.y + m[8];
    if (std::abs(z) < 1e-12) z = 1e-12;
    return {(float)(x / z), (float)(y / z)};
}

Gray ToGray(const u8* rgba, int w, int h, int maxDim) {
    Gray g;
    int s = 1;
    while (std::max(w, h) / s > maxDim) ++s;
    g.scale = (float)s;
    g.w = w / s;
    g.h = h / s;
    g.px.assign((size_t)g.w * g.h, 0.0f);
    for (int y = 0; y < g.h; ++y)
        for (int x = 0; x < g.w; ++x) {
            float acc = 0;
            for (int j = 0; j < s; ++j)
                for (int i = 0; i < s; ++i) {
                    const u8* p = rgba + ((size_t)(y * s + j) * w + (x * s + i)) * 4;
                    acc += (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) / 255.0f;
                }
            g.px[(size_t)y * g.w + x] = acc / (s * s);
        }
    return g;
}

namespace {

std::vector<float> Blur(const std::vector<float>& src, int w, int h, int r) {
    std::vector<float> tmp(src.size()), out(src.size());
    for (int y = 0; y < h; ++y) {
        float acc = 0;
        int n = 0;
        for (int x = -r; x <= r; ++x) if (x >= 0 && x < w) { acc += src[(size_t)y * w + x]; ++n; }
        for (int x = 0; x < w; ++x) {
            tmp[(size_t)y * w + x] = acc / n;
            int xo = x - r, xi = x + r + 1;
            if (xo >= 0) { acc -= src[(size_t)y * w + xo]; --n; }
            if (xi < w) { acc += src[(size_t)y * w + xi]; ++n; }
        }
    }
    for (int x = 0; x < w; ++x) {
        float acc = 0;
        int n = 0;
        for (int y = -r; y <= r; ++y) if (y >= 0 && y < h) { acc += tmp[(size_t)y * w + x]; ++n; }
        for (int y = 0; y < h; ++y) {
            out[(size_t)y * w + x] = acc / n;
            int yo = y - r, yi = y + r + 1;
            if (yo >= 0) { acc -= tmp[(size_t)yo * w + x]; --n; }
            if (yi < h) { acc += tmp[(size_t)yi * w + x]; ++n; }
        }
    }
    return out;
}

}  // namespace

std::vector<Feature> Detect(const Gray& g, int maxCount) {
    const int W = g.w, H = g.h;
    std::vector<Feature> out;
    if (W < 40 || H < 40) return out;
    std::vector<float> sm = Blur(g.px, W, H, 1);
    std::vector<float> ixx(sm.size()), iyy(sm.size()), ixy(sm.size());
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) {
            size_t i = (size_t)y * W + x;
            float gx = (sm[i + 1] - sm[i - 1]) * 0.5f, gy = (sm[i + W] - sm[i - W]) * 0.5f;
            ixx[i] = gx * gx;
            iyy[i] = gy * gy;
            ixy[i] = gx * gy;
        }
    ixx = Blur(ixx, W, H, 2);
    iyy = Blur(iyy, W, H, 2);
    ixy = Blur(ixy, W, H, 2);
    std::vector<float> R(sm.size(), 0.0f);
    for (size_t i = 0; i < R.size(); ++i) {
        float det = ixx[i] * iyy[i] - ixy[i] * ixy[i], tr = ixx[i] + iyy[i];
        R[i] = det - 0.04f * tr * tr;
    }
    // Local maxima (7x7), bucketed on a grid so the features cover the image.
    const int border = 12, G = 8;
    std::vector<std::vector<std::pair<float, int>>> cells(G * G);
    for (int y = border; y < H - border; ++y)
        for (int x = border; x < W - border; ++x) {
            float r = R[(size_t)y * W + x];
            if (r <= 1e-7f) continue;
            bool mx = true;
            for (int j = -3; j <= 3 && mx; ++j)
                for (int i = -3; i <= 3; ++i)
                    if ((i || j) && R[(size_t)(y + j) * W + x + i] > r) { mx = false; break; }
            if (mx) cells[(y * G / H) * G + x * G / W].push_back({r, y * W + x});
        }
    const int perCell = std::max(4, maxCount / (G * G));
    for (auto& c : cells) {
        std::sort(c.begin(), c.end(), [](auto& a, auto& b) { return a.first > b.first; });
        for (int k = 0; k < (int)c.size() && k < perCell; ++k) {
            int x = c[k].second % W, y = c[k].second / W;
            Feature f;
            // 8x8 samples over a 16x16 window of the smoothed image.
            float mean = 0;
            for (int j = 0; j < 8; ++j)
                for (int i = 0; i < 8; ++i) {
                    float v = sm[(size_t)(y - 8 + j * 2 + 1) * W + (x - 8 + i * 2 + 1)];
                    f.d[j * 8 + i] = v;
                    mean += v;
                }
            mean /= 64;
            float n2 = 0;
            for (auto& v : f.d) { v -= mean; n2 += v * v; }
            if (n2 < 1e-5f) continue;  // flat
            float inv = 1.0f / std::sqrt(n2);
            for (auto& v : f.d) v *= inv;
            f.x = (x + 0.5f) * g.scale;
            f.y = (y + 0.5f) * g.scale;
            out.push_back(f);
        }
    }
    return out;
}

namespace {

// Solves A x = b (n x n, Gaussian elimination with partial pivoting).
bool Solve(std::vector<double>& A, std::vector<double>& b, int n) {
    for (int c = 0; c < n; ++c) {
        int p = c;
        for (int r = c + 1; r < n; ++r) if (std::abs(A[r * n + c]) > std::abs(A[p * n + c])) p = r;
        if (std::abs(A[p * n + c]) < 1e-12) return false;
        if (p != c) {
            for (int k = 0; k < n; ++k) std::swap(A[c * n + k], A[p * n + k]);
            std::swap(b[c], b[p]);
        }
        for (int r = c + 1; r < n; ++r) {
            double f = A[r * n + c] / A[c * n + c];
            for (int k = c; k < n; ++k) A[r * n + k] -= f * A[c * n + k];
            b[r] -= f * b[c];
        }
    }
    for (int c = n - 1; c >= 0; --c) {
        double s = b[c];
        for (int k = c + 1; k < n; ++k) s -= A[c * n + k] * b[k];
        b[c] = s / A[c * n + c];
    }
    return true;
}

// Least squares fit of q ~ M(p) for the model, from the given correspondences (normal equations).
bool Fit(const std::vector<float2>& p, const std::vector<float2>& q, Model model, Mat3& m) {
    const size_t n = p.size();
    if (model == Model::Translation) {
        if (n < 1) return false;
        double tx = 0, ty = 0;
        for (size_t i = 0; i < n; ++i) { tx += q[i].x - p[i].x; ty += q[i].y - p[i].y; }
        m = {1, 0, tx / n, 0, 1, ty / n, 0, 0, 1};
        return true;
    }
    // Normalize for conditioning.
    double cx = 0, cy = 0, dx = 0, dy = 0;
    for (size_t i = 0; i < n; ++i) { cx += p[i].x; cy += p[i].y; dx += q[i].x; dy += q[i].y; }
    cx /= n; cy /= n; dx /= n; dy /= n;
    double sp = 0, sq = 0;
    for (size_t i = 0; i < n; ++i) {
        sp += std::hypot(p[i].x - cx, p[i].y - cy);
        sq += std::hypot(q[i].x - dx, q[i].y - dy);
    }
    sp = sp > 0 ? std::sqrt(2.0) * n / sp : 1;
    sq = sq > 0 ? std::sqrt(2.0) * n / sq : 1;
    Mat3 Tp = {sp, 0, -sp * cx, 0, sp, -sp * cy, 0, 0, 1}, Tq = {sq, 0, -sq * dx, 0, sq, -sq * dy, 0, 0, 1};
    Mat3 core;
    if (model == Model::Similarity) {
        // x' = a x - b y + tx, y' = b x + a y + ty
        if (n < 2) return false;
        std::vector<double> A(16, 0.0), B(4, 0.0);
        for (size_t i = 0; i < n; ++i) {
            double x = (p[i].x - cx) * sp, y = (p[i].y - cy) * sp, u = (q[i].x - dx) * sq, v = (q[i].y - dy) * sq;
            double r0[4] = {x, -y, 1, 0}, r1[4] = {y, x, 0, 1};
            for (int a = 0; a < 4; ++a) {
                for (int b2 = 0; b2 < 4; ++b2) A[a * 4 + b2] += r0[a] * r0[b2] + r1[a] * r1[b2];
                B[a] += r0[a] * u + r1[a] * v;
            }
        }
        if (!Solve(A, B, 4)) return false;
        core = {B[0], -B[1], B[2], B[1], B[0], B[3], 0, 0, 1};
    } else {
        if (n < 4) return false;
        std::vector<double> A(64, 0.0), B(8, 0.0);
        for (size_t i = 0; i < n; ++i) {
            double x = (p[i].x - cx) * sp, y = (p[i].y - cy) * sp, u = (q[i].x - dx) * sq, v = (q[i].y - dy) * sq;
            double r0[8] = {x, y, 1, 0, 0, 0, -u * x, -u * y}, r1[8] = {0, 0, 0, x, y, 1, -v * x, -v * y};
            for (int a = 0; a < 8; ++a) {
                for (int b2 = 0; b2 < 8; ++b2) A[a * 8 + b2] += r0[a] * r0[b2] + r1[a] * r1[b2];
                B[a] += r0[a] * u + r1[a] * v;
            }
        }
        if (!Solve(A, B, 8)) return false;
        core = {B[0], B[1], B[2], B[3], B[4], B[5], B[6], B[7], 1};
    }
    Mat3 Tqi;
    if (!Invert(Tq, Tqi)) return false;
    m = Mul(Tqi, Mul(core, Tp));
    if (std::abs(m[8]) > 1e-12) for (auto& v : m) v /= m[8];
    return true;
}

}  // namespace

Result Estimate(const std::vector<Feature>& a, const std::vector<Feature>& b, Model model, float threshold) {
    Result res;
    if (a.empty() || b.empty()) return res;
    // Mutual best matches with a ratio test (dot product of unit descriptors = correlation).
    auto best = [](const Feature& f, const std::vector<Feature>& set, float& s1, float& s2) {
        int bi = -1;
        s1 = -2;
        s2 = -2;
        for (int i = 0; i < (int)set.size(); ++i) {
            float s = 0;
            for (int k = 0; k < 64; ++k) s += f.d[k] * set[i].d[k];
            if (s > s1) { s2 = s1; s1 = s; bi = i; }
            else if (s > s2) s2 = s;
        }
        return bi;
    };
    std::vector<float2> pa, pb;
    for (int i = 0; i < (int)b.size(); ++i) {
        float s1, s2, t1, t2;
        int j = best(b[i], a, s1, s2);
        if (j < 0 || s1 < 0.8f || (1 - s1) > 0.75f * (1 - s2)) continue;  // distinctive enough
        if (best(a[j], b, t1, t2) != i) continue;                           // mutual
        pa.push_back({a[j].x, a[j].y});
        pb.push_back({b[i].x, b[i].y});
    }
    res.matches = (int)pa.size();
    const int need = model == Model::Translation ? 1 : model == Model::Similarity ? 2 : 4;
    if ((int)pa.size() < need + 2) return res;
    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> pick(0, (int)pa.size() - 1);
    std::vector<int> bestIn;
    const float t2 = threshold * threshold;
    for (int it = 0; it < 2000; ++it) {
        std::vector<float2> sp, sq;
        for (int k = 0; k < need; ++k) {
            int r = pick(rng);
            sp.push_back(pb[r]);
            sq.push_back(pa[r]);
        }
        Mat3 m;
        if (!Fit(sp, sq, model, m)) continue;
        std::vector<int> in;
        for (int k = 0; k < (int)pa.size(); ++k) {
            float2 q = Apply(m, pb[k]);
            float dx = q.x - pa[k].x, dy = q.y - pa[k].y;
            if (dx * dx + dy * dy < t2) in.push_back(k);
        }
        if (in.size() > bestIn.size()) bestIn = std::move(in);
    }
    if ((int)bestIn.size() < need + 2) return res;
    // Refit on the inliers (twice: the consensus may grow).
    Mat3 m;
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<float2> sp, sq;
        for (int k : bestIn) { sp.push_back(pb[k]); sq.push_back(pa[k]); }
        if (!Fit(sp, sq, model, m)) return res;
        std::vector<int> in;
        for (int k = 0; k < (int)pa.size(); ++k) {
            float2 q = Apply(m, pb[k]);
            float dx = q.x - pa[k].x, dy = q.y - pa[k].y;
            if (dx * dx + dy * dy < t2) in.push_back(k);
        }
        if (in.size() < bestIn.size()) break;
        bestIn = std::move(in);
    }
    res.ok = true;
    res.m = m;
    res.inliers = (int)bestIn.size();
    return res;
}

float2 ToCylinder(float2 p, float f, float cx, float cy) {
    float x = p.x - cx, y = p.y - cy;
    float th = std::atan2(x, f);
    float h = y / std::sqrt(x * x + f * f);
    return {f * th + cx, f * h + cy};
}

float2 FromCylinder(float2 q, float f, float cx, float cy) {
    float th = (q.x - cx) / f, h = (q.y - cy) / f;
    float x = std::tan(th) * f;
    float y = h * std::sqrt(x * x + f * f);
    return {x + cx, y + cy};
}

}  // namespace sx::align
