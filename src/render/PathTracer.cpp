// Path tracer: scene flattening, BVH, environment sampling tables and the CPU
// integrator (shaders/pathtrace.slang is the GPU twin of PtTrace).
#include "render/PathTracer.h"

#include <stb_image.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace sx {

namespace {

constexpr float kPi = 3.14159265358979f;

inline float3 Mul(float3 a, float3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline float3 Min3(float3 a, float3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline float3 Max3(float3 a, float3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline float Comp(float3 a, int k) { return k == 0 ? a.x : k == 1 ? a.y : a.z; }
inline float Lum(float3 c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }
inline float3 Lerp3(float3 a, float3 b, float t) { return a + (b - a) * t; }

// Rotation (degrees): yaw around Y, pitch around X, roll around Z.
struct Rot {
    float m[9];
    explicit Rot(float3 deg) {
        float y = deg.x * kPi / 180, p = deg.y * kPi / 180, r = deg.z * kPi / 180;
        float cy = std::cos(y), sy = std::sin(y), cp = std::cos(p), sp = std::sin(p), cr = std::cos(r), sr = std::sin(r);
        // R = Ry * Rx * Rz
        float Ry[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy}, Rx[9] = {1, 0, 0, 0, cp, -sp, 0, sp, cp}, Rz[9] = {cr, -sr, 0, sr, cr, 0, 0, 0, 1};
        float t[9];
        auto mul = [](const float* a, const float* b, float* o) {
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) o[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
        };
        mul(Ry, Rx, t);
        mul(t, Rz, m);
    }
    float3 operator()(float3 v) const {
        return {m[0] * v.x + m[1] * v.y + m[2] * v.z, m[3] * v.x + m[4] * v.y + m[5] * v.z, m[6] * v.x + m[7] * v.y + m[8] * v.z};
    }
};

// ---------------------------------------------------------------- BVH (binned SAH)
struct Builder {
    std::vector<PtTriangle>& tris;
    std::vector<PtNode>& nodes;
    std::vector<float3> centroid;
    void Bounds(PtNode& n) {
        n.bmin = {1e30f, 1e30f, 1e30f};
        n.bmax = {-1e30f, -1e30f, -1e30f};
        for (u32 i = 0; i < n.count; ++i)
            for (auto& p : tris[n.leftFirst + i].p) { n.bmin = Min3(n.bmin, p); n.bmax = Max3(n.bmax, p); }
    }
    static float Area(float3 a, float3 b) {
        float3 e = b - a;
        return e.x < 0 ? 0 : 2 * (e.x * e.y + e.y * e.z + e.z * e.x);
    }
    void Split(u32 ni, int depth) {
        PtNode& n = nodes[ni];
        if (n.count <= 4 || depth > 60) return;
        float3 cmin = {1e30f, 1e30f, 1e30f}, cmax = {-1e30f, -1e30f, -1e30f};
        for (u32 i = 0; i < n.count; ++i) { cmin = Min3(cmin, centroid[n.leftFirst + i]); cmax = Max3(cmax, centroid[n.leftFirst + i]); }
        constexpr int B = 12;
        float bestCost = 1e30f;
        int bestAxis = -1, bestBin = 0;
        for (int axis = 0; axis < 3; ++axis) {
            float lo = Comp(cmin, axis), hi = Comp(cmax, axis);
            if (hi - lo < 1e-9f) continue;
            struct Bin { float3 bmin{1e30f, 1e30f, 1e30f}, bmax{-1e30f, -1e30f, -1e30f}; u32 count = 0; } bins[B];
            float k = B / (hi - lo);
            for (u32 i = 0; i < n.count; ++i) {
                int b = std::min(B - 1, (int)((Comp(centroid[n.leftFirst + i], axis) - lo) * k));
                bins[b].count++;
                for (auto& p : tris[n.leftFirst + i].p) { bins[b].bmin = Min3(bins[b].bmin, p); bins[b].bmax = Max3(bins[b].bmax, p); }
            }
            float leftArea[B - 1], rightArea[B - 1];
            u32 leftCount[B - 1], rightCount[B - 1];
            float3 lmin{1e30f, 1e30f, 1e30f}, lmax{-1e30f, -1e30f, -1e30f}, rmin = lmin, rmax = lmax;
            u32 ls = 0, rs = 0;
            for (int i = 0; i < B - 1; ++i) {
                ls += bins[i].count;
                lmin = Min3(lmin, bins[i].bmin);
                lmax = Max3(lmax, bins[i].bmax);
                leftCount[i] = ls;
                leftArea[i] = Area(lmin, lmax);
                rs += bins[B - 1 - i].count;
                rmin = Min3(rmin, bins[B - 1 - i].bmin);
                rmax = Max3(rmax, bins[B - 1 - i].bmax);
                rightCount[B - 2 - i] = rs;
                rightArea[B - 2 - i] = Area(rmin, rmax);
            }
            for (int i = 0; i < B - 1; ++i) {
                float cost = leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i];
                if (leftCount[i] && rightCount[i] && cost < bestCost) { bestCost = cost; bestAxis = axis; bestBin = i; }
            }
        }
        if (bestAxis < 0 || bestCost >= n.count * Area(n.bmin, n.bmax)) return;
        float lo = Comp(cmin, bestAxis), k = B / (Comp(cmax, bestAxis) - lo);
        u32 i = n.leftFirst, j = i + n.count - 1;
        while (i <= j) {
            int b = std::min(B - 1, (int)((Comp(centroid[i], bestAxis) - lo) * k));
            if (b <= bestBin) ++i;
            else {
                std::swap(tris[i], tris[j]);
                std::swap(centroid[i], centroid[j]);
                if (j == 0) break;
                --j;
            }
        }
        u32 leftCount = i - n.leftFirst;
        if (leftCount == 0 || leftCount == n.count) return;
        u32 left = (u32)nodes.size();
        nodes.push_back({});
        nodes.push_back({});
        PtNode& nn = nodes[ni];  // nodes may have moved
        nodes[left].leftFirst = nn.leftFirst;
        nodes[left].count = leftCount;
        nodes[left + 1].leftFirst = i;
        nodes[left + 1].count = nn.count - leftCount;
        nn.leftFirst = left;
        nn.count = 0;
        Bounds(nodes[left]);
        Bounds(nodes[left + 1]);
        Split(left, depth + 1);
        Split(left + 1, depth + 1);
    }
};

}  // namespace

float3 PtLight::Direction() const {
    float y = yaw * kPi / 180, p = pitch * kPi / 180;
    return Normalize({std::cos(p) * std::sin(y), std::sin(p), std::cos(p) * std::cos(y)});
}

void BuildPtScene(const std::vector<PtObject>& objects, const std::vector<PtLight>& lights, PtScene& out) {
    out = {};
    for (auto& o : objects) {
        out.materials.push_back(o.material);
        int mat = (int)out.materials.size() - 1;
        if (!o.visible || !o.mesh) continue;
        Rot R(o.rotation);
        const MeshData& m = *o.mesh;
        for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            PtTriangle tri;
            for (int k = 0; k < 3; ++k) {
                const MeshVertex& v = m.vertices[m.indices[t + k]];
                tri.p[k] = R(v.pos * o.scale) + o.position;
                tri.n[k] = Normalize(R(v.normal));
                tri.uv[k] = v.uv;
            }
            tri.material = mat;
            out.triangles.push_back(tri);
        }
    }
    for (auto& l : lights) if (l.visible) out.lights.push_back(l);
    if (out.triangles.empty()) return;
    out.nodes.reserve(out.triangles.size() * 2);
    out.nodes.push_back({});
    out.nodes[0].leftFirst = 0;
    out.nodes[0].count = (u32)out.triangles.size();
    Builder b{out.triangles, out.nodes, {}};
    b.centroid.resize(out.triangles.size());
    for (size_t i = 0; i < out.triangles.size(); ++i) b.centroid[i] = (out.triangles[i].p[0] + out.triangles[i].p[1] + out.triangles[i].p[2]) * (1.0f / 3);
    b.Bounds(out.nodes[0]);
    b.Split(0, 0);
}

std::shared_ptr<PtEnvironment> LoadPtEnvironment(const std::string& path, std::string* error) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(path).c_str(), L"rb") != 0 || !f) { if (error) *error = "fichier introuvable"; return nullptr; }
    int w = 0, h = 0, comp = 0;
    float* src = stbi_loadf_from_file(f, &w, &h, &comp, 3);
    fclose(f);
    if (!src) { if (error) *error = "image illisible"; return nullptr; }
    // Large panoramas: halved until 2048 wide (the GPU texture and the tables stay reasonable).
    int step = 1;
    while (w / step > 2048) step *= 2;
    auto env = std::make_shared<PtEnvironment>();
    env->width = w / step;
    env->height = h / step;
    env->pixels.resize((size_t)env->width * env->height);
    for (int y = 0; y < env->height; ++y)
        for (int x = 0; x < env->width; ++x) {
            float3 acc{0, 0, 0};
            for (int j = 0; j < step; ++j)
                for (int i = 0; i < step; ++i) {
                    const float* p = &src[((size_t)(y * step + j) * w + (x * step + i)) * 3];
                    acc = acc + float3{p[0], p[1], p[2]};
                }
            acc = acc * (1.0f / (step * step));
            env->pixels[(size_t)y * env->width + x] = {acc.x, acc.y, acc.z, 1};
        }
    stbi_image_free(src);
    BuildEnvTables(*env);
    return env;
}

std::shared_ptr<PtEnvironment> MakeStudioEnvironment() {
    // Gray gradient (darker floor), a large soft box above the front, a strip on the side, a fill on the other side.
    auto env = std::make_shared<PtEnvironment>();
    env->width = 1024;
    env->height = 512;
    env->pixels.resize((size_t)env->width * env->height);
    struct Box { float3 dir; float size; float3 color; };
    const Box boxes[] = {{Normalize({-0.45f, 0.75f, -0.5f}), 0.32f, {9.0f, 8.6f, 8.2f}},
                         {Normalize({0.95f, 0.25f, 0.2f}), 0.14f, {3.5f, 3.7f, 4.2f}},
                         {Normalize({-0.6f, 0.15f, 0.8f}), 0.22f, {1.2f, 1.2f, 1.25f}}};
    for (int y = 0; y < env->height; ++y)
        for (int x = 0; x < env->width; ++x) {
            float u = (x + 0.5f) / env->width, v = (y + 0.5f) / env->height;
            float phi = (u - 0.5f) * 2 * kPi, th = v * kPi;
            float3 d = {std::sin(th) * std::sin(phi), std::cos(th), -std::sin(th) * std::cos(phi)};
            float t = d.y;
            float3 c = t > 0 ? Lerp3({0.16f, 0.16f, 0.17f}, {0.30f, 0.31f, 0.33f}, std::min(t * 1.5f, 1.0f))
                             : Lerp3({0.16f, 0.16f, 0.17f}, {0.05f, 0.05f, 0.05f}, std::min(-t * 4.0f, 1.0f));
            for (const Box& b : boxes) {
                // Soft rectangle around b.dir (angular distance in the box's tangent plane).
                float3 bu = Normalize(Cross({0, 1, 0}, b.dir)), bv = Cross(b.dir, bu);
                float cd = Dot(d, b.dir);
                if (cd <= 0) continue;
                float px = Dot(d, bu) / cd, py = Dot(d, bv) / cd;
                float e = std::max(std::abs(px) / (b.size * 1.6f), std::abs(py) / b.size);
                float k = std::clamp((1.0f - e) / 0.12f, 0.0f, 1.0f);
                c = c + b.color * (k * k * (3 - 2 * k));
            }
            env->pixels[(size_t)y * env->width + x] = {c.x, c.y, c.z, 1};
        }
    BuildEnvTables(*env);
    return env;
}

void BuildEnvTables(PtEnvironment& e) {
    PtEnvironment* env = &e;
    // Importance: luminance * sin(theta); conditional CDF per row, marginal over rows.
    const int W = env->width, H = env->height;
    env->conditional.resize((size_t)W * H);
    env->marginal.resize(H);
    double total = 0;
    std::vector<double> rowSum(H);
    for (int y = 0; y < H; ++y) {
        float s = std::sin((y + 0.5f) / H * kPi);
        double acc = 0;
        for (int x = 0; x < W; ++x) {
            const float4& p = env->pixels[(size_t)y * W + x];
            acc += std::max(1e-6f, 0.2126f * p.x + 0.7152f * p.y + 0.0722f * p.z) * s;
            env->conditional[(size_t)y * W + x] = (float)acc;
        }
        rowSum[y] = acc;
        for (int x = 0; x < W; ++x) env->conditional[(size_t)y * W + x] = acc > 0 ? (float)(env->conditional[(size_t)y * W + x] / acc) : (x + 1.0f) / W;
        total += acc;
    }
    double acc = 0;
    for (int y = 0; y < H; ++y) {
        acc += rowSum[y];
        env->marginal[y] = total > 0 ? (float)(acc / total) : (y + 1.0f) / H;
    }
    env->total = (float)total;
}

MeshData MakeBackdrop() {
    // Floor (z from -3 to 1) bending up into a wall at z > 1 (radius 1.2), 20 wide.
    MeshData m;
    m.name = "Fond";
    const int sx = 24, sz = 48;
    const float half = 10.0f, R = 1.2f;
    std::vector<float3> prof;  // (z, y) along the profile, with normals
    std::vector<float3> nrm;
    for (int i = 0; i <= sz; ++i) {
        float t = i / (float)sz;
        float s = t * (4.0f + kPi * R * 0.5f + 3.0f);  // floor 4, bend, wall 3
        float z, y;
        float3 n;
        if (s < 4.0f) { z = -3.0f + s; y = 0; n = {0, 1, 0}; }
        else if (s < 4.0f + kPi * R * 0.5f) {
            float a = (s - 4.0f) / R;
            z = 1.0f + std::sin(a) * R;
            y = R - std::cos(a) * R;
            n = {0, std::cos(a), -std::sin(a)};
        } else { z = 1.0f + R; y = R + (s - 4.0f - kPi * R * 0.5f); n = {0, 0, -1}; }
        prof.push_back({0, y, z});
        nrm.push_back(n);
    }
    for (int i = 0; i <= sz; ++i)
        for (int j = 0; j <= sx; ++j) {
            float x = -half + 2 * half * j / sx;
            m.vertices.push_back({{x, prof[i].y, prof[i].z}, nrm[i], {(float)j / sx, (float)i / sz}});
        }
    for (int i = 0; i < sz; ++i)
        for (int j = 0; j < sx; ++j) {
            u32 a = i * (sx + 1) + j, b = a + 1, c = a + sx + 1, d = c + 1;
            m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
        }
    return m;
}

MeshData MakeFloorPlane(float half) {
    MeshData m;
    m.name = "Sol";
    m.vertices = {{{-half, 0, -half}, {0, 1, 0}, {0, 0}}, {{half, 0, -half}, {0, 1, 0}, {1, 0}}, {{-half, 0, half}, {0, 1, 0}, {0, 1}}, {{half, 0, half}, {0, 1, 0}, {1, 1}}};
    m.indices = {0, 2, 1, 1, 2, 3};
    return m;
}

float4 PtImage::Sample(float2 uv) const {
    if (pixels.empty()) return {0, 0, 0, 0};
    float fx = (uv.x - std::floor(uv.x)) * width - 0.5f, fy = (uv.y - std::floor(uv.y)) * height - 0.5f;
    int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
    float ax = fx - x0, ay = fy - y0;
    auto at = [&](int x, int y) {
        x = ((x % width) + width) % width;
        y = ((y % height) + height) % height;
        return pixels[(size_t)y * width + x];
    };
    float4 a = at(x0, y0), b = at(x0 + 1, y0), c = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
    auto mix = [](float4 p, float4 q, float t) { return float4{p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, p.z + (q.z - p.z) * t, p.w + (q.w - p.w) * t}; };
    return mix(mix(a, b, ax), mix(c, d, ax), ay);
}

// ---------------------------------------------------------------- CPU integrator

namespace {

inline u32 Pcg(u32& s) {
    s = s * 747796405u + 2891336453u;
    u32 w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
inline float Rnd(u32& s) { return (Pcg(s) >> 8) * (1.0f / 16777216.0f); }

struct Hit {
    float t = 1e30f;
    int tri = -1;
    float b1 = 0, b2 = 0;
    int light = -1;  // area light hit
};

bool RayBox(float3 o, float3 inv, float3 bmin, float3 bmax, float tmax) {
    float t0 = 0, t1 = tmax;
    for (int k = 0; k < 3; ++k) {
        float a = (Comp(bmin, k) - Comp(o, k)) * Comp(inv, k), b = (Comp(bmax, k) - Comp(o, k)) * Comp(inv, k);
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1) return false;
    }
    return true;
}

void Intersect(const PtScene& sc, float3 o, float3 d, Hit& h, bool any, float tmax) {
    h.t = tmax;
    if (sc.nodes.empty()) return;
    float3 inv = {1.0f / (std::abs(d.x) > 1e-12f ? d.x : 1e-12f), 1.0f / (std::abs(d.y) > 1e-12f ? d.y : 1e-12f), 1.0f / (std::abs(d.z) > 1e-12f ? d.z : 1e-12f)};
    u32 stack[64];
    int sp = 0;
    stack[sp++] = 0;
    while (sp) {
        const PtNode& n = sc.nodes[stack[--sp]];
        if (!RayBox(o, inv, n.bmin, n.bmax, h.t)) continue;
        if (n.count) {
            for (u32 i = 0; i < n.count; ++i) {
                const PtTriangle& tr = sc.triangles[n.leftFirst + i];
                float3 e1 = tr.p[1] - tr.p[0], e2 = tr.p[2] - tr.p[0];
                float3 pv = Cross(d, e2);
                float det = Dot(e1, pv);
                if (std::abs(det) < 1e-12f) continue;
                float id = 1.0f / det;
                float3 tv = o - tr.p[0];
                float u = Dot(tv, pv) * id;
                if (u < 0 || u > 1) continue;
                float3 qv = Cross(tv, e1);
                float v = Dot(d, qv) * id;
                if (v < 0 || u + v > 1) continue;
                float t = Dot(e2, qv) * id;
                if (t > 1e-4f && t < h.t) {
                    h.t = t;
                    h.tri = (int)(n.leftFirst + i);
                    h.b1 = u;
                    h.b2 = v;
                    if (any) return;
                }
            }
        } else if (sp < 62) {
            stack[sp++] = n.leftFirst;
            stack[sp++] = n.leftFirst + 1;
        }
    }
}

// Area lights: one-sided squares facing their direction.
bool AreaLightHit(const PtLight& l, float3 o, float3 d, float& t) {
    float3 n = l.Direction();
    float dn = Dot(d, n);
    if (dn >= -1e-6f) return false;
    t = Dot(l.position - o, n) / dn;
    if (t <= 1e-4f) return false;
    float3 p = o + d * t - l.position;
    float3 u = Normalize(std::abs(n.y) < 0.99f ? Cross({0, 1, 0}, n) : Cross({1, 0, 0}, n)), v = Cross(n, u);
    return std::abs(Dot(p, u)) <= l.size * 0.5f && std::abs(Dot(p, v)) <= l.size * 0.5f;
}

float3 EnvLookup(const PtScene& sc, const PtSettings& s, float3 d) {
    if (!sc.env) return s.background;
    const PtEnvironment& e = *sc.env;
    // Same mapping as the 3D view (env_common.slang).
    float phi = std::atan2(d.x, -d.z), theta = std::acos(std::clamp(d.y, -1.0f, 1.0f));
    float u = phi / (2 * kPi) + 0.5f + s.envRotation / 360.0f;
    u -= std::floor(u);
    int x = std::clamp((int)(u * e.width), 0, e.width - 1), y = std::clamp((int)(theta / kPi * e.height), 0, e.height - 1);
    const float4& p = e.pixels[(size_t)y * e.width + x];
    return float3{p.x, p.y, p.z} * s.envIntensity;
}

float EnvPdf(const PtScene& sc, const PtSettings& s, float3 d) {
    const PtEnvironment& e = *sc.env;
    // Same mapping as the 3D view (env_common.slang).
    float phi = std::atan2(d.x, -d.z), theta = std::acos(std::clamp(d.y, -1.0f, 1.0f));
    float u = phi / (2 * kPi) + 0.5f + s.envRotation / 360.0f;
    u -= std::floor(u);
    int x = std::clamp((int)(u * e.width), 0, e.width - 1), y = std::clamp((int)(theta / kPi * e.height), 0, e.height - 1);
    const float4& p = e.pixels[(size_t)y * e.width + x];
    float l = std::max(1e-6f, 0.2126f * p.x + 0.7152f * p.y + 0.0722f * p.z);
    return e.total > 0 ? l * e.width * e.height / (e.total * 2 * kPi * kPi) : 1.0f / (4 * kPi);
}

float3 EnvSample(const PtScene& sc, const PtSettings& s, u32& rng, float& pdf) {
    const PtEnvironment& e = *sc.env;
    float r1 = Rnd(rng), r2 = Rnd(rng);
    int y = (int)(std::lower_bound(e.marginal.begin(), e.marginal.end(), r1) - e.marginal.begin());
    y = std::clamp(y, 0, e.height - 1);
    const float* row = &e.conditional[(size_t)y * e.width];
    int x = (int)(std::lower_bound(row, row + e.width, r2) - row);
    x = std::clamp(x, 0, e.width - 1);
    float u = (x + Rnd(rng)) / e.width - s.envRotation / 360.0f, v = (y + Rnd(rng)) / e.height;
    float phi = (u - 0.5f) * 2 * kPi, theta = v * kPi;
    float3 d = {std::sin(theta) * std::sin(phi), std::cos(theta), -std::sin(theta) * std::cos(phi)};
    pdf = EnvPdf(sc, s, d);
    return d;
}

// GGX helpers
float D_GGX(float nh, float a) {
    float a2 = a * a, d = nh * nh * (a2 - 1) + 1;
    return a2 / (kPi * d * d + 1e-12f);
}
float G1(float nx, float a) {
    float k = a * 0.5f;
    return nx / (nx * (1 - k) + k + 1e-12f);
}
float3 FSchlick(float3 f0, float c) {
    float t = std::pow(1 - std::clamp(c, 0.0f, 1.0f), 5.0f);
    return f0 + (float3{1, 1, 1} - f0) * t;
}
void Basis(float3 n, float3& t, float3& b) {
    t = Normalize(std::abs(n.x) > 0.5f ? Cross({0, 1, 0}, n) : Cross({1, 0, 0}, n));
    b = Cross(n, t);
}

struct Surf {
    float3 p, n, ng, base, emission;
    float rough, metal, transmission, ior;
};

// BSDF value (times cos) and pdf for a direction pair (opaque lobes).
float3 BsdfEval(const Surf& s, float3 wo, float3 wi, float pSpec, float& pdf) {
    float nl = Dot(s.n, wi), nv = Dot(s.n, wo);
    pdf = 0;
    if (nl <= 0 || nv <= 0) return {0, 0, 0};
    float a = std::max(s.rough * s.rough, 0.002f);
    float3 h = Normalize(wi + wo);
    float nh = std::max(Dot(s.n, h), 0.0f), vh = std::max(Dot(wo, h), 0.0f);
    float3 f0 = Lerp3({0.04f, 0.04f, 0.04f}, s.base, s.metal);
    float3 F = FSchlick(f0, vh);
    float D = D_GGX(nh, a), G = G1(nl, a) * G1(nv, a);
    float3 spec = F * (D * G / (4 * nv + 1e-6f));
    float3 diff = Mul(s.base * ((1 - s.metal) / kPi), float3{1, 1, 1} - F) * nl;
    pdf = pSpec * D * nh / (4 * vh + 1e-6f) + (1 - pSpec) * nl / kPi;
    return spec + diff;
}

float FresnelDielectric(float cosi, float eta) {
    float s2 = (1 - cosi * cosi) / (eta * eta);
    if (s2 >= 1) return 1;
    float cost = std::sqrt(1 - s2);
    float rs = (cosi - eta * cost) / (cosi + eta * cost), rp = (eta * cosi - cost) / (eta * cosi + cost);
    return 0.5f * (rs * rs + rp * rp);
}

float Power(float a, float b) { return a * a / (a * a + b * b + 1e-12f); }

}  // namespace

float3 PtTrace(const PtScene& sc, const PtProjectTextures* tex, const PtSettings& s, float3 o, float3 d, u32& rng) {
    float3 L{0, 0, 0}, beta{1, 1, 1};
    float lastPdf = 0;
    bool specularBounce = true;
    const int nLights = (int)sc.lights.size() + (sc.env ? 1 : 0);
    for (int bounce = 0; bounce <= s.maxBounces; ++bounce) {
        Hit h;
        Intersect(sc, o, d, h, false, 1e30f);
        // Area lights in front of the geometry (seen in reflections and bounces, not by the camera).
        for (int i = 0; i < (int)sc.lights.size(); ++i) {
            float t;
            if (bounce > 0 && sc.lights[i].type == PtLightType::Area && AreaLightHit(sc.lights[i], o, d, t) && t < h.t) { h.t = t; h.light = i; h.tri = -1; }
        }
        auto add = [&](float3 c) {
            if (bounce > 0 && s.clampIndirect > 0) {
                float l = Lum(c);
                if (l > s.clampIndirect) c = c * (s.clampIndirect / l);
            }
            L = L + c;
        };
        if (h.light >= 0) {
            const PtLight& l = sc.lights[h.light];
            float area = l.size * l.size;
            float3 Le = l.color * (l.intensity / (kPi * area));
            float w = 1;
            if (!specularBounce) {
                float cosl = std::max(-Dot(d, l.Direction()), 1e-4f);
                float lpdf = h.t * h.t / (cosl * area) / nLights;
                w = Power(lastPdf, lpdf);
            }
            add(Mul(beta, Le) * w);
            break;
        }
        if (h.tri < 0) {
            float3 env = EnvLookup(sc, s, d);
            if (bounce == 0) { if (s.envVisible || !sc.env) L = L + (sc.env ? env : s.background); break; }
            if (!sc.env) { add(Mul(beta, s.background) * 0.0f); break; }
            float w = specularBounce ? 1.0f : Power(lastPdf, EnvPdf(sc, s, d) / nLights);
            add(Mul(beta, env) * w);
            break;
        }
        // Surface.
        const PtTriangle& tr = sc.triangles[h.tri];
        const PtMaterial& m = sc.materials[tr.material];
        float b0 = 1 - h.b1 - h.b2;
        Surf su;
        su.p = o + d * h.t;
        su.ng = Normalize(Cross(tr.p[1] - tr.p[0], tr.p[2] - tr.p[0]));
        su.n = Normalize(tr.n[0] * b0 + tr.n[1] * h.b1 + tr.n[2] * h.b2);
        float2 uv = {tr.uv[0].x * b0 + tr.uv[1].x * h.b1 + tr.uv[2].x * h.b2, tr.uv[0].y * b0 + tr.uv[1].y * h.b1 + tr.uv[2].y * h.b2};
        su.base = m.baseColor;
        su.rough = m.roughness;
        su.metal = m.metallic;
        su.emission = m.emission * m.emissionStrength;
        su.transmission = m.transmission;
        su.ior = m.ior;
        if (m.useProject && tex) {
            float4 b = tex->base.Sample(uv);
            if (b.w > 1e-4f) su.base = {b.x, b.y, b.z};  // packed straight (pt_pack)
            float4 orm = tex->orm.Sample(uv);
            su.rough = orm.x;
            su.metal = orm.y;
            float4 e = tex->emissive.Sample(uv);
            su.emission = su.emission + float3{e.x, e.y, e.z};
            if (!tex->normal.pixels.empty()) {
                float4 nm = tex->normal.Sample(uv);
                float3 tn = {nm.x * 2 - 1, nm.y * 2 - 1, nm.z * 2 - 1};
                // Tangent frame from the triangle's UVs.
                float3 e1 = tr.p[1] - tr.p[0], e2 = tr.p[2] - tr.p[0];
                float du1 = tr.uv[1].x - tr.uv[0].x, dv1 = tr.uv[1].y - tr.uv[0].y, du2 = tr.uv[2].x - tr.uv[0].x, dv2 = tr.uv[2].y - tr.uv[0].y;
                float r = du1 * dv2 - du2 * dv1;
                if (std::abs(r) > 1e-12f) {
                    float3 T = (e1 * dv2 - e2 * dv1) * (1.0f / r), B = (e2 * du1 - e1 * du2) * (1.0f / r);
                    T = Normalize(T - su.n * Dot(su.n, T));
                    B = Normalize(B - su.n * Dot(su.n, B) - T * Dot(T, B));
                    su.n = Normalize(T * tn.x + B * tn.y + su.n * tn.z);
                }
            }
        }
        su.rough = std::clamp(su.rough, 0.02f, 1.0f);
        float3 wo = d * -1.0f;
        if (Dot(su.ng, tr.n[0] + tr.n[1] + tr.n[2]) < 0) su.ng = su.ng * -1.0f;  // outward like the vertex normals (any winding)
        bool entering = Dot(su.ng, wo) > 0;
        if (!entering && su.transmission <= 0) { su.n = su.n * -1.0f; su.ng = su.ng * -1.0f; }
        if (Dot(su.n, wo) <= 0) su.n = Normalize(su.n + su.ng * (0.01f - Dot(su.n, wo)));
        if (Lum(su.emission) > 0) add(Mul(beta, su.emission));

        // Dielectric (glass): reflect or refract, no light sampling.
        if (su.transmission > 0 && Rnd(rng) < su.transmission) {
            float3 n = entering ? su.ng : su.ng * -1.0f;
            float3 sn = Dot(su.n, n) > 0 ? su.n : su.n * -1.0f;
            float a = su.rough * su.rough;
            float3 t, b;
            Basis(sn, t, b);
            float r1 = Rnd(rng), r2 = Rnd(rng);
            float th = std::atan(a * std::sqrt(r1 / (1 - r1 + 1e-6f))), ph = 2 * kPi * r2;
            float3 hm = Normalize(t * (std::sin(th) * std::cos(ph)) + b * (std::sin(th) * std::sin(ph)) + sn * std::cos(th));
            float eta = entering ? su.ior : 1.0f / su.ior;
            float cosi = std::max(Dot(wo, hm), 1e-4f);
            float F = FresnelDielectric(cosi, eta);
            if (Rnd(rng) < F) d = Normalize(hm * (2 * Dot(wo, hm)) - wo);
            else {
                float k = 1 - (1 - cosi * cosi) / (eta * eta);
                d = Normalize(wo * (-1.0f / eta) + hm * (cosi / eta - std::sqrt(std::max(k, 0.0f))));
                beta = Mul(beta, su.base);
            }
            o = su.p + d * 1e-3f;
            specularBounce = true;
            continue;
        }
        float3 f0 = Lerp3({0.04f, 0.04f, 0.04f}, su.base, su.metal);
        float pSpec = std::clamp(Lum(FSchlick(f0, std::max(Dot(su.n, wo), 0.0f))) * 1.5f + su.metal * 0.5f, 0.1f, 0.9f);
        // Next-event estimation: one light (or the environment).
        if (nLights > 0) {
            int pick = std::min((int)(Rnd(rng) * nLights), nLights - 1);
            float3 wi, Li{0, 0, 0};
            float dist = 1e30f, lpdf = 0;
            bool delta = false;
            if (pick < (int)sc.lights.size()) {
                const PtLight& l = sc.lights[pick];
                if (l.type == PtLightType::Area) {
                    float3 n = l.Direction(), u, v;
                    u = Normalize(std::abs(n.y) < 0.99f ? Cross({0, 1, 0}, n) : Cross({1, 0, 0}, n));
                    v = Cross(n, u);  // same square as AreaLightHit
                    float3 q = l.position + u * ((Rnd(rng) - 0.5f) * l.size) + v * ((Rnd(rng) - 0.5f) * l.size);
                    float3 dv = q - su.p;
                    dist = Length(dv);
                    wi = dv * (1.0f / dist);
                    float cosl = -Dot(wi, n);
                    if (cosl > 1e-4f) {
                        float area = l.size * l.size;
                        lpdf = dist * dist / (cosl * area);
                        Li = l.color * (l.intensity / (kPi * area));
                    }
                } else if (l.type == PtLightType::Sun) {
                    float3 dir = l.Direction() * -1.0f, u, v;  // towards the sun
                    Basis(dir, u, v);
                    float ang = l.size * 0.5f * kPi / 180;
                    float r = std::sqrt(Rnd(rng)) * std::tan(ang), ph = 2 * kPi * Rnd(rng);
                    wi = Normalize(dir + u * (r * std::cos(ph)) + v * (r * std::sin(ph)));
                    Li = l.color * l.intensity;
                    lpdf = 1;
                    delta = true;
                } else {
                    // Point / spot: a small sphere (soft shadows).
                    float3 dv = l.position - su.p;
                    float3 u, v;
                    float3 c = Normalize(dv);
                    Basis(c, u, v);
                    float r = std::sqrt(Rnd(rng)) * l.size, ph = 2 * kPi * Rnd(rng);
                    float3 q = l.position + u * (r * std::cos(ph)) + v * (r * std::sin(ph));
                    float3 dq = q - su.p;
                    dist = Length(dq);
                    wi = dq * (1.0f / dist);
                    float I = l.intensity / (4 * kPi);
                    float fall = 1;
                    if (l.type == PtLightType::Spot) {
                        float cosA = Dot(wi * -1.0f, l.Direction()), outer = std::cos(l.spotAngle * 0.5f * kPi / 180);
                        float inner = std::cos(l.spotAngle * 0.4f * kPi / 180);
                        fall = std::clamp((cosA - outer) / std::max(inner - outer, 1e-4f), 0.0f, 1.0f);
                        fall = fall * fall * (3 - 2 * fall);
                    }
                    Li = l.color * (I * fall / std::max(dist * dist, 1e-4f));
                    lpdf = 1;
                    delta = true;
                }
            } else {
                wi = EnvSample(sc, s, rng, lpdf);
                Li = EnvLookup(sc, s, wi);
            }
            if (lpdf > 0 && Lum(Li) > 0) {
                float bpdf;
                float3 f = BsdfEval(su, wo, wi, pSpec, bpdf);
                if (Lum(f) > 0) {
                    Hit sh;
                    Intersect(sc, su.p + su.ng * 1e-3f, wi, sh, true, dist - 2e-3f);
                    if (sh.tri < 0) {
                        float w = delta ? 1.0f : Power(lpdf / nLights, bpdf);
                        add(Mul(Mul(beta, f), Li) * (w * nLights / (delta ? 1.0f : lpdf)));
                    }
                }
            }
        }
        // BSDF sampling.
        float3 t, b;
        Basis(su.n, t, b);
        float3 wi;
        if (Rnd(rng) < pSpec) {
            float a = std::max(su.rough * su.rough, 0.002f);
            float r1 = Rnd(rng), r2 = Rnd(rng);
            float th = std::atan(a * std::sqrt(r1 / (1 - r1 + 1e-6f))), ph = 2 * kPi * r2;
            float3 hm = t * (std::sin(th) * std::cos(ph)) + b * (std::sin(th) * std::sin(ph)) + su.n * std::cos(th);
            wi = Normalize(hm * (2 * Dot(wo, hm)) - wo);
        } else {
            float r1 = Rnd(rng), r2 = Rnd(rng), r = std::sqrt(r1), ph = 2 * kPi * r2;
            wi = t * (r * std::cos(ph)) + b * (r * std::sin(ph)) + su.n * std::sqrt(std::max(0.0f, 1 - r1));
        }
        float pdf;
        float3 f = BsdfEval(su, wo, wi, pSpec, pdf);
        if (pdf <= 1e-8f || Lum(f) <= 0) break;
        beta = Mul(beta, f * (1.0f / pdf));
        lastPdf = pdf;
        specularBounce = false;
        o = su.p + su.ng * 1e-3f * (Dot(wi, su.ng) > 0 ? 1.0f : -1.0f);
        d = wi;
        if (bounce >= 3) {
            float q = std::min(std::max(beta.x, std::max(beta.y, beta.z)), 0.95f);
            if (Rnd(rng) > q) break;
            beta = beta * (1.0f / q);
        }
    }
    return L;
}

// ---------------------------------------------------------------- CPU renderer

void PtCpuRenderer::Start(std::shared_ptr<const PtScene> sc, std::shared_ptr<const PtProjectTextures> tx, const PtSettings& s, const PtCamera& cam, int w,
                          int h) {
    Stop();
    scene = std::move(sc);
    tex = std::move(tx);
    settings = s;
    camera = cam;
    width = w;
    height = h;
    accum.assign((size_t)w * h, float4{0, 0, 0, 0});
    samples = 0;
    fetched = -1;
    maxSamples = s.maxSamples;
    stop = false;
    running = true;
    thread = std::thread([this] { Loop(); });
}

void PtCpuRenderer::Stop() {
    stop = true;
    if (thread.joinable()) thread.join();
    running = false;
}

void PtCpuRenderer::Loop() {
    const int W = width, H = height;
    float3 fwd = Normalize(camera.target - camera.position);
    float3 right = Normalize(Cross(camera.up, fwd)), up = Cross(fwd, right);
    float ty = std::tan(settings.fov * 0.5f), tx = ty * W / H;
    const unsigned nt = std::max(1u, std::thread::hardware_concurrency() - 1);
    std::vector<float4> pass((size_t)W * H);
    while (!stop) {
        if (paused || samples >= maxSamples) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            continue;
        }
        const int frame = samples.load();
        std::atomic<int> nextRow{0};
        std::vector<std::thread> workers;
        for (unsigned k = 0; k < nt; ++k)
            workers.emplace_back([&] {
                for (int y = nextRow++; y < H && !stop; y = nextRow++)
                    for (int x = 0; x < W; ++x) {
                        u32 rng = (u32)(x * 1973 + y * 9277 + frame * 26699) | 1u;
                        Pcg(rng);
                        float px = (x + Rnd(rng)) / W * 2 - 1, py = 1 - (y + Rnd(rng)) / H * 2;
                        float3 dir = Normalize(fwd + right * (px * tx) + up * (py * ty));
                        float3 org = camera.position;
                        if (settings.aperture > 0) {
                            // Thin lens: aperture disc, sharp plane at the focus distance.
                            float3 focus = org + dir * (settings.focusDistance / std::max(Dot(dir, fwd), 1e-4f));
                            float r = std::sqrt(Rnd(rng)) * settings.aperture, ph = 2 * kPi * Rnd(rng);
                            org = org + right * (r * std::cos(ph)) + up * (r * std::sin(ph));
                            dir = Normalize(focus - org);
                        }
                        float3 c = PtTrace(*scene, tex.get(), settings, org, dir, rng);
                        if (!(c.x == c.x) || !(c.y == c.y) || !(c.z == c.z)) c = {0, 0, 0};  // NaN guard
                        pass[(size_t)y * W + x] = {c.x, c.y, c.z, 1};
                    }
            });
        for (auto& t : workers) t.join();
        if (stop) break;
        std::lock_guard lock(mutex);
        for (size_t i = 0; i < pass.size(); ++i) {
            accum[i].x += pass[i].x;
            accum[i].y += pass[i].y;
            accum[i].z += pass[i].z;
            accum[i].w += 1;
        }
        ++samples;
    }
    running = false;
}

bool PtCpuRenderer::Fetch(std::vector<float4>& out, int& samplesOut) {
    std::lock_guard lock(mutex);
    if (samples.load() == fetched || accum.empty()) return false;
    fetched = samples.load();
    out.resize(accum.size());
    for (size_t i = 0; i < accum.size(); ++i) {
        float n = std::max(accum[i].w, 1.0f);
        out[i] = {accum[i].x / n, accum[i].y / n, accum[i].z / n, 1};
    }
    samplesOut = fetched;
    return true;
}

}  // namespace sx
