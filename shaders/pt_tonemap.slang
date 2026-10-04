// Render mode: accumulation (sum of radiance, a = sample count) to a display
// image: exposure, tone curve, sRGB encoding.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    float gExposure;   // multiplier
    uint gTonemap;     // 0 linear (clipped), 1 ACES, 2 Reinhard
};

Texture2D<float4> gAccum : register(t0);
RWTexture2D<float4> gOut : register(u0);

float3 Aces(float3 x)
{
    // Narkowicz fit of the ACES filmic curve.
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float4 a = gAccum.Load(int3(id.xy, 0));
    float3 c = a.a > 0 ? a.rgb / a.a : float3(0, 0, 0);
    c *= gExposure;
    if (gTonemap == 1) c = Aces(c);
    else if (gTonemap == 2) c = c / (1.0 + Luma(c));
    gOut[id.xy] = float4(LinearToSrgb(saturate(c)), 1.0);
}
