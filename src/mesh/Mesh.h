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
    Mat4 operator*(const Mat4& b) const;
    float4 Transform(float4 v) const;
};

struct MeshVertex {
    float3 pos;
    float3 normal;
    float2 uv;
};
static_assert(sizeof(MeshVertex) == 32, "vertex layout");

struct MeshData {
    std::string name;
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    float3 boundsMin, boundsMax;
};

bool LoadObj(const std::string& path, MeshData& out, std::string* error);
bool LoadGltf(const std::string& path, MeshData& out, std::string* error);  // .gltf / .glb
// By extension (.obj, .gltf, .glb).
bool LoadMesh(const std::string& path, MeshData& out, std::string* error);
MeshData MakeSphere(int segments = 64, int rings = 32);
MeshData MakeCube();
MeshData MakePlane();
// Center on the origin and scale to a unit radius (keeps painting brush sizes predictable).
void NormalizeMesh(MeshData& m);

}  // namespace sx
