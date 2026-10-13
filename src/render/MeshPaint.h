#pragma once
// 3D painting scene attached to a document: mesh buffers, UV-space position
// and normal maps, orbit camera and the viewport render targets.

#include "mesh/Mesh.h"
#include "render/PostProcess.h"

namespace sx {

// PBR material channels (shader slots t0..t5).
enum class PbrChannel : int { BaseColor, Roughness, Metallic, Normal, Emissive, Occlusion, Height, Count };
const char* PbrChannelName(PbrChannel c);    // group name in the document
const char* PbrChannelSuffix(PbrChannel c);  // file / Unreal suffix
struct MeshMaterial {
    gpu::Texture* tex[(int)PbrChannel::Count] = {};
};

// Extra directional light of the 3D view (fill, rim...): no shadow map; the ray-traced view casts its shadows.
struct MeshLight {
    bool on = true;
    bool follow = true;          // direction relative to the camera (like the key light)
    float yaw = 0, pitch = 0.5f; // radians
    float3 color = {1, 1, 1};
    float intensity = 1.0f;
};
constexpr int kMaxMeshLights = 4;

// Saved point of view of the 3D view.
struct CameraBookmark {
    std::string name;
    float yaw = 0, pitch = 0, distance = 2.8f, fov = 0.8f;
    float3 target;
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
    bool ortho = false;  // orthographic projection: the view's height is what the perspective shows at the target
    float OrthoHalfHeight() const { return distance * std::tan(fov * 0.5f); }
    // Eye for view-dependent shading: far along the view axis when orthographic (parallel view direction).
    float3 ShadingEye() const { return ortho ? target + (camPos - target) * (1000.0f / std::max(distance, 1e-3f)) : camPos; }
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
    int viewChannel = 0;  // 0 = PBR, k = PbrChannel k-1 alone, 8..11 baked mesh maps
    bool wireframe = false;
    // Rendering (Marmoset-like): post-process, quality, key light and its shadow.
    PostSettings post;
    PostChain postChain;
    int quality = 1;                 // 0 fast, 1 shadows + baked occlusion
    bool lightFollow = true;         // key light attached to the camera
    float lightYaw = 0.8f, lightPitch = 0.9f;  // fixed light (radians)
    float lightIntensity = 1.6f;     // without an HDRI
    float sunIntensity = 0.0f;       // with an HDRI
    float shadowStrength = 0.85f, aoStrength = 1.0f;
    bool groundShadow = true;        // shadow on the ground under the model (quality view)
    float ssao = 0.8f;               // screen-space ambient occlusion strength (quality view, 0 off)
    gpu::TexturePtr aoTex;
    std::vector<MeshLight> lights;   // extra lights (up to kMaxMeshLights)
    float dofAperture = 0.0f, dofFocus = 2.8f;  // depth of field (ray-traced view): lens radius, focus distance
    std::vector<CameraBookmark> cameras;
    gpu::TexturePtr hdr, shadowMap;
    int rtSamples = 512, rtBounces = 6;  // ray-traced view (quality 2)
    float rtDenoise = 0.6f;
    bool rtFloor = true;
    std::string envPath;                 // HDRI file (the ray-traced view loads it too)
    // Mesh maps for smart masks (R AO, G convexity, B concavity, A thickness), baked on the CPU.
    gpu::TexturePtr maps;
    u32 mapsRev = 0;              // bumped at each bake (smart masks regenerate)
    u32 mapsForRevision = ~0u;    // geometry revision the maps belong to
    void UpdateCamera(float aspect);
    float3 KeyLightDir() const;                    // toward the light
    float3 LightDir(const MeshLight& l) const;
};

// Load an equirectangular .hdr (or any image) as the 3D view's environment.
bool LoadEnvironment(MeshScene& s, const std::string& path, std::string* error);

}  // namespace sx
