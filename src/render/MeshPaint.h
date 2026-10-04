#pragma once
// 3D painting scene attached to a document: mesh buffers, UV-space position
// and normal maps, orbit camera and the viewport render targets.

#include "mesh/Mesh.h"

namespace sx {

// PBR material channels (shader slots t0..t5).
enum class PbrChannel : int { BaseColor, Roughness, Metallic, Normal, Emissive, Occlusion, Height, Count };
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
    u32 revision = 0;  // bumped when the geometry or UVs change (render scene)
    gpu::TexturePtr color, depth;                     // viewport
    float yaw = 0.7f, pitch = 0.35f, distance = 2.8f, fov = 0.8f;
    float3 target;
    Mat4 viewProj;
    float3 camPos;
    // HDRI environment (prefiltered atlas uploaded on the next render).
    gpu::TexturePtr envAtlas;
    std::vector<float4> envPixels;
    u32 envW = 0, envH = 0;
    std::string envName;
    float envIntensity = 1.0f, envRotation = 0.0f;
    bool envBackground = true;  // show the HDRI behind the model
    float envBlur = 0.0f;       // background blur (atlas level 0..5)
    int viewChannel = 0;  // 0 = PBR, k = PbrChannel k-1 alone
    void UpdateCamera(float aspect);
};

// Load an equirectangular .hdr (or any image) as the 3D view's environment.
bool LoadEnvironment(MeshScene& s, const std::string& path, std::string* error);

}  // namespace sx
