#pragma once
// Mesh maps for smart masks (Paint 3D), baked on the CPU in UV space:
//   R ambient occlusion (1 = open), G convexity (sharp convex edges, bumps),
//   B concavity (creases, cavities), A thickness (1 = thick).
// The model is the normalized one (unit radius), so the distances are relative.

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

}  // namespace sx
