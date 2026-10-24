// Mixer brush: the brush carries a reservoir of paint (a small image in brush space). Each dab lays the reservoir
// on the canvas (pass 0), then the reservoir picks up the canvas under it (wetness) and is reloaded with the brush
// color (load) (pass 1), so colors drag and blend along the stroke like wet paint.
#include "common.slang"

cbuffer Root : register(b0)
{
    int2 gRectOrigin;
    uint2 gRectSize;    // pass 0: the dab's rectangle; pass 1: the reservoir's size
    uint2 gDocSize;
    float2 gCenter;
    float gRadius;
    float gHardness;
    float gFlow;
    float gWet;         // pickup of the canvas into the reservoir, per dab
    float gLoad;        // reload with the brush color, per dab
    uint gPass;         // 0 lay down, 1 pick up / reload, 2 reset to the brush color
    uint gFlags;        // 1 = has selection
};

cbuffer Params : register(b1)
{
    float4 gColor;      // brush color (linear, premultiplied)
};

Texture2D<float4> gSnap : register(t0);   // the canvas just before this dab
Texture2D<float> gSel : register(t1);
Texture2D<float4> gRes : register(t2);    // reservoir (brush space)
RWTexture2D<float4> gOut : register(u0);  // pass 0: the layer; pass 1: the new reservoir

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gRectSize)) return;
    const float r = max(gRadius, 0.5);
    if (gPass == 2)
    {
        gOut[id.xy] = gColor;
        return;
    }
    if (gPass == 1)
    {
        // Reservoir texel -> canvas point under it.
        const float2 uv = (float2(id.xy) + 0.5) / float2(gRectSize);
        const float2 p = gCenter + (uv * 2.0 - 1.0) * r;
        float4 under = gSnap.SampleLevel(sLinearClamp, p / float2(gDocSize), 0);
        float4 res = gRes.Load(int3(id.xy, 0));
        if (any(isnan(under)) || any(isinf(under))) under = res;  // (never poisons the reservoir)
        res = lerp(res, under, saturate(gWet));
        res = lerp(res, gColor, saturate(gLoad));
        gOut[id.xy] = res;
        return;
    }
    const int2 px = gRectOrigin + int2(id.xy);
    if (any(px < 0) || any(px >= int2(gDocSize))) return;
    const float2 p = float2(px) + 0.5;
    const float d = length(p - gCenter);
    const float inner = r * saturate(gHardness);
    float cov = (r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0) * saturate(r - d + 0.5);
    if (gFlags & 1) cov *= gSel.Load(int3(px, 0));
    if (cov <= 0.0) return;
    const float2 uv = (p - gCenter) / (2.0 * r) + 0.5;
    const float4 res = gRes.SampleLevel(sLinearClamp, uv, 0);
    const float4 cur = gSnap.Load(int3(px, 0));
    gOut[px] = lerp(cur, res, saturate(cov * gFlow));
}
