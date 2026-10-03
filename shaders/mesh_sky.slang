// 3D view background: the HDRI environment seen through the camera, optionally
// blurred. Writes display-ready (sRGB-encoded) pixels into the viewport.
#include "env_common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    float gBlur;       // atlas level, 0 sharp .. 5
    float gIntensity;
};

cbuffer Sky : register(b1)
{
    float4 gRight;     // camera basis scaled by the frustum half extents
    float4 gUp;
    float4 gForward;
    float gRotation;
};

Texture2D<float4> gEnv : register(t0);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float2 ndc = float2((id.x + 0.5) / gSize.x * 2.0 - 1.0, 1.0 - (id.y + 0.5) / gSize.y * 2.0);
    float3 d = normalize(gForward.xyz + gRight.xyz * ndc.x + gUp.xyz * ndc.y);
    float3 c = EnvSample(gEnv, d, gBlur, gRotation) * gIntensity;
    c = c / (1.0 + c * 0.15);  // same highlight compression as the shaded model
    gOut[id.xy] = float4(LinearToSrgb(saturate(c)), 1.0);
}
