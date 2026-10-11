// Render passes of the path tracer from its first-hit guides (averaged over the samples):
// albedo, world normal, depth, coverage mask. Output RGBA8, ready to export.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gMode;   // 0 albedo, 1 normal, 2 depth, 3 mask
    float gFar;   // depth: distance shown black
};

Texture2D<float4> gAccum : register(t0);   // a: samples
Texture2D<float4> gGuideA : register(t1);  // albedo sum, a: hits
Texture2D<float4> gGuideN : register(t2);  // normal sum, w: distance sum
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int3 p = int3(id.xy, 0);
    const float n = max(gAccum.Load(p).a, 1.0);
    const float4 ga = gGuideA.Load(p) / n, gn = gGuideN.Load(p) / n;
    const float cover = saturate(ga.a);
    float3 c = 0;
    if (gMode == 0) c = cover > 0 ? LinearToSrgb(saturate(ga.rgb)) : 0;
    else if (gMode == 1) c = length(gn.xyz) > 1e-3 ? normalize(gn.xyz) * 0.5 + 0.5 : float3(0.5, 0.5, 0.5);
    else if (gMode == 2)
    {
        // Mean distance of the hits only (the background counts 1e4 per sample).
        const float d = cover > 0 ? (gn.w - (1.0 - cover) * 1e4) / cover : 1e4;
        c = (1.0 - saturate(d / gFar)) * cover;
    }
    else c = cover;
    gOut[id.xy] = float4(c, cover > 0 || gMode == 1 ? 1.0 : (gMode == 3 ? 1.0 : 0.0));
}
