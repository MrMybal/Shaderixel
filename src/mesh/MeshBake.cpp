// CPU baker of the mesh maps used by the smart masks (see MeshBake.h).
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

// Bounding volume hierarchy over the triangles (median split), for occlusion and thickness rays.
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
        Split(0, 0, (int)n, cen);
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
    bool Tri(u32 t, float3 o, float3 d, float tmax, float& tHit) const {
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
        tHit = th;
        return true;
    }
    // Nearest hit distance (or tmax when nothing).
    float Trace(float3 o, float3 d, float tmax) const {
        float3 inv{1.0f / (std::fabs(d.x) > 1e-12f ? d.x : 1e-12f), 1.0f / (std::fabs(d.y) > 1e-12f ? d.y : 1e-12f), 1.0f / (std::fabs(d.z) > 1e-12f ? d.z : 1e-12f)};
        int stack[64];
        int sp = 0;
        stack[sp++] = 0;
        float best = tmax;
        while (sp) {
            const Node& nd = nodes[stack[--sp]];
            if (!Box(nd, o, inv, best)) continue;
            if (nd.count) {
                for (int i = nd.start; i < nd.start + nd.count; ++i) {
                    float t;
                    if (Tri(tris[i], o, d, best, t)) best = t;
                }
            } else if (sp < 62) {
                stack[sp++] = nd.left;
                stack[sp++] = nd.left + 1;
            }
        }
        return best;
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

}  // namespace

MeshMaps BakeMeshMaps(const MeshData& mesh, const MeshMapsOptions& o, std::atomic<float>* progress, std::atomic<bool>* cancel) {
    MeshMaps out;
    const int S = std::clamp(o.size, 64, 4096);
    const auto& V = mesh.vertices;
    const auto& I = mesh.indices;
    const size_t nt = I.size() / 3;
    if (nt == 0) { out.error = "maillage vide"; return out; }

    // Welded positions: shared by the BVH, the edges and the vertex curvature.
    std::unordered_map<u64, u32> weld;
    std::vector<float3> P;
    std::vector<u32> W(I.size());
    for (size_t i = 0; i < I.size(); ++i) {
        auto [it, added] = weld.emplace(Key3(V[I[i]].pos), (u32)P.size());
        if (added) P.push_back(V[I[i]].pos);
        W[i] = it->second;
    }
    std::vector<float3> faceN(nt);
    for (size_t t = 0; t < nt; ++t) faceN[t] = Normalize(Cross(P[W[t * 3 + 1]] - P[W[t * 3]], P[W[t * 3 + 2]] - P[W[t * 3]]));
    Bvh bvh;
    bvh.Build(P, W);

    // Sharp edges (convex / concave) and the smooth curvature of the other vertices.
    std::unordered_map<u64, std::vector<u32>> edgeTris;
    for (size_t t = 0; t < nt; ++t)
        for (int k = 0; k < 3; ++k) {
            u32 a = W[t * 3 + k], b = W[t * 3 + (k + 1) % 3];
            if (a > b) std::swap(a, b);
            edgeTris[((u64)a << 32) | b].push_back((u32)t);
        }
    const float sharpCos = std::cos(o.sharpAngle * 0.0174533f);
    struct Seg { float3 a, b; };
    std::vector<Seg> convex, concave;
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
    std::vector<float> vcurv(P.size(), 0.0f);
    for (size_t i = 0; i < P.size(); ++i) {
        if (sharpVertex[i] || nbr[i].empty()) continue;
        float3 n = Normalize(vn[i]);
        float s = 0;
        for (u32 j : nbr[i]) s += Dot(n, Normalize(P[j] - P[i]));
        vcurv[i] = -s / nbr[i].size();  // > 0 convex
    }
    // Grid of the sharp edges (cell = reach).
    const float R = std::max(o.edgeWidth, 1e-3f);
    struct Grid {
        float cell;
        std::unordered_map<u64, std::vector<u32>> cells;
        u64 Key(int x, int y, int z) const { return ((u64)(u32)(x + 100000) << 42) ^ ((u64)(u32)(y + 100000) << 21) ^ (u64)(u32)(z + 100000); }
    };
    auto buildGrid = [&](const std::vector<Seg>& segs) {
        Grid g{R, {}};
        for (u32 i = 0; i < segs.size(); ++i) {
            float3 mn = Min3(segs[i].a, segs[i].b) - float3{R, R, R}, mx = Max3(segs[i].a, segs[i].b) + float3{R, R, R};
            int x0 = (int)std::floor(mn.x / R), x1 = (int)std::floor(mx.x / R), y0 = (int)std::floor(mn.y / R), y1 = (int)std::floor(mx.y / R);
            int z0 = (int)std::floor(mn.z / R), z1 = (int)std::floor(mx.z / R);
            if ((i64)(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 20000) continue;  // huge segment (rare): skipped
            for (int x = x0; x <= x1; ++x)
                for (int y = y0; y <= y1; ++y)
                    for (int z = z0; z <= z1; ++z) g.cells[g.Key(x, y, z)].push_back(i);
        }
        return g;
    };
    Grid gConvex = buildGrid(convex), gConcave = buildGrid(concave);
    auto proximity = [&](const Grid& g, const std::vector<Seg>& segs, float3 p) {
        auto it = g.cells.find(g.Key((int)std::floor(p.x / R), (int)std::floor(p.y / R), (int)std::floor(p.z / R)));
        if (it == g.cells.end()) return 0.0f;
        float d = R;
        for (u32 i : it->second) d = std::min(d, SegmentDistance(p, segs[i].a, segs[i].b));
        return 1.0f - d / R;
    };

    // Texels covered by the UV layout: position, normal, smooth curvature.
    struct Texel { u32 idx; float3 p, n; float curv; };
    std::vector<Texel> texels;
    std::vector<u8> filled((size_t)S * S, 0);
    for (size_t t = 0; t < nt; ++t) {
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
                tx.p = V[I[t * 3]].pos * w0 + V[I[t * 3 + 1]].pos * w1 + V[I[t * 3 + 2]].pos * w2;
                tx.n = Normalize(V[I[t * 3]].normal * w0 + V[I[t * 3 + 1]].normal * w1 + V[I[t * 3 + 2]].normal * w2);
                if (Dot(tx.n, faceN[t]) < 0.2f) tx.n = faceN[t];
                tx.curv = vcurv[W[t * 3]] * w0 + vcurv[W[t * 3 + 1]] * w1 + vcurv[W[t * 3 + 2]] * w2;
                texels.push_back(tx);
                filled[idx] = 1;
            }
    }
    if (texels.empty()) { out.error = "pas de coordonnées UV"; return out; }

    // Hemisphere directions (cosine-weighted, Fibonacci), turned per texel.
    std::vector<float3> dirs;
    const int nr = std::max(4, o.aoRays);
    for (int i = 0; i < nr; ++i) {
        float u = (i + 0.5f) / nr, phi = i * 2.3999632f;
        float r = std::sqrt(u), z = std::sqrt(1 - u);
        dirs.push_back({r * std::cos(phi), r * std::sin(phi), z});
    }
    out.rgba.assign((size_t)S * S * 4, 0.0f);
    std::atomic<size_t> next{0}, done{0};
    auto work = [&] {
        const size_t chunk = 256;
        for (;;) {
            if (cancel && cancel->load()) return;
            size_t s0 = next.fetch_add(chunk);
            if (s0 >= texels.size()) return;
            size_t s1 = std::min(texels.size(), s0 + chunk);
            for (size_t i = s0; i < s1; ++i) {
                const Texel& tx = texels[i];
                float3 n = tx.n;
                float3 t = Normalize(Cross(std::fabs(n.y) < 0.9f ? float3{0, 1, 0} : float3{1, 0, 0}, n));
                float3 b = Cross(n, t);
                const float rot = Hash(tx.idx) * 6.2831853f, cr = std::cos(rot), sr = std::sin(rot);
                float occl = 0, thick = 0;
                const float3 o0 = tx.p + n * 0.002f, o1 = tx.p - n * 0.002f;
                for (int k = 0; k < nr; ++k) {
                    float3 d = dirs[k];
                    float dx = d.x * cr - d.y * sr, dy = d.x * sr + d.y * cr;
                    float3 w = t * dx + b * dy + n * d.z;
                    float hit = bvh.Trace(o0, w, o.aoDistance);
                    if (hit < o.aoDistance) occl += 1.0f - hit / o.aoDistance;
                    if (k % 3 == 0) {
                        // Thickness: inside, against the normal.
                        float3 wi = t * dx * 0.5f + b * dy * 0.5f - n * d.z;
                        float h2 = bvh.Trace(o1, Normalize(wi), 1.0f);
                        thick += std::min(h2, 1.0f);
                    }
                }
                const float ao = 1.0f - occl / nr;
                const float th = std::clamp(thick / ((nr + 2) / 3) * 2.0f, 0.0f, 1.0f);
                const float cv = std::max(proximity(gConvex, convex, tx.p), std::clamp(tx.curv * 3.0f, 0.0f, 1.0f));
                const float cc = std::max(proximity(gConcave, concave, tx.p), std::clamp(-tx.curv * 3.0f, 0.0f, 1.0f));
                float* px = &out.rgba[(size_t)tx.idx * 4];
                px[0] = ao;
                px[1] = cv;
                px[2] = cc;
                px[3] = th;
            }
            size_t d = done.fetch_add(s1 - s0) + (s1 - s0);
            if (progress) progress->store((float)d / texels.size() * 0.95f);
        }
    };
    const unsigned nth = std::max(1u, std::min(32u, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    for (unsigned i = 0; i < nth; ++i) pool.emplace_back(work);
    for (auto& th : pool) th.join();
    if (cancel && cancel->load()) { out.error = "annulé"; return out; }
    // Padding: empty texels take the mean of their filled neighbors (seams sample a little outside).
    for (int pass = 0; pass < 8; ++pass) {
        std::vector<u8> nf = filled;
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
                        for (int k = 0; k < 4; ++k) acc[k] += out.rgba[((size_t)yy * S + xx) * 4 + k];
                        ++c;
                    }
                if (!c) continue;
                for (int k = 0; k < 4; ++k) out.rgba[idx * 4 + k] = acc[k] / c;
                nf[idx] = 1;
            }
        filled.swap(nf);
    }
    for (size_t i = 0; i < filled.size(); ++i)
        if (!filled[i]) out.rgba[i * 4] = 1.0f, out.rgba[i * 4 + 3] = 1.0f;  // far from the layout: open, thick
    out.size = S;
    out.ok = true;
    if (progress) progress->store(1.0f);
    return out;
}

}  // namespace sx
