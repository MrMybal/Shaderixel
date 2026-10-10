#pragma once
// Mesh maps baked on the CPU in UV space (BVH ray tracing):
//  - smart masks (Paint 3D): R ambient occlusion (1 = open), G convexity (sharp convex edges,
//    bumps), B concavity (creases, cavities), A thickness (1 = thick);
//  - the baker (Marmoset / Substance style): from the low poly itself, or from a high poly
//    model projected on it (cage distances): tangent-space normal, AO, curvature, thickness,
//    height, position, world normal.
// The models are the normalized ones (unit radius), so distances are relative.

#include "mesh/Mesh.h"

#include <atomic>

namespace sx {

struct MeshMapsOptions {
    int size = 1024;
    int aoRays = 24;
    float aoDistance = 0.5f;   // rays longer than this do not occlude
    float edgeWidth = 0.12f;   // reach of the edge proximity (convex / concave)
    float sharpAngle = 25.0f;  // degrees between faces for a sharp edge
};

struct MeshMaps {
    int size = 0;
    std::vector<float> rgba;   // size * size * 4
    bool ok = false;
    std::string error;
};

// Runs on any thread; progress 0..1, cancel stops early (result not ok).
MeshMaps BakeMeshMaps(const MeshData& mesh, const MeshMapsOptions& o, std::atomic<float>* progress = nullptr, std::atomic<bool>* cancel = nullptr);

// ------------------------------------------------------------------ baker

struct BakeOptions {
    int size = 2048;
    int aoRays = 64;
    float aoDistance = 0.4f;
    float edgeWidth = 0.06f;
    float sharpAngle = 25.0f;
    // High poly projection: rays start `front` above the low surface and search down to `back` below it.
    float front = 0.05f, back = 0.05f;
    bool normal = true, ao = true, curvature = true, thickness = true, height = true, position = false, worldNormal = false;
    int padding = 16;  // texels painted past the UV islands (dilation)
};

struct BakeResult {
    int size = 0;
    bool ok = false;
    std::string error;
    size_t texels = 0, misses = 0;  // high poly: texels whose ray found nothing (kept from the low poly)
    // Each map size*size, encoded 0..1 like a texture (normal: tangent space, X right, Y down = DirectX / Unreal).
    std::vector<float> normal;      // rgb
    std::vector<float> ao;          // 1 = open
    std::vector<float> curvature;   // 0.5 flat, > 0.5 convex
    std::vector<float> thickness;   // 1 = thick
    std::vector<float> height;      // 0.5 = on the low surface, > 0.5 high poly above it
    std::vector<float> position;    // rgb, bounding box mapped to 0..1
    std::vector<float> worldNormal; // rgb, -1..1 -> 0..1
    std::vector<float> maps;        // smart mask maps (rgba, see MeshMaps)
};

// `high` (optional) must be in the low poly's space (see TransformToMeshSpace).
BakeResult BakeMeshes(const MeshData& low, const MeshData* high, const BakeOptions& o, std::atomic<float>* progress = nullptr,
                      std::atomic<bool>* cancel = nullptr);

// Rays against a mesh (particle simulation, picking): nearest hit, interpolated normal (outward).
class MeshRaycaster {
public:
    void Build(const MeshData& m);
    bool Empty() const { return !impl; }
    bool Ray(float3 origin, float3 dir, float tmax, float3* pos, float3* normal, float* t = nullptr) const;

private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

// Positions / normals of `m` (normalized by its loader) moved into the normalized space of `ref`
// (both use srcCenter / srcScale to go back to the file units).
void TransformToMeshSpace(MeshData& m, const MeshData& ref);

}  // namespace sx
