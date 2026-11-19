#include "mesh/UvUnwrap.h"

#include <xatlas.h>
#include <array>
#include <unordered_map>

namespace sx {

namespace {
struct ProgressState {
    std::atomic<float>* progress;
    std::atomic<bool>* cancel;
};

bool OnProgress(xatlas::ProgressCategory category, int percent, void* user) {
    auto* s = (ProgressState*)user;
    // Rough weights: charts (add mesh, segmentation, parameterization) then packing.
    float base = 0, span = 0.25f;
    switch (category) {
        case xatlas::ProgressCategory::AddMesh: base = 0.0f; span = 0.05f; break;
        case xatlas::ProgressCategory::ComputeCharts: base = 0.05f; span = 0.45f; break;
        case xatlas::ProgressCategory::PackCharts: base = 0.5f; span = 0.45f; break;
        case xatlas::ProgressCategory::BuildOutputMeshes: base = 0.95f; span = 0.05f; break;
        default: break;
    }
    if (s->progress) s->progress->store(base + span * percent / 100.0f);
    return !(s->cancel && s->cancel->load());
}
}  // namespace

bool RepairPartialUvs(MeshData& mesh, const std::vector<u8>& missing) {
    if (missing.size() != mesh.indices.size() / 3) return false;
    if (std::none_of(missing.begin(), missing.end(), [](u8 x) { return x != 0; })) return true;
    if (std::all_of(missing.begin(), missing.end(), [](u8 x) { return x != 0; })) return false;
    std::array<bool, 1000> used{};
    MeshData holes;
    std::unordered_map<u32, u32> remap;
    for (size_t t = 0; t < missing.size(); ++t) {
        const u32* face = mesh.indices.data() + t * 3;
        if (missing[t]) {
            for (u32 index : {face[0], face[1], face[2]}) {
                auto [it, inserted] = remap.emplace(index, (u32)holes.vertices.size());
                if (inserted) holes.vertices.push_back(mesh.vertices[index]);
                holes.indices.push_back(it->second);
            }
        } else {
            const auto& a = mesh.vertices[face[0]].uv;
            const auto& b = mesh.vertices[face[1]].uv;
            const auto& c = mesh.vertices[face[2]].uv;
            const float2 center = (a + b + c) / 3;
            if (!std::isfinite(center.x) || !std::isfinite(center.y) || center.x < 0 || center.x >= 10 || center.y > 1 || center.y <= -99) return false;
            const int col = (int)std::floor(center.x), row = (int)std::floor(1 - center.y);
            if (row >= 100) return false;
            for (const auto& uv : {a, b, c})
                if (uv.x < col - 0.001f || uv.x > col + 1.001f || 1 - uv.y < row - 0.001f || 1 - uv.y > row + 1.001f) return false;
            used[row * 10 + col] = true;
        }
    }
    const auto free = std::find(used.begin(), used.end(), false);
    if (free == used.end()) return false;
    UnwrapResult result;
    if (!UnwrapMesh(holes, UnwrapOptions{}, result) || result.mesh.indices.size() != holes.indices.size() || !result.error.empty()) return false;
    const size_t cell = (size_t)(free - used.begin());
    const float2 offset = {(float)(cell % 10), -(float)(cell / 10)};
    const u32 base = (u32)mesh.vertices.size();
    for (auto& vertex : result.mesh.vertices) {
        vertex.uv = vertex.uv + offset;
        mesh.vertices.push_back(vertex);
    }
    size_t index = 0;
    for (size_t t = 0; t < missing.size(); ++t)
        if (missing[t]) for (unsigned k = 0; k < 3; ++k) mesh.indices[t * 3 + k] = base + result.mesh.indices[index++];
    return true;
}

bool UnwrapMesh(const MeshData& in, const UnwrapOptions& o, UnwrapResult& out, std::atomic<float>* progress, std::atomic<bool>* cancel) {
    out = {};
    if (in.vertices.empty() || in.indices.size() < 3) { out.error = "maillage vide"; return false; }
    xatlas::Atlas* atlas = xatlas::Create();
    ProgressState ps{progress, cancel};
    xatlas::SetProgressCallback(atlas, OnProgress, &ps);
    xatlas::MeshDecl decl;
    decl.vertexPositionData = &in.vertices[0].pos;
    decl.vertexPositionStride = sizeof(MeshVertex);
    decl.vertexNormalData = &in.vertices[0].normal;
    decl.vertexNormalStride = sizeof(MeshVertex);
    if (o.useInputUvs) {
        decl.vertexUvData = &in.vertices[0].uv;
        decl.vertexUvStride = sizeof(MeshVertex);
    }
    decl.vertexCount = (u32)in.vertices.size();
    decl.indexData = in.indices.data();
    decl.indexCount = (u32)in.indices.size();
    decl.indexFormat = xatlas::IndexFormat::UInt32;
    xatlas::AddMeshError err = xatlas::AddMesh(atlas, decl);
    if (err != xatlas::AddMeshError::Success) {
        out.error = std::string("maillage refusé : ") + xatlas::StringForEnum(err);
        xatlas::Destroy(atlas);
        return false;
    }
    xatlas::ChartOptions co;
    co.normalDeviationWeight = o.normalDeviationWeight;
    co.roundnessWeight = o.roundnessWeight;
    co.straightnessWeight = o.straightnessWeight;
    co.normalSeamWeight = o.normalSeamWeight;
    co.textureSeamWeight = o.textureSeamWeight;
    co.maxCost = o.maxCost;
    co.maxIterations = (uint32_t)std::max(1, o.maxIterations);
    co.useInputMeshUvs = o.useInputUvs;
    xatlas::PackOptions po;
    po.resolution = (uint32_t)std::max(64, o.resolution);
    po.padding = (uint32_t)std::max(0, o.padding);
    po.bruteForce = o.bruteForce;
    po.blockAlign = o.blockAlign;
    po.rotateCharts = o.rotateCharts;
    po.rotateChartsToAxis = o.rotateCharts;
    xatlas::Generate(atlas, co, po);
    if (cancel && cancel->load()) { xatlas::Destroy(atlas); out.error = "annulé"; return false; }
    if (atlas->meshCount < 1 || atlas->width == 0 || atlas->height == 0) { xatlas::Destroy(atlas); out.error = "échec du dépliage"; return false; }
    const xatlas::Mesh& m = atlas->meshes[0];
    // One square texture: the atlas size is the larger side (UVs normalized by it, no stretch).
    const float side = (float)std::max(atlas->width, atlas->height);
    out.mesh.name = in.name;
    out.mesh.boundsMin = in.boundsMin;
    out.mesh.boundsMax = in.boundsMax;
    out.mesh.srcCenter = in.srcCenter;
    out.mesh.srcScale = in.srcScale;
    out.mesh.vertices.resize(m.vertexCount);
    out.sourceVertex.resize(m.vertexCount);
    out.chartOfVertex.resize(m.vertexCount);
    for (u32 i = 0; i < m.vertexCount; ++i) {
        const xatlas::Vertex& xv = m.vertexArray[i];
        MeshVertex v = in.vertices[xv.xref];
        v.uv = {xv.uv[0] / side, xv.uv[1] / side};
        out.mesh.vertices[i] = v;
        out.sourceVertex[i] = xv.xref;
        out.chartOfVertex[i] = xv.chartIndex < 0 ? 0u : (u32)xv.chartIndex;
    }
    out.mesh.indices.assign(m.indexArray, m.indexArray + m.indexCount);
    // Materials kept: xatlas keeps the face order, so the per-triangle material carries over.
    // The material tiles do not (new UVs over the whole texture), and the mesh has UVs now.
    out.mesh.materials = in.materials;
    if (m.indexCount == in.indices.size()) {
        out.mesh.triMaterial = in.triMaterial;
        out.mesh.partNames = in.partNames;
        out.mesh.triPart = in.triPart;
    }
    out.mesh.uvTiles.clear();
    out.mesh.udims.clear();
    out.mesh.udimMaterials.clear();
    out.mesh.udimScale.clear();
    out.mesh.udimCols = 0;
    out.mesh.hasUvs = true;
    out.charts = atlas->chartCount;
    out.width = atlas->width;
    out.height = atlas->height;
    out.utilization = atlas->atlasCount > 0 && atlas->utilization ? atlas->utilization[0] : 0.0f;
    if (atlas->atlasCount > 1) out.error = "plusieurs atlas produits : seul le premier est utilisé (augmentez la résolution)";
    xatlas::Destroy(atlas);
    if (progress) progress->store(1.0f);
    return true;
}

}  // namespace sx
