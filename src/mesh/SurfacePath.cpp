#include "mesh/SurfacePath.h"
#include <map>
#include <queue>
#include <unordered_map>

namespace sx {
float3 SurfacePathTopology::Position(const MeshData& m, SurfaceAnchor p) {
    const u32* i = &m.indices[(size_t)p.triangle * 3];
    return m.vertices[i[0]].pos * p.weights.x + m.vertices[i[1]].pos * p.weights.y + m.vertices[i[2]].pos * p.weights.z;
}

void SurfacePathTopology::Build(const MeshData& m) {
    faces.assign(m.indices.size() / 3, {});
    std::map<std::array<int, 3>, u32> weldedIds;
    std::vector<u32> weld(m.vertices.size());
    for (size_t i = 0; i < weld.size(); ++i) {
        const float3 p = m.vertices[i].pos;
        const std::array<int, 3> key{(int)std::lround(p.x * 100000), (int)std::lround(p.y * 100000), (int)std::lround(p.z * 100000)};
        weld[i] = weldedIds.emplace(key, (u32)weldedIds.size()).first->second;
    }
    std::unordered_map<u64, std::pair<int, int>> edgeOwner;
    for (int t = 0; t < (int)faces.size(); ++t) {
        const u32* i = &m.indices[(size_t)t * 3];
        const float3 a = m.vertices[i[0]].pos, b = m.vertices[i[1]].pos, c = m.vertices[i[2]].pos;
        faces[t].center = (a + b + c) * (1.0f / 3.0f);
        faces[t].normal = Normalize(Cross(b - a, c - a));
        if (Dot(faces[t].normal, m.vertices[i[0]].normal + m.vertices[i[1]].normal + m.vertices[i[2]].normal) < 0)
            faces[t].normal = faces[t].normal * -1;
        for (int e = 0; e < 3; ++e) {
            auto v = weld[i[e]], w = weld[i[(e + 1) % 3]];
            const u64 key = ((u64)std::min(v, w) << 32) | std::max(v, w);
            auto [it, added] = edgeOwner.emplace(key, std::pair{t, e});
            if (!added) {
                auto [n, f] = it->second;
                if (faces[n].neighbors[f] < 0) {
                    faces[t].neighbors[e] = n;
                    faces[n].neighbors[f] = t;
                }
            }
        }
    }
}

bool SurfacePathTopology::Connect(const MeshData& m, SurfaceAnchor a, SurfaceAnchor b, float& length,
                                  std::vector<SurfaceLineSegment>& segments, std::vector<u32>* owners) const {
    if (a.triangle >= faces.size() || b.triangle >= faces.size()) return false;
    struct Crossing {
        float3 lo, hi, point;
        int face;
    };
    const float3 first = Position(m, a), last = Position(m, b);
    std::vector<Crossing> crossings;
    if (a.triangle != b.triangle) {
        // A* through neighboring triangles. A straight-line lower bound keeps distant faces out of the search.
        using QueueEntry = std::pair<float, u32>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
        std::vector<float> cost(faces.size(), std::numeric_limits<float>::infinity());
        std::vector<int> previous(faces.size(), -1), edge(faces.size(), -1);
        cost[a.triangle] = 0;
        queue.emplace(0.0f, a.triangle);
        while (!queue.empty()) {
            auto [priority, t] = queue.top();
            queue.pop();
            if (t == b.triangle) break;
            if (priority > cost[t] + Length(faces[t].center - faces[b.triangle].center) + 0.00001f) continue;
            for (int e = 0; e < 3; ++e) {
                int n = faces[t].neighbors[e];
                if (n < 0) continue;
                float next = cost[t] + Length(faces[t].center - faces[n].center);
                if (next >= cost[n]) continue;
                cost[n] = next;
                previous[n] = (int)t;
                edge[n] = e;
                queue.emplace(next + Length(faces[n].center - faces[b.triangle].center), (u32)n);
            }
        }
        if (previous[b.triangle] < 0) return false;
        for (int t = (int)b.triangle; t != (int)a.triangle; t = previous[t]) {
            int parent = previous[t], e = edge[t];
            const u32* idx = &m.indices[(size_t)parent * 3];
            float3 lo = m.vertices[idx[e]].pos, hi = m.vertices[idx[(e + 1) % 3]].pos;
            crossings.push_back({lo, hi, (lo + hi) * 0.5f, parent});
        }
        std::reverse(crossings.begin(), crossings.end());
        // Tighten the line within the face strip: every crossing remains on its shared edge.
        for (int pass = 0; pass < 5; ++pass)
            for (size_t k = 0; k < crossings.size(); ++k) {
                auto& c = crossings[k];
                float3 p = k ? crossings[k - 1].point : first, q = k + 1 < crossings.size() ? crossings[k + 1].point : last;
                float3 axis = c.hi - c.lo;
                float scale = Length(axis);
                if (scale < 1e-9f) continue;
                axis = axis * (1 / scale);
                float tp = Dot(p - c.lo, axis), tq = Dot(q - c.lo, axis);
                float dp = Length(p - (c.lo + axis * tp)), dq = Length(q - (c.lo + axis * tq));
                float t = dp + dq > 1e-9f ? (tp * dq + tq * dp) / (dp + dq) : (tp + tq) * 0.5f;
                c.point = c.lo + axis * std::clamp(t, 0.0f, scale);
            }
    }
    float3 previous = first;
    auto append = [&](float3 pos, int face) {
        float start = length;
        length += Length(pos - previous);
        float3 n = faces[face].normal;
        segments.push_back({{previous.x, previous.y, previous.z, start}, {pos.x, pos.y, pos.z, length}, {n.x, n.y, n.z, 0}});
        if (owners) owners->push_back((u32)face);
        previous = pos;
    };
    for (const auto& c : crossings)
        append(c.point, c.face);
    append(last, (int)b.triangle);
    return true;
}
namespace {
float Wedge(float2 a, float2 b) { return a.x * b.y - a.y * b.x; }
float2 FaceCoords(const MeshData& m, u32 face, float3 p) {
    const auto* ix = &m.indices[(size_t)face * 3];
    const float3 a = m.vertices[ix[0]].pos, b = m.vertices[ix[1]].pos - a, c = m.vertices[ix[2]].pos - a;
    const float3 n = Cross(b, c);
    const float scale = std::max(Dot(n, n), 1e-24f);
    return {Dot(Cross(p - a, c), n) / scale, Dot(Cross(b, p - a), n) / scale};
}
struct Cut {
    float2 a, b;
};
const float2 faceRef{0.32115f, 0.28731f};
int Crossings(const std::vector<Cut>& cuts, float2 to) {
    const float2 dir = to - faceRef;
    int parity = 0;
    for (const Cut& cut : cuts) {
        const auto a = cut.a - faceRef, b = cut.b - faceRef;
        if ((Wedge(dir, a) > 0) == (Wedge(dir, b) > 0)) continue;
        const float t = Wedge(a, b - a) / Wedge(dir, b - a);
        if (t >= 0 && t < 1) parity ^= 1;
    }
    return parity;
}
} // namespace

bool SurfacePathTopology::Fill(const MeshData& mesh, const std::vector<SurfaceLineSegment>& segments, const std::vector<u32>& owners,
                               int width, int height, std::vector<u8>& mask) const {
    if (owners.empty() || owners.size() != segments.size() || width <= 0 || height <= 0) return false;
    std::vector<std::vector<Cut>> cuts(faces.size());
    for (size_t i = 0; i < segments.size(); ++i) {
        auto t = owners[i];
        if (t >= faces.size()) return false;
        auto s = segments[i];
        cuts[t].push_back({FaceCoords(mesh, t, {s.a.x, s.a.y, s.a.z}), FaceCoords(mesh, t, {s.b.x, s.b.y, s.b.z})});
    }
    // Transfer even/odd containment between adjacent triangles, including welded UV seams.
    // Each shared-edge portal is evaluated from both face interiors. Unconnected shells stay outside.
    std::vector<int8_t> side(faces.size(), -1);
    std::vector<u32> queue{owners.front()};
    side[queue.front()] = 0;
    double area[2] = {}, boundary[2] = {};
    for (size_t item = 0; item < queue.size(); ++item) {
        u32 t = queue[item];
        const auto* ix = &mesh.indices[(size_t)t * 3];
        auto a = mesh.vertices[ix[0]].pos, b = mesh.vertices[ix[1]].pos, c = mesh.vertices[ix[2]].pos;
        const double faceArea = Length(Cross(b - a, c - a));
        for (float2 probe : {float2{0.15f, 0.15f}, float2{0.70f, 0.15f}, float2{0.15f, 0.70f}, faceRef})
            area[side[t] ^ Crossings(cuts[t], probe)] += faceArea * 0.25;
        for (int edge = 0; edge < 3; ++edge) {
            int neighbor = faces[t].neighbors[edge];
            auto l = mesh.vertices[ix[edge]].pos, r = mesh.vertices[ix[(edge + 1) % 3]].pos;
            if (std::array{l.x, l.y, l.z} > std::array{r.x, r.y, r.z}) std::swap(l, r);
            const float3 portal = l + (r - l) * 0.386731f;
            const int atEdge = side[t] ^ Crossings(cuts[t], FaceCoords(mesh, t, portal));
            if (neighbor < 0) {
                boundary[atEdge] += Length(r - l);
                continue;
            }
            int next = atEdge ^ Crossings(cuts[neighbor], FaceCoords(mesh, neighbor, portal));
            if (side[neighbor] < 0) {
                side[neighbor] = (int8_t)next;
                queue.push_back((u32)neighbor);
            } else if (side[neighbor] != next)
                return false; // A loop around a handle has no inside.
        }
    }
    const int outside = (boundary[0] + boundary[1] > 0) ? boundary[1] > boundary[0] : area[1] > area[0];
    mask.assign((size_t)width * height, 0);
    for (u32 t : queue) {
        if (side[t] == outside && cuts[t].empty()) continue;
        const auto* ix = &mesh.indices[(size_t)t * 3];
        const auto pixel = [&](u32 i) {
            auto uv = mesh.vertices[i].uv;
            return float2{uv.x * width, uv.y * height};
        };
        auto a = pixel(ix[0]), b = pixel(ix[1]), c = pixel(ix[2]);
        const float det = Wedge(b - a, c - a);
        if (std::abs(det) < 1e-10f) continue;
        const int x0 = std::max(0, (int)std::floor(std::min({a.x, b.x, c.x}))),
                  y0 = std::max(0, (int)std::floor(std::min({a.y, b.y, c.y})));
        const int x1 = std::min(width, (int)std::ceil(std::max({a.x, b.x, c.x}))),
                  y1 = std::min(height, (int)std::ceil(std::max({a.y, b.y, c.y})));
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const float2 offset = float2{x + 0.5f, y + 0.5f} - a;
                const float2 weights{Wedge(offset, c - a) / det, Wedge(b - a, offset) / det};
                if (weights.x < 0 || weights.y < 0 || weights.x + weights.y > 1) continue;
                if ((side[t] ^ Crossings(cuts[t], weights)) != outside) mask[(size_t)y * width + x] = 255;
            }
    }
    return true;
}
} // namespace sx
