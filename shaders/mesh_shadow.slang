// 3D view shadow map: the model's depth seen from the key light (orthographic).
#include "common.slang"

cbuffer View : register(b1)
{
    row_major float4x4 gLightVP;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};

float4 vsMain(VSIn i) : SV_Position
{
    return mul(gLightVP, float4(i.pos, 1.0));
}

void psMain()
{
}
