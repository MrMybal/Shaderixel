// Paint 3D tools on the mesh: picking a triangle under the cursor and the
// polygon fill (triangle, flat face, UV island, mesh part, whole model).
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

#include <imgui_internal.h>
#include <unordered_map>

namespace sx {

namespace {

float2 ToScreen(const Mat4& vp, float3 p, u32 w, u32 h, float* depth) {
    float4 c = vp.Transform({p.x, p.y, p.z, 1});
    if (c.w <= 1e-5f) { if (depth) *depth = 1e30f; return {-1e9f, -1e9f}; }
    if (depth) *depth = c.z / c.w;
    return {(c.x / c.w * 0.5f + 0.5f) * w, (0.5f - c.y / c.w * 0.5f) * h};
}

struct UnionFind {
    std::vector<u32> p;
    explicit UnionFind(size_t n) : p(n) { for (size_t i = 0; i < n; ++i) p[i] = (u32)i; }
    u32 Find(u32 x) { while (p[x] != x) x = p[x] = p[p[x]]; return x; }
    void Join(u32 a, u32 b) { a = Find(a); b = Find(b); if (a != b) p[b] = a; }
};

u64 Key3(float3 v) {
    // Welded position (1e-5 of the normalized model).
    auto q = [](float f) { return (u64)(u32)(i32)std::lround(f * 100000.0f) & 0x1FFFFF; };
    return q(v.x) | (q(v.y) << 21) | (q(v.z) << 42);
}

}  // namespace

int App::PickMeshTriangle(MeshScene& s, float2 sp, u32 w, u32 h) {
    // The nearest triangle whose screen projection contains the point.
    const auto& V = s.data.vertices;
    const auto& I = s.data.indices;
    int best = -1;
    float bestZ = 1e30f;
    for (size_t t = 0; t + 2 < I.size(); t += 3) {
        float z[3];
        float2 a = ToScreen(s.viewProj, V[I[t]].pos, w, h, &z[0]), b = ToScreen(s.viewProj, V[I[t + 1]].pos, w, h, &z[1]),
               c = ToScreen(s.viewProj, V[I[t + 2]].pos, w, h, &z[2]);
        if (z[0] > 1e29f || z[1] > 1e29f || z[2] > 1e29f) continue;
        const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::fabs(area) < 1e-8f) continue;
        const float w0 = ((b.x - sp.x) * (c.y - sp.y) - (b.y - sp.y) * (c.x - sp.x)) / area;
        const float w1 = ((c.x - sp.x) * (a.y - sp.y) - (c.y - sp.y) * (a.x - sp.x)) / area;
        const float w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        const float d = w0 * z[0] + w1 * z[1] + w2 * z[2];
        if (d < bestZ) { bestZ = d; best = (int)(t / 3); }
    }
    return best;
}

bool App::PickMeshUv(MeshScene& s, float2 sp, u32 w, u32 h, float2& uv) {
    // UV under a view point: the nearest triangle there, its UVs interpolated (screen barycentrics).
    const auto& V = s.data.vertices;
    const auto& I = s.data.indices;
    const int t = PickMeshTriangle(s, sp, w, h);
    if (t < 0) return false;
    float z[3];
    const MeshVertex &A = V[I[t * 3]], &B = V[I[t * 3 + 1]], &C = V[I[t * 3 + 2]];
    const float2 a = ToScreen(s.viewProj, A.pos, w, h, &z[0]), b = ToScreen(s.viewProj, B.pos, w, h, &z[1]), c = ToScreen(s.viewProj, C.pos, w, h, &z[2]);
    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(area) < 1e-8f) return false;
    const float w0 = ((b.x - sp.x) * (c.y - sp.y) - (b.y - sp.y) * (c.x - sp.x)) / area;
    const float w1 = ((c.x - sp.x) * (a.y - sp.y) - (c.y - sp.y) * (a.x - sp.x)) / area;
    const float w2 = 1 - w0 - w1;
    uv = A.uv * w0 + B.uv * w1 + C.uv * w2;
    return true;
}

void App::StartMaterialPick(DocView& v, float2 sp, u32 w, u32 h) {
    // 3D eyedropper: the channels of the project under the point, read back (one pixel each).
    float2 uv;
    if (!v.mesh || !PickMeshUv(*v.mesh, sp, w, h, uv)) { Status("Pipette 3D : cliquez sur le modèle"); return; }
    Document& d = *v.doc;
    v.renderer.EvaluateComplete(d, *ctx);
    const int x = std::clamp((int)(uv.x * d.Width()), 0, (int)d.Width() - 1), y = std::clamp((int)(uv.y * d.Height()), 0, (int)d.Height() - 1);
    static const PbrChannel chans[4] = {PbrChannel::BaseColor, PbrChannel::Roughness, PbrChannel::Metallic, PbrChannel::Height};
    for (int k = 0; k < 4; ++k) {
        gpu::Texture* t = ChannelTexture(v, chans[k]);
        matPick.rb[k] = t ? v.renderer.ReadPixel(*ctx, t, x, y) : nullptr;
    }
    matPick.active = true;
}

void App::PollMaterialPick() {
    if (!matPick.active) return;
    for (auto& rb : matPick.rb)
        if (rb && !rb->ready) return;
    auto value = [](const gpu::ReadbackPtr& rb, float4& c) {
        if (!rb) return false;
        const auto& d = rb->data;
        if (rb->format == gpu::Format::RGBA16F && d.size() >= 8) {
            const u16* h = (const u16*)d.data();
            c = {HalfToFloat(h[0]), HalfToFloat(h[1]), HalfToFloat(h[2]), HalfToFloat(h[3])};
        } else if (rb->format == gpu::Format::RGBA32F && d.size() >= 16) {
            memcpy(&c, d.data(), 16);
        } else if (d.size() >= 4) {
            c = {d[0] / 255.0f, d[1] / 255.0f, d[2] / 255.0f, d[3] / 255.0f};
        } else {
            return false;
        }
        if (c.w > 1e-5f && c.w < 0.999f) c = {c.x / c.w, c.y / c.w, c.z / c.w, 1};
        return true;
    };
    float4 c;
    std::string msg = "Pipette 3D :";
    if (value(matPick.rb[0], c)) {
        fg = {std::clamp(LinearToSrgb(c.x), 0.0f, 1.0f), std::clamp(LinearToSrgb(c.y), 0.0f, 1.0f), std::clamp(LinearToSrgb(c.z), 0.0f, 1.0f), 1.0f};
        msg += " couleur";
    }
    // Data channels hold visual gray levels (as painted).
    if (value(matPick.rb[1], c)) { brush3D.rough = std::clamp(LinearToSrgb(c.x), 0.0f, 1.0f); msg += ", rugosité " + std::to_string((int)std::lround(brush3D.rough * 100)) + " %"; }
    if (value(matPick.rb[2], c)) { brush3D.metal = std::clamp(LinearToSrgb(c.x), 0.0f, 1.0f); msg += ", métal " + std::to_string((int)std::lround(brush3D.metal * 100)) + " %"; }
    if (value(matPick.rb[3], c)) brush3D.height = std::clamp(LinearToSrgb(c.x), 0.0f, 1.0f);
    Status(msg + " (matériau du pinceau)");
    for (auto& rb : matPick.rb) rb.reset();
    matPick.active = false;
}

void App::BuildMeshTopology(MeshScene& s) {
    auto& T = meshTopo;
    const auto& V = s.data.vertices;
    const auto& I = s.data.indices;
    const size_t n = I.size() / 3;
    if (T.scene == &s && T.revision == s.revision && T.tris == n) return;
    T.scene = &s;
    T.revision = s.revision;
    T.tris = n;
    // Edges keyed by welded positions; UV islands also need the same UVs on both sides.
    UnionFind face(n), island(n), part(n);
    std::unordered_map<u64, u32> posId;
    auto pid = [&](u32 vi) {
        auto [it, added] = posId.emplace(Key3(V[vi].pos), (u32)posId.size());
        return it->second;
    };
    std::vector<u32> P(I.size());
    for (size_t i = 0; i < I.size(); ++i) P[i] = pid(I[i]);
    auto normalOf = [&](size_t t) {
        float3 a = V[I[t * 3]].pos, b = V[I[t * 3 + 1]].pos, c = V[I[t * 3 + 2]].pos;
        return Normalize(Cross(b - a, c - a));
    };
    struct EdgeRef { u32 tri; float2 uva, uvb; };
    std::unordered_map<u64, std::vector<EdgeRef>> edges;
    for (size_t t = 0; t < n; ++t)
        for (int k = 0; k < 3; ++k) {
            u32 ia = I[t * 3 + k], ib = I[t * 3 + (k + 1) % 3];
            u32 a = P[t * 3 + k], b = P[t * 3 + (k + 1) % 3];
            float2 ua = V[ia].uv, ub = V[ib].uv;
            if (a > b) { std::swap(a, b); std::swap(ua, ub); }
            edges[((u64)a << 32) | b].push_back({(u32)t, ua, ub});
        }
    auto sameUv = [](float2 x, float2 y) { return std::fabs(x.x - y.x) < 1e-5f && std::fabs(x.y - y.y) < 1e-5f; };
    for (auto& [k, list] : edges)
        for (size_t i = 1; i < list.size(); ++i) {
            const EdgeRef& e0 = list[0];
            const EdgeRef& e = list[i];
            part.Join(e0.tri, e.tri);
            if (sameUv(e0.uva, e.uva) && sameUv(e0.uvb, e.uvb)) island.Join(e0.tri, e.tri);
            if (Dot(normalOf(e0.tri), normalOf(e.tri)) > 0.995f) face.Join(e0.tri, e.tri);
        }
    T.face.resize(n);
    T.island.resize(n);
    T.part.resize(n);
    for (size_t t = 0; t < n; ++t) {
        T.face[t] = face.Find((u32)t);
        T.island[t] = island.Find((u32)t);
        T.part[t] = part.Find((u32)t);
    }
}

std::vector<u32> App::PolyFillTriangles(MeshScene& s, int tri, int mode) {
    std::vector<u32> out;
    const size_t n = s.data.indices.size() / 3;
    if (tri < 0 || (size_t)tri >= n) return out;
    if (mode == 0) return {(u32)tri};
    if (mode == 4) {
        for (size_t t = 0; t < n; ++t) out.push_back((u32)t);
        return out;
    }
    BuildMeshTopology(s);
    const std::vector<u32>& g = mode == 1 ? meshTopo.face : mode == 2 ? meshTopo.island : meshTopo.part;
    for (size_t t = 0; t < n; ++t) if (g[t] == g[tri]) out.push_back((u32)t);
    return out;
}

void App::DrawPolyHighlight(MeshScene& s, int tri, ImVec2 origin, u32 w, u32 h) {
    // Outline of what a click fills (a few thousand triangles at most, drawn as filled tint).
    std::vector<u32> tris = PolyFillTriangles(s, tri, brush3D.polyMode);
    if (tris.size() > 20000) tris.resize(20000);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto& V = s.data.vertices;
    const auto& I = s.data.indices;
    for (u32 t : tris) {
        float z;
        float2 a = ToScreen(s.viewProj, V[I[t * 3]].pos, w, h, &z), b = ToScreen(s.viewProj, V[I[t * 3 + 1]].pos, w, h, &z),
               c = ToScreen(s.viewProj, V[I[t * 3 + 2]].pos, w, h, &z);
        // Only front-facing triangles (screen winding).
        if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) > 0) continue;
        dl->AddTriangleFilled(ImVec2(origin.x + a.x, origin.y + a.y), ImVec2(origin.x + b.x, origin.y + b.y), ImVec2(origin.x + c.x, origin.y + c.y),
                              IM_COL32(255, 150, 40, 70));
    }
}

void App::PolygonFill(DocView& v, int tri, bool erase) {
    if (!v.mesh) return;
    MeshScene& s = *v.mesh;
    Document& d = *v.doc;
    std::vector<u32> tris = PolyFillTriangles(s, tri, brush3D.polyMode);
    if (tris.empty()) return;
    // The triangles rasterized in UV space (doc size), with a 2 px margin against seams.
    const int W = (int)d.Width(), H = (int)d.Height();
    std::vector<u8> cov((size_t)W * H, 0);
    const auto& V = s.data.vertices;
    const auto& I = s.data.indices;
    for (u32 t : tris) {
        float2 p[3];
        for (int k = 0; k < 3; ++k) p[k] = {V[I[t * 3 + k]].uv.x * W, V[I[t * 3 + k]].uv.y * H};
        const float area = (p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[1].y - p[0].y) * (p[2].x - p[0].x);
        if (std::fabs(area) < 1e-9f) continue;
        const float pad = 2.0f;
        int x0 = std::max(0, (int)std::floor(std::min({p[0].x, p[1].x, p[2].x}) - pad)), x1 = std::min(W - 1, (int)std::ceil(std::max({p[0].x, p[1].x, p[2].x}) + pad));
        int y0 = std::max(0, (int)std::floor(std::min({p[0].y, p[1].y, p[2].y}) - pad)), y1 = std::min(H - 1, (int)std::ceil(std::max({p[0].y, p[1].y, p[2].y}) + pad));
        // Distance to the triangle (inside = 0) for the margin.
        auto edgeDist = [&](float2 q, float2 a, float2 b) {
            float2 ab = b - a, aq = q - a;
            float l = std::max(ab.x * ab.x + ab.y * ab.y, 1e-12f);
            float h = std::clamp((aq.x * ab.x + aq.y * ab.y) / l, 0.0f, 1.0f);
            return Length(aq - ab * h);
        };
        const float sg = area > 0 ? 1.0f : -1.0f;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float2 q{x + 0.5f, y + 0.5f};
                float e0 = ((p[1].x - p[0].x) * (q.y - p[0].y) - (p[1].y - p[0].y) * (q.x - p[0].x)) * sg;
                float e1 = ((p[2].x - p[1].x) * (q.y - p[1].y) - (p[2].y - p[1].y) * (q.x - p[1].x)) * sg;
                float e2 = ((p[0].x - p[2].x) * (q.y - p[2].y) - (p[0].y - p[2].y) * (q.x - p[2].x)) * sg;
                bool inside = e0 >= 0 && e1 >= 0 && e2 >= 0;
                if (!inside) {
                    float dd = std::min({edgeDist(q, p[0], p[1]), edgeDist(q, p[1], p[2]), edgeDist(q, p[2], p[0])});
                    if (dd > pad) continue;
                }
                cov[(size_t)y * W + x] = 255;
            }
    }
    auto tex = gpu::Dev().CreateTexture((u32)W, (u32)H, gpu::Format::R8, "PolyFill");
    if (!tex) return;
    ctx->Upload(tex.get(), cov.data(), (u32)W);
    std::vector<float4> colors;
    std::vector<Id> targets = Paint3DTargets(v, colors);
    if (targets.empty()) { Status("Remplissage par polygone : sélectionnez un calque 3D"); return; }
    v.history.BeginGroup();
    for (size_t i = 0; i < targets.size(); ++i)
        v.renderer.FillSelection(d, *ctx, &v.history, targets[i], erase ? 1 : 0, colors[i], 1.0f, erase ? "Effacer (polygone)" : "Remplissage par polygone", nullptr,
                                 tex.get(), true);
    v.history.EndGroup();
    static const char* what[] = {"triangle", "face", "île UV", "pièce", "modèle"};
    Status(std::string(erase ? "Effacé : " : "Rempli : ") + what[std::clamp(brush3D.polyMode, 0, 4)] + " (" + std::to_string(tris.size()) + " triangles)");
}

}  // namespace sx
