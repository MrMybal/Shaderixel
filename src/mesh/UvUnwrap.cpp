#include "mesh/UvUnwrap.h"

#include <xatlas.h>

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
