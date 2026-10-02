// Smudge: one dab at a time. Pulls pixels from behind the dab (along its motion)
// into the dab, reading a snapshot of the region taken just before the dab.
#include "common.slang"

cbuffer Root : register(b0)
{
    int2 gRectOrigin;
    uint2 gRectSize;
    uint2 gDocSize;
    float2 gCenter;
    float2 gDelta;      // motion since the previous dab
    float gRadius;
    float gHardness;
    float gStrength;
    uint gFlags;        // 1 = has selection
};

Texture2D<float4> gSnap : register(t0);
Texture2D<float> gSel : register(t1);
RWTexture2D<float4> gLayer : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gRectSize)) return;
    int2 px = gRectOrigin + int2(id.xy);
    if (any(px < 0) || any(px >= int2(gDocSize))) return;
    float2 p = float2(px) + 0.5;
    float r = max(gRadius, 0.5);
    float d = length(p - gCenter);
    float inner = r * saturate(gHardness);
    float cov = (r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0) * saturate(r - d + 0.5);
    if (gFlags & 1) cov *= gSel.Load(int3(px, 0));
    if (cov <= 0.0) return;
    float4 cur = gSnap.Load(int3(px, 0));
    float4 from = gSnap.SampleLevel(sLinearClamp, (p - gDelta) / float2(gDocSize), 0);
    gLayer[px] = lerp(cur, from, saturate(cov * gStrength));
}
