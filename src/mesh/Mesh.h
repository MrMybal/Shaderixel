#pragma once
// Meshes for 3D painting: OBJ loading, procedural primitives, small 3D math.

#include "core/Common.h"
#include "gpu/Device.h"

namespace sx {

struct float3 {
    float x = 0, y = 0, z = 0;
    float3() = default;
    constexpr float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    float3 operator+(float3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    float3 operator-(float3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    float3 operator*(float s) const { return {x * s, y * s, z * s}; }
};
inline float Dot(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float3 Cross(float3 a, float3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Length(float3 a) { return std::sqrt(Dot(a, a)); }
inline float3 Normalize(float3 a) { float l = Length(a); return l > 1e-12f ? a * (1.0f / l) : float3{0, 0, 1}; }

// Row-major 4x4, column-vector convention: clip = M * p.
struct Mat4 {
    float m[4][4] = {};
    static Mat4 Identity();
    static Mat4 LookAtLH(float3 eye, float3 at, float3 up);
    static Mat4 PerspectiveLH(float fovY, float aspect, float zn, float zf);
    static Mat4 OrthoLH(float halfW, float halfH, float zn, float zf);  // zn may be negative
    Mat4 operator*(const Mat4& b) const;
    float4 Transform(float4 v) const;
};

struct MeshVertex {
    float3 pos;
    float3 normal;
    float2 uv;
};
static_assert(sizeof(MeshVertex) == 32, "vertex layout");

// Material of a part of an imported model (render scenes).
struct MeshMaterialInfo {
    std::string name;
    float4 baseColor = {0.8f, 0.8f, 0.8f, 1.0f};
    float roughness = 0.5f, metallic = 0.0f;
    float3 emissive;
    std::string baseColorPath;      // texture file (absolute, or relative to the model)
    std::vector<u8> baseColorData;  // embedded texture (encoded image)
};

struct MeshData {
    std::string name;
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    std::vector<MeshMaterialInfo> materials;  // imported models: one per part (optional)
    std::vector<u16> triMaterial;             // per triangle, index into materials (empty: one material)
    // Paint 3D: each material's UVs moved into its own tile of the texture (UV rect x0, y0, x1, y1 per material;
    // empty: the UVs as loaded). Exports cut one texture set per material from these tiles.
    std::vector<float4> uvTiles;
    float3 boundsMin, boundsMax;
    // NormalizeMesh: original position = pos / srcScale + srcCenter (exports in the file's own units).
    float3 srcCenter;
    float srcScale = 1.0f;
    bool hasUvs = true;  // false: loaded without texture coordinates (unwrap before painting)
};

bool LoadObj(const std::string& path, MeshData& out, std::string* error);
bool LoadGltf(const std::string& path, MeshData& out, std::string* error);  // .gltf / .glb
bool LoadFbx(const std::string& path, MeshData& out, std::string* error);
// By extension (.obj, .gltf, .glb, .fbx).
bool LoadMesh(const std::string& path, MeshData& out, std::string* error);
MeshData MakeSphere(int segments = 64, int rings = 32);
MeshData MakeCube();
MeshData MakePlane();
// Center on the origin and scale to a unit radius (keeps painting brush sizes predictable).
void NormalizeMesh(MeshData& m);
// Several materials sharing the UV space (each one laid out over 0..1, or tiling): every material's UVs are
// fitted into its own tile of a grid (padding in texels for `resolution`). mode 0: only when the materials'
// UVs overlap or leave 0..1, 1: always (2 materials or more). Returns true when the UVs were moved.
// sourceVertex (optional): for each vertex of the result, the vertex it comes from (paint transfer).
bool TileMaterialUvs(MeshData& m, u32 resolution, int mode, std::vector<u32>* sourceVertex = nullptr);
// Wavefront OBJ (positions back in the original units when `original`), Y up, UV v flipped like LoadObj reads it.
bool SaveObj(const std::string& path, const MeshData& m, bool original, std::string* error = nullptr);

}  // namespace sx
