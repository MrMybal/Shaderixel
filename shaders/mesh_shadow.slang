// 3D view shadow map: the model's depth seen from the key light (orthographic).
#include "common.slang"
#include "mesh_opacity.slang"

cbuffer View : register(b1)
{
    row_major float4x4 gLightVP;
    uint gHasOpacity;
};
Texture2D<float4> gOpacity : register(t0);

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};

struct ShadowVertex { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
ShadowVertex vsMain(VSIn i)
{
    ShadowVertex o;
    o.pos = mul(gLightVP, float4(i.pos, 1.0));
    o.uv = i.uv;
    return o;
}

void psMain(ShadowVertex i)
{
    if (gHasOpacity != 0 && MeshOpacity(gOpacity, i.uv) < 0.5) discard;
}
