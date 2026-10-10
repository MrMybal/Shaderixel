// CPU bakers (see MeshBake.h): smart mask maps and the high-to-low baker.
#include "mesh/MeshBake.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <unordered_map>

namespace sx {

namespace {

float3 Min3(float3 a, float3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
float3 Max3(float3 a, float3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
float Comp(float3 v, int k) { return k == 0 ? v.x : k == 1 ? v.y : v.z; }

struct Hit {
    float t = 0;
    u32 tri = 0;
    float u = 0, v = 0;  // barycentrics of vertices 1 and 2
};

// Bounding volume hierarchy over the triangles (median split).
struct Bvh {
    struct Node {
        float3 mn, mx;
        int left = -1, start = 0, count = 0;  // leaf when count > 0
    };
    std::vector<Node> nodes;
    std::vector<u32> tris;
    const std::vector<float3>* P = nullptr;
    const std::vector<u32>* I = nullptr;

    void Build(const std::vector<float3>& pos, const std::vector<u32>& idx) {
        P = &pos;
        I = &idx;
        const size_t n = idx.size() / 3;
        tris.resize(n);
        std::vector<float3> cen(n);
        for (size_t t = 0; t < n; ++t) {
            tris[t] = (u32)t;
            cen[t] = (pos[idx[t * 3]] + pos[idx[t * 3 + 1]] + pos[idx[t * 3 + 2]]) * (1.0f / 3.0f);
        }
        nodes.reserve(n * 2 + 1);
        nodes.push_back({});
        if (n) Split(0, 0, (int)n, cen);
    }
    void Bounds(Node& nd, int s, int c) {
        nd.mn = {1e30f, 1e30f, 1e30f};
        nd.mx = {-1e30f, -1e30f, -1e30f};
        for (int i = s; i < s + c; ++i)
            for (int k = 0; k < 3; ++k) {
                float3 p = (*P)[(*I)[tris[i] * 3 + k]];
                nd.mn = Min3(nd.mn, p);
                nd.mx = Max3(nd.mx, p);
            }
    }
    void Split(int ni, int s, int c, std::vector<float3>& cen) {
        Bounds(nodes[ni], s, c);
        if (c <= 4) { nodes[ni].start = s; nodes[ni].count = c; return; }
        float3 cmn{1e30f, 1e30f, 1e30f}, cmx{-1e30f, -1e30f, -1e30f};
        for (int i = s; i < s + c; ++i) { cmn = Min3(cmn, cen[tris[i]]); cmx = Max3(cmx, cen[tris[i]]); }
        float3 e = cmx - cmn;
        int axis = e.x > e.y && e.x > e.z ? 0 : e.y > e.z ? 1 : 2;
        int mid = s + c / 2;
        std::nth_element(tris.begin() + s, tris.begin() + mid, tris.begin() + s + c,
                         [&](u32 a, u32 b) { return Comp(cen[a], axis) < Comp(cen[b], axis); });
        int l = (int)nodes.size();
        nodes.push_back({});
        nodes.push_back({});
        nodes[ni].left = l;
        Split(l, s, mid - s, cen);
        Split(l + 1, mid, s + c - mid, cen);
    }
    static bool Box(const Node& nd, float3 o, float3 inv, float tmax) {
        float t0 = 0, t1 = tmax;
        for (int k = 0; k < 3; ++k) {
            float a = (Comp(nd.mn, k) - Comp(o, k)) * Comp(inv, k), b = (Comp(nd.mx, k) - Comp(o, k)) * Comp(inv, k);
            if (a > b) std::swap(a, b);
            t0 = std::max(t0, a);
            t1 = std::min(t1, b);
            if (t0 > t1) return false;
        }
        return true;
    }
    bool Tri(u32 t, float3 o, float3 d, float tmax, Hit& h) const {
        float3 a = (*P)[(*I)[t * 3]], b = (*P)[(*I)[t * 3 + 1]], c = (*P)[(*I)[t * 3 + 2]];
        float3 e1 = b - a, e2 = c - a, pv = Cross(d, e2);
        float det = Dot(e1, pv);
        if (std::fabs(det) < 1e-12f) return false;
        float inv = 1.0f / det;
        float3 tv = o - a;
        float u = Dot(tv, pv) * inv;
        if (u < 0 || u > 1) return false;
        float3 qv = Cross(tv, e1);
        float v = Dot(d, qv) * inv;
        if (v < 0 || u + v > 1) return false;
        float th = Dot(e2, qv) * inv;
        if (th <= 1e-5f || th >= tmax) return false;
        h = {th, t, u, v};
        return true;
    }
    // Nearest hit (h.t = tmax when nothing).
    bool Trace(float3 o, float3 d, float tmax, Hit* out = nullptr) const {
        if (nodes.empty() || tris.empty()) return false;
        float3 inv{1.0f / (std::fabs(d.x) > 1e-12f ? d.x : 1e-12f), 1.0f / (std::fabs(d.y) > 1e-12f ? d.y : 1e-12f), 1.0f / (std::fabs(d.z) > 1e-12f ? d.z : 1e-12f)};
        int stack[64];
        int sp = 0;
        stack[sp++] = 0;
        Hit best;
        best.t = tmax;
        bool any = false;
        while (sp) {
            const Node& nd = nodes[stack[--sp]];
            if (!Box(nd, o, inv, best.t)) continue;
            if (nd.count) {
                for (int i = nd.start; i < nd.start + nd.count; ++i) {
                    Hit h;
                    if (Tri(tris[i], o, d, best.t, h)) { best = h; any = true; }
                }
            } else if (sp < 62) {
                stack[sp++] = nd.left;
                stack[sp++] = nd.left + 1;
            }
        }
        if (out) *out = best;
        return any;
    }
    float Distance(float3 o, float3 d, float tmax) const {
        Hit h;
        return Trace(o, d, tmax, &h) ? h.t : tmax;
    }
};

u64 Key3(float3 v) {
    auto q = [](float f) { return (u64)(u32)(i32)std::lround(f * 100000.0f) & 0x1FFFFF; };
    return q(v.x) | (q(v.y) << 21) | (q(v.z) << 42);
}

float SegmentDistance(float3 p, float3 a, float3 b) {
    float3 ab = b - a, ap = p - a;
    float t = std::clamp(Dot(ap, ab) / std::max(Dot(ab, ab), 1e-12f), 0.0f, 1.0f);
    return Length(ap - ab * t);
}

float Hash(u32 x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777216.0f;
}

// A mesh ready for the bakers: welded positions, BVH, outward face normals, sharp edges, smooth curvature.
struct Analyzed {
    const MeshData* m = nullptr;
    std::vector<float3> P;
    std::vector<u32> W;
    std::vector<float3> faceN;
    std::vector<float> vcurv;
    Bvh bvh;
    struct Seg { float3 a, b; };
    std::vector<Seg> convex, concave;
    struct Grid {
        std::unordered_map<u64, std::vector<u32>> cells;
        static u64 Key(int x, int y, int z) { return ((u64)(u32)(x + 100000) << 42) ^ ((u64)(u32)(y + 100000) << 21) ^ (u64)(u32)(z + 100000); }
    } gConvex, gConcave;
    float R = 0.1f;

    void Build(const MeshData& mesh, float sharpAngle, float edgeWidth) {
        m = &mesh;
        const auto& V = mesh.vertices;
        const auto& I = mesh.indices;
        const size_t nt = I.size() / 3;
        std::unordered_map<u64, u32> weld;
        W.resize(I.size());
        for (size_t i = 0; i < I.size(); ++i) {
            auto [it, added] = weld.emplace(Key3(V[I[i]].pos), (u32)P.size());
            if (added) P.push_back(V[I[i]].pos);
            W[i] = it->second;
        }
        faceN.resize(nt);
        for (size_t t = 0; t < nt; ++t) {
            faceN[t] = Normalize(Cross(P[W[t * 3 + 1]] - P[W[t * 3]], P[W[t * 3 + 2]] - P[W[t * 3]]));
            // Outward like the model's normals, whatever the winding of the file.
            float3 vnSum = V[I[t * 3]].normal + V[I[t * 3 + 1]].normal + V[I[t * 3 + 2]].normal;
            if (Dot(faceN[t], vnSum) < 0) faceN[t] = faceN[t] * -1.0f;
        }
        bvh.Build(P, W);
        std::unordered_map<u64, std::vector<u32>> edgeTris;
        for (size_t t = 0; t < nt; ++t)
            for (int k = 0; k < 3; ++k) {
                u32 a = W[t * 3 + k], b = W[t * 3 + (k + 1) % 3];
                if (a > b) std::swap(a, b);
                edgeTris[((u64)a << 32) | b].push_back((u32)t);
            }
        const float sharpCos = std::cos(sharpAngle * 0.0174533f);
        std::vector<u8> sharpVertex(P.size(), 0);
        std::vector<float3> vn(P.size());
        std::vector<std::vector<u32>> nbr(P.size());
        for (size_t t = 0; t < nt; ++t) {
            float area = Length(Cross(P[W[t * 3 + 1]] - P[W[t * 3]], P[W[t * 3 + 2]] - P[W[t * 3]]));
            for (int k = 0; k < 3; ++k) vn[W[t * 3 + k]] = vn[W[t * 3 + k]] + faceN[t] * area;
        }
        for (auto& [key, list] : edgeTris) {
            u32 a = (u32)(key >> 32), b = (u32)(key & 0xFFFFFFFF);
            nbr[a].push_back(b);
            nbr[b].push_back(a);
            if (list.size() < 2) continue;  // open border
            const u32 t0 = list[0], t1 = list[1];
            if (Dot(faceN[t0], faceN[t1]) > sharpCos) continue;
            // Convex when the other face's center is behind the first face's plane.
            float3 c1 = (P[W[t1 * 3]] + P[W[t1 * 3 + 1]] + P[W[t1 * 3 + 2]]) * (1.0f / 3.0f);
            float side = Dot(c1 - P[a], faceN[t0]);
            (side < 0 ? convex : concave).push_back({P[a], P[b]});
            sharpVertex[a] = sharpVertex[b] = 1;
        }
        vcurv.assign(P.size(), 0.0f);
        for (size_t i = 0; i < P.size(); ++i) {
            if (sharpVertex[i] || nbr[i].empty()) continue;
            float3 n = Normalize(vn[i]);
            float s = 0;
            for (u32 j : nbr[i]) s += Dot(n, Normalize(P[j] - P[i]));
            vcurv[i] = -s / nbr[i].size();  // > 0 convex
        }
        R = std::max(edgeWidth, 1e-3f);
        BuildGrid(convex, gConvex);
        BuildGrid(concave, gConcave);
    }
    void BuildGrid(const std::vector<Seg>& segs, Grid& g) {
        for (u32 i = 0; i < segs.size(); ++i) {
            float3 mn = Min3(segs[i].a, segs[i].b) - float3{R, R, R}, mx = Max3(segs[i].a, segs[i].b) + float3{R, R, R};
            int x0 = (int)std::floor(mn.x / R), x1 = (int)std::floor(mx.x / R), y0 = (int)std::floor(mn.y / R), y1 = (int)std::floor(mx.y / R);
            int z0 = (int)std::floor(mn.z / R), z1 = (int)std::floor(mx.z / R);
            if ((i64)(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 20000) continue;  // huge segment (rare): skipped
            for (int x = x0; x <= x1; ++x)
                for (int y = y0; y <= y1; ++y)
                    for (int z = z0; z <= z1; ++z) g.cells[Grid::Key(x, y, z)].push_back(i);
        }
    }
    float Proximity(const Grid& g, const std::vector<Seg>& segs, float3 p) const {
        auto it = g.cells.find(Grid::Key((int)std::floor(p.x / R), (int)std::floor(p.y / R), (int)std::floor(p.z / R)));
        if (it == g.cells.end()) return 0.0f;
        float d = R;
        for (u32 i : it->second) d = std::min(d, SegmentDistance(p, segs[i].a, segs[i].b));
        return 1.0f - d / R;
    }
    // Smooth curvature at a point of triangle t (barycentrics w).
    float Curv(u32 t, float w0, float w1, float w2) const { return vcurv[W[t * 3]] * w0 + vcurv[W[t * 3 + 1]] * w1 + vcurv[W[t * 3 + 2]] * w2; }
    float Convex(float3 p, float curv) const { return std::max(Proximity(gConvex, convex, p), std::clamp(curv * 3.0f, 0.0f, 1.0f)); }
    float Concave(float3 p, float curv) const { return std::max(Proximity(gConcave, concave, p), std::clamp(-curv * 3.0f, 0.0f, 1.0f)); }
    // Interpolated vertex normal, kept on the outer side of the face.
    float3 Normal(u32 t, float w0, float w1, float w2) const {
        const auto& V = m->vertices;
        const auto& I = m->indices;
        float3 n = Normalize(V[I[t * 3]].normal * w0 + V[I[t * 3 + 1]].normal * w1 + V[I[t * 3 + 2]].normal * w2);
        return Dot(n, faceN[t]) < 0.2f ? faceN[t] : n;
    }
};

struct Dirs {
    std::vector<float3> d;
    explicit Dirs(int n) {
        // Hemisphere directions (cosine-weighted, Fibonacci), turned per texel.
        n = std::max(4, n);
        for (int i = 0; i < n; ++i) {
            float u = (i + 0.5f) / n, phi = i * 2.3999632f;
            float r = std::sqrt(u), z = std::sqrt(1 - u);
            d.push_back({r * std::cos(phi), r * std::sin(phi), z});
        }
    }
};

// Occlusion (1 = open) and thickness (1 = thick) at p (normal n) against the analyzed mesh.
void OcclusionThickness(const Analyzed& A, const Dirs& dirs, float3 p, float3 n, u32 seed, float aoDist, float& ao, float& thick) {
    float3 t = Normalize(Cross(std::fabs(n.y) < 0.9f ? float3{0, 1, 0} : float3{1, 0, 0}, n));
    float3 b = Cross(n, t);
    const float rot = Hash(seed) * 6.2831853f, cr = std::cos(rot), sr = std::sin(rot);
    float occl = 0, th = 0;
    int nth = 0;
    const float3 o0 = p + n * 0.002f, o1 = p - n * 0.002f;
    const int nr = (int)dirs.d.size();
    for (int k = 0; k < nr; ++k) {
        float3 d = dirs.d[k];
        float dx = d.x * cr - d.y * sr, dy = d.x * sr + d.y * cr;
        float3 w = t * dx + b * dy + n * d.z;
        float hit = A.bvh.Distance(o0, w, aoDist);
        if (hit < aoDist) occl += 1.0f - hit / aoDist;
        if (k % 3 == 0) {
            float3 wi = t * dx * 0.5f + b * dy * 0.5f - n * d.z;
            th += std::min(A.bvh.Distance(o1, Normalize(wi), 1.0f), 1.0f);
            ++nth;
        }
    }
    ao = 1.0f - occl / nr;
    thick = std::clamp(th / std::max(nth, 1) * 2.0f, 0.0f, 1.0f);
}

// Texels covered by the low poly's UVs.
struct Texel {
    u32 idx;
    u32 tri;
    float w0, w1, w2;
    float3 p, n;
};

std::vector<Texel> RasterUv(const Analyzed& A, int S, std::vector<u8>& filled) {
    const auto& V = A.m->vertices;
    const auto& I = A.m->indices;
    std::vector<Texel> texels;
    filled.assign((size_t)S * S, 0);
    for (size_t t = 0; t < I.size() / 3; ++t) {
        float2 uv[3];
        for (int k = 0; k < 3; ++k) uv[k] = {V[I[t * 3 + k]].uv.x * S, V[I[t * 3 + k]].uv.y * S};
        const float area = (uv[1].x - uv[0].x) * (uv[2].y - uv[0].y) - (uv[1].y - uv[0].y) * (uv[2].x - uv[0].x);
        if (std::fabs(area) < 1e-9f) continue;
        int x0 = std::max(0, (int)std::floor(std::min({uv[0].x, uv[1].x, uv[2].x}))), x1 = std::min(S - 1, (int)std::ceil(std::max({uv[0].x, uv[1].x, uv[2].x})));
        int y0 = std::max(0, (int)std::floor(std::min({uv[0].y, uv[1].y, uv[2].y}))), y1 = std::min(S - 1, (int)std::ceil(std::max({uv[0].y, uv[1].y, uv[2].y})));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float px = x + 0.5f, py = y + 0.5f;
                float w0 = ((uv[1].x - px) * (uv[2].y - py) - (uv[1].y - py) * (uv[2].x - px)) / area;
                float w1 = ((uv[2].x - px) * (uv[0].y - py) - (uv[2].y - py) * (uv[0].x - px)) / area;
                float w2 = 1 - w0 - w1;
                const float eps = -0.02f;  // a little past the edges
                if (w0 < eps || w1 < eps || w2 < eps) continue;
                const size_t idx = (size_t)y * S + x;
                if (filled[idx]) continue;
                w0 = std::max(w0, 0.0f), w1 = std::max(w1, 0.0f), w2 = std::max(w2, 0.0f);
                const float sum = w0 + w1 + w2;
                w0 /= sum, w1 /= sum, w2 /= sum;
                Texel tx;
                tx.idx = (u32)idx;
                tx.tri = (u32)t;
                tx.w0 = w0, tx.w1 = w1, tx.w2 = w2;
                tx.p = V[I[t * 3]].pos * w0 + V[I[t * 3 + 1]].pos * w1 + V[I[t * 3 + 2]].pos * w2;
                tx.n = A.Normal((u32)t, w0, w1, w2);
                texels.push_back(tx);
                filled[idx] = 1;
            }
    }
    return texels;
}

// Per-vertex tangent frames from the UVs (accumulated per triangle, like MikkTSpace without the splits).
void Tangents(const MeshData& m, std::vector<float3>& T, std::vector<float3>& B) {
    const auto& V = m.vertices;
    const auto& I = m.indices;
    T.assign(V.size(), {});
    B.assign(V.size(), {});
    for (size_t t = 0; t + 2 < I.size(); t += 3) {
        const MeshVertex &a = V[I[t]], &b = V[I[t + 1]], &c = V[I[t + 2]];
        float3 e1 = b.pos - a.pos, e2 = c.pos - a.pos;
        float du1 = b.uv.x - a.uv.x, dv1 = b.uv.y - a.uv.y, du2 = c.uv.x - a.uv.x, dv2 = c.uv.y - a.uv.y;
        float det = du1 * dv2 - du2 * dv1;
        if (std::fabs(det) < 1e-12f) continue;
        float r = 1.0f / det;
        float3 tt = (e1 * dv2 - e2 * dv1) * r, bb = (e2 * du1 - e1 * du2) * r;
        for (int k = 0; k < 3; ++k) {
            T[I[t + k]] = T[I[t + k]] + tt;
            B[I[t + k]] = B[I[t + k]] + bb;
        }
    }
}

void Dilate(std::vector<float>& img, int channels, int S, std::vector<u8> filled, int passes) {
    for (int pass = 0; pass < passes; ++pass) {
        std::vector<u8> nf = filled;
        bool any = false;
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x) {
                size_t idx = (size_t)y * S + x;
                if (filled[idx]) continue;
                float acc[4] = {};
                int c = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= S || yy >= S || !filled[(size_t)yy * S + xx]) continue;
                        for (int k = 0; k < channels; ++k) acc[k] += img[((size_t)yy * S + xx) * channels + k];
                        ++c;
                    }
                if (!c) continue;
                for (int k = 0; k < channels; ++k) img[idx * channels + k] = acc[k] / c;
                nf[idx] = 1;
                any = true;
            }
        filled.swap(nf);
        if (!any) break;
    }
}

void Denoise3x3(std::vector<float>& img, int channels, int k, int S, const std::vector<u8>& filled, const std::vector<Texel>& texels) {
    std::vector<float> src(img);
    for (const Texel& tx : texels) {
        const int x = (int)(tx.idx % S), y = (int)(tx.idx / S);
        float acc = 0;
        int c = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                int xx = x + dx, yy = y + dy;
                if (xx < 0 || yy < 0 || xx >= S || yy >= S || !filled[(size_t)yy * S + xx]) continue;
                acc += src[((size_t)yy * S + xx) * channels + k];
                ++c;
            }
        if (c) img[(size_t)tx.idx * channels + k] = acc / c;
    }
}

template <class F>
void ParallelFor(size_t n, std::atomic<float>* progress, std::atomic<bool>* cancel, float from, float to, F&& f) {
    std::atomic<size_t> next{0}, done{0};
    auto work = [&] {
        const size_t chunk = 256;
        for (;;) {
            if (cancel && cancel->load()) return;
            size_t s0 = next.fetch_add(chunk);
            if (s0 >= n) return;
            size_t s1 = std::min(n, s0 + chunk);
            for (size_t i = s0; i < s1; ++i) f(i);
            size_t d = done.fetch_add(s1 - s0) + (s1 - s0);
            if (progress) progress->store(from + (to - from) * (float)d / n);
        }
    };
    const unsigned nth = std::max(1u, std::min(32u, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    for (unsigned i = 0; i < nth; ++i) pool.emplace_back(work);
    for (auto& th : pool) th.join();
}

}  // namespace

struct MeshRaycaster::Impl {
    MeshData mesh;
    Analyzed a;
};

void MeshRaycaster::Build(const MeshData& m) {
    auto i = std::make_shared<Impl>();
    i->mesh = m;
    i->a.Build(i->mesh, 25.0f, 0.05f);
    impl = i;
}

bool MeshRaycaster::Ray(float3 o, float3 d, float tmax, float3* pos, float3* normal, float* tOut) const {
    if (!impl) return false;
    Hit h;
    if (!impl->a.bvh.Trace(o, d, tmax, &h)) return false;
    if (pos) *pos = o + d * h.t;
    if (normal) *normal = impl->a.Normal(h.tri, 1 - h.u - h.v, h.u, h.v);
    if (tOut) *tOut = h.t;
    return true;
}

void TransformToMeshSpace(MeshData& m, const MeshData& ref) {
    // m: p_m = (orig - cm) * sm  ->  ref: p_r = (orig - cr) * sr
    const float k = ref.srcScale / std::max(m.srcScale, 1e-12f);
    for (auto& v : m.vertices) {
        float3 orig = v.pos * (1.0f / std::max(m.srcScale, 1e-12f)) + m.srcCenter;
        v.pos = (orig - ref.srcCenter) * ref.srcScale;
    }
    (void)k;
    m.srcCenter = ref.srcCenter;
    m.srcScale = ref.srcScale;
    m.boundsMin = {1e30f, 1e30f, 1e30f};
    m.boundsMax = {-1e30f, -1e30f, -1e30f};
    for (auto& v : m.vertices) {
        m.boundsMin = Min3(m.boundsMin, v.pos);
        m.boundsMax = Max3(m.boundsMax, v.pos);
    }
}

BakeResult BakeMeshes(const MeshData& low, const MeshData* high, const BakeOptions& o, std::atomic<float>* progress, std::atomic<bool>* cancel) {
    BakeResult out;
    const int S = std::clamp(o.size, 64, 8192);
    if (low.indices.empty()) { out.error = "maillage vide"; return out; }
    if (high && high->indices.empty()) high = nullptr;
    Analyzed L;
    L.Build(low, o.sharpAngle, o.edgeWidth);
    Analyzed H;
    if (high) H.Build(*high, o.sharpAngle, o.edgeWidth);
    const Analyzed& D = high ? H : L;  // the detail
    std::vector<u8> filled;
    std::vector<Texel> texels = RasterUv(L, S, filled);
    if (texels.empty()) { out.error = "pas de coordonnées UV"; return out; }
    out.texels = texels.size();
    if (progress) progress->store(0.02f);
    std::vector<float3> TV, BV;
    if (o.normal) Tangents(low, TV, BV);
    const size_t N = (size_t)S * S;
    out.maps.assign(N * 4, 0.0f);
    if (o.normal) out.normal.assign(N * 3, 0.5f);
    if (o.ao) out.ao.assign(N, 1.0f);
    if (o.curvature) out.curvature.assign(N, 0.5f);
    if (o.thickness) out.thickness.assign(N, 1.0f);
    if (o.height) out.height.assign(N, 0.5f);
    if (o.position) out.position.assign(N * 3, 0.0f);
    if (o.worldNormal) out.worldNormal.assign(N * 3, 0.5f);
    const float3 bmin = low.boundsMin, bext = Max3(low.boundsMax - low.boundsMin, float3{1e-6f, 1e-6f, 1e-6f});
    const Dirs dirs(o.aoRays);
    std::atomic<size_t> misses{0};
    const float span = std::max(o.front + o.back, 1e-5f);
    const auto& LV = low.vertices;
    const auto& LI = low.indices;
    ParallelFor(texels.size(), progress, cancel, 0.02f, 0.95f, [&](size_t i) {
        const Texel& tx = texels[i];
        float3 p = tx.p, n = tx.n;
        float offset = 0;
        u32 dtri = tx.tri;
        float dw0 = tx.w0, dw1 = tx.w1, dw2 = tx.w2;
        if (high) {
            // Ray from the cage (above the low surface) down to `back` below it: the first high poly surface.
            const float3 origin = tx.p + tx.n * o.front;
            Hit h;
            if (H.bvh.Trace(origin, tx.n * -1.0f, span, &h)) {
                p = origin - tx.n * h.t;
                dtri = h.tri;
                dw0 = 1 - h.u - h.v, dw1 = h.u, dw2 = h.v;
                n = H.Normal(dtri, dw0, dw1, dw2);
                offset = o.front - h.t;
            } else {
                misses.fetch_add(1);
                dtri = ~0u;
            }
        }
        const bool onDetail = !high || dtri != ~0u;
        const Analyzed& A = onDetail ? D : L;
        if (!onDetail) { dtri = tx.tri; dw0 = tx.w0, dw1 = tx.w1, dw2 = tx.w2; }
        float ao = 1, th = 1;
        OcclusionThickness(A, dirs, p, n, tx.idx, o.aoDistance, ao, th);
        const float curv = A.Curv(dtri, dw0, dw1, dw2);
        const float cv = A.Convex(p, curv), cc = A.Concave(p, curv);
        float* m4 = &out.maps[(size_t)tx.idx * 4];
        m4[0] = ao, m4[1] = cv, m4[2] = cc, m4[3] = th;
        if (o.ao) out.ao[tx.idx] = ao;
        if (o.curvature) out.curvature[tx.idx] = std::clamp(0.5f + 0.5f * (cv - cc), 0.0f, 1.0f);
        if (o.thickness) out.thickness[tx.idx] = th;
        if (o.height) out.height[tx.idx] = std::clamp(0.5f + offset / (2.0f * std::max(o.front, o.back)), 0.0f, 1.0f);
        if (o.position) {
            float3 q = p - bmin;
            out.position[(size_t)tx.idx * 3] = q.x / bext.x;
            out.position[(size_t)tx.idx * 3 + 1] = q.y / bext.y;
            out.position[(size_t)tx.idx * 3 + 2] = q.z / bext.z;
        }
        if (o.worldNormal)
            for (int k = 0; k < 3; ++k) out.worldNormal[(size_t)tx.idx * 3 + k] = Comp(n, k) * 0.5f + 0.5f;
        if (o.normal) {
            // Tangent frame of the low poly at the texel (X = +U, Y = +V = image down: DirectX / Unreal convention).
            const u32 a = LI[tx.tri * 3], b = LI[tx.tri * 3 + 1], c = LI[tx.tri * 3 + 2];
            float3 T = TV[a] * tx.w0 + TV[b] * tx.w1 + TV[c] * tx.w2, B = BV[a] * tx.w0 + BV[b] * tx.w1 + BV[c] * tx.w2;
            const float3 ln = tx.n;
            T = Normalize(T - ln * Dot(ln, T));
            B = B - ln * Dot(ln, B);
            B = Normalize(B - T * Dot(T, B));
            float3 ts{Dot(n, T), Dot(n, B), Dot(n, ln)};
            ts = Normalize(ts);
            float* px = &out.normal[(size_t)tx.idx * 3];
            px[0] = ts.x * 0.5f + 0.5f, px[1] = ts.y * 0.5f + 0.5f, px[2] = ts.z * 0.5f + 0.5f;
        }
        (void)LV;
    });
    if (cancel && cancel->load()) { out.error = "annulé"; return out; }
    out.misses = misses.load();
    // Ray noise of the occlusion, then padding past the islands.
    Denoise3x3(out.maps, 4, 0, S, filled, texels);
    if (o.ao) Denoise3x3(out.ao, 1, 0, S, filled, texels);
    const int pad = std::max(o.padding, 2);
    Dilate(out.maps, 4, S, filled, pad);
    if (o.normal) Dilate(out.normal, 3, S, filled, pad);
    if (o.ao) Dilate(out.ao, 1, S, filled, pad);
    if (o.curvature) Dilate(out.curvature, 1, S, filled, pad);
    if (o.thickness) Dilate(out.thickness, 1, S, filled, pad);
    if (o.height) Dilate(out.height, 1, S, filled, pad);
    if (o.position) Dilate(out.position, 3, S, filled, pad);
    if (o.worldNormal) Dilate(out.worldNormal, 3, S, filled, pad);
    // Far from the layout: neutral values (open, thick) in the smart mask maps.
    {
        std::vector<u8> f2 = filled;
        for (size_t i = 0; i < N; ++i)
            if (out.maps[i * 4] == 0 && out.maps[i * 4 + 3] == 0 && !f2[i]) out.maps[i * 4] = 1.0f, out.maps[i * 4 + 3] = 1.0f;
    }
    out.size = S;
    out.ok = true;
    if (progress) progress->store(1.0f);
    return out;
}

MeshMaps BakeMeshMaps(const MeshData& mesh, const MeshMapsOptions& o, std::atomic<float>* progress, std::atomic<bool>* cancel) {
    BakeOptions b;
    b.size = o.size;
    b.aoRays = o.aoRays;
    b.aoDistance = o.aoDistance;
    b.edgeWidth = o.edgeWidth;
    b.sharpAngle = o.sharpAngle;
    b.normal = b.ao = b.curvature = b.thickness = b.height = b.position = b.worldNormal = false;
    b.padding = 8;
    BakeResult r = BakeMeshes(mesh, nullptr, b, progress, cancel);
    MeshMaps m;
    m.ok = r.ok;
    m.error = r.error;
    m.size = r.size;
    m.rgba = std::move(r.maps);
    return m;
}

}  // namespace sx
