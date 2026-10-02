#pragma once
// 3D painting scene attached to a document: mesh buffers, UV-space position
// and normal maps, orbit camera and the viewport render targets.

#include "mesh/Mesh.h"

namespace sx {

// PBR material channels (shader slots t0..t5).
enum class PbrChannel : int { BaseColor, Roughness, Metallic, Normal, Emissive, Occlusion, Count };
const char* PbrChannelName(PbrChannel c);    // group name in the document
const char* PbrChannelSuffix(PbrChannel c);  // file / Unreal suffix
struct MeshMaterial {
    gpu::Texture* tex[(int)PbrChannel::Count] = {};
};

struct MeshScene {
    MeshData data;
    gpu::BufferPtr vb, ib;
    gpu::TexturePtr posMap, nrmMap, tmpPos, tmpNrm;  // UV bake (document size)
    u32 bakedW = 0, bakedH = 0;
    bool gpuDirty = true;
    gpu::TexturePtr color, depth;                     // viewport
    float yaw = 0.7f, pitch = 0.35f, distance = 2.8f, fov = 0.8f;
    float3 target;
    Mat4 viewProj;
    float3 camPos;
    void UpdateCamera(float aspect);
};

}  // namespace sx
