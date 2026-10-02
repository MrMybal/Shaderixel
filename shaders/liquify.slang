// Liquify brush. The stroke keeps a displacement field D (out(p) = pre(p + D(p))),
// so every dab resamples the pre-stroke pixels once instead of blurring them
// repeatedly. A dab maps each pixel p to a source point q of the current
// result; composing gives D'(p) = D(q) + (q - p).
#include "common.slang"

cbuffer Root : register(b0)
{
    int2 gRectOrigin;   // dab rectangle (document pixels)
    uint2 gRectSize;
    uint2 gDocSize;
    float2 gCenter;
    float2 gDelta;      // push: brush motion since the previous dab
    float gRadius;
    float gHardness;
    float gStrength;
    uint gMode;         // 0 push, 1 twirl cw, 2 twirl ccw, 3 pucker, 4 bloat, 5 reconstruct
    uint gFlags;        // 1 = selection
};

Texture2D<float4> gPre : register(t0);    // pixels before the stroke
Texture2D<float4> gSnap : register(t1);   // D over the dab rectangle before this dab
Texture2D<float> gSel : register(t2);
RWTexture2D<float4> gDisp : register(u0);
RWTexture2D<float4> gOut : register(u1);

float2 SnapAt(float2 q)
{
    // D is stored at pixel centers; bilinear inside the valid snapshot rectangle
    // (the snapshot texture may be larger than the rectangle).
    uint tw, th;
    gSnap.GetDimensions(tw, th);
    float2 local = clamp(q - float2(gRectOrigin), 0.5, float2(gRectSize) - 0.5);
    return gSnap.SampleLevel(sLinearClamp, local / float2(tw, th), 0).xy;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gRectSize)) return;
    int2 p = gRectOrigin + int2(id.xy);
    if (any(p < 0) || any(p >= int2(gDocSize))) return;
    float2 pc = float2(p) + 0.5;
    float2 rel = pc - gCenter;
    float t = length(rel) / max(gRadius, 1e-3);
    if (t >= 1.0) return;
    // Smooth falloff; hardness widens the full-strength core.
    float w = 1.0 - smoothstep(min(gHardness, 0.95), 1.0, t);
    w *= w * (3.0 - 2.0 * w);
    float k = gStrength * w;

    float2 dOld = gSnap.Load(int3(id.xy, 0)).xy;
    float2 dNew;
    if (gMode == 5)
    {
        dNew = dOld * (1.0 - 0.25 * k);
    }
    else
    {
        float2 q = pc;
        if (gMode == 0) q = pc - gDelta * k;
        else if (gMode <= 2)
        {
            float a = (gMode == 1 ? 1.0 : -1.0) * 0.12 * k;
            float s = sin(-a), c = cos(-a);
            q = gCenter + float2(c * rel.x - s * rel.y, s * rel.x + c * rel.y);
        }
        else
        {
            float f = gMode == 3 ? 1.0 + 0.06 * k : 1.0 - 0.06 * k;
            q = gCenter + rel * f;
        }
        dNew = SnapAt(q) + (q - pc);
    }
    if (gFlags & 1)
    {
        float sel = saturate(gSel.Load(int3(p, 0)));
        dNew = lerp(dOld, dNew, sel);
    }
    gDisp[p] = float4(dNew, 0, 0);
    gOut[p] = gPre.SampleLevel(sLinearClamp, (pc + dNew) / float2(gDocSize), 0);
}
