#pragma once
#include "mesh/Mesh.h"
#include <array>

namespace sx {
struct SurfaceAnchor {
    u32 triangle = 0;
    float3 weights;
};
struct SurfaceLineSegment {
    float4 a, b, normal; // xyz positions; w carries cumulative length
};

// Walks the triangle faces between barycentric anchors, welding UV-seam copies of the same edges.
class SurfacePathTopology {
  public:
    void Build(const MeshData& mesh);
    bool Connect(const MeshData& mesh, SurfaceAnchor from, SurfaceAnchor to, float& length, std::vector<SurfaceLineSegment>& segments,
                 std::vector<u32>* owners = nullptr) const;
    bool Fill(const MeshData& mesh, const std::vector<SurfaceLineSegment>& segments, const std::vector<u32>& owners, int width, int height,
              std::vector<u8>& mask) const;
    static float3 Position(const MeshData& mesh, SurfaceAnchor point);

  private:
    struct Face {
        float3 center, normal;
        std::array<int, 3> neighbors{-1, -1, -1};
    };
    std::vector<Face> faces;
};
} // namespace sx
