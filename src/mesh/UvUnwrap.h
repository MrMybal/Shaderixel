#pragma once
// Automatic UV unwrapping with xatlas: charts (segmentation + parameterization)
// and packing into one atlas. The result is a new mesh (vertices split at the
// seams) that remembers, per vertex, the vertex it comes from.
#include "mesh/Mesh.h"

#include <atomic>
#include <future>

namespace sx {

struct UnwrapOptions {
    // Charts
    float normalDeviationWeight = 2.0f;  // angle between a face and its chart's normal
    float roundnessWeight = 0.01f;
    float straightnessWeight = 6.0f;
    float normalSeamWeight = 4.0f;       // > 1000: hard normal seams always cut
    float textureSeamWeight = 0.5f;
    float maxCost = 2.0f;                // lower: more, flatter charts
    int maxIterations = 1;
    bool useInputUvs = false;            // keep the existing islands (re-pack only)
    // Packing
    int resolution = 2048;
    int padding = 4;                     // texels between charts
    bool bruteForce = false;             // slower, tighter
    bool blockAlign = false;
    bool rotateCharts = true;
};

struct UnwrapResult {
    MeshData mesh;                   // new vertices / indices / normalized UVs
    std::vector<u32> sourceVertex;   // per new vertex: index of the input vertex
    std::vector<u32> chartOfVertex;  // per new vertex: chart index (preview colors)
    u32 charts = 0, width = 0, height = 0;
    float utilization = 0;           // 0..1 of the atlas covered
    std::string error;
};

// Runs on any thread; progress 0..1, cancel stops at the next checkpoint.
bool UnwrapMesh(const MeshData& in, const UnwrapOptions& o, UnwrapResult& out, std::atomic<float>* progress = nullptr,
                std::atomic<bool>* cancel = nullptr);

// Background unwrap (UI): input copy, result, progress, cancel.
struct UvJob {
    UnwrapOptions options;
    MeshData input;
    UnwrapResult result;
    std::future<bool> work;
    std::atomic<float> progress{0};
    std::atomic<bool> cancel{false};
};

}  // namespace sx
