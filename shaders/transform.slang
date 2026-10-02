// Projective transform of a layer (free transform / move).
// Without selection the whole layer moves; with a selection only the selected
// pixels float over what remains.
#include "common.slang"

cbuffer Params : register(b1)
{
    float4 gHinv0;      // inverse homography rows (dest pixel -> source pixel)
    float4 gHinv1;
    float4 gHinv2;
    float4 gSrcRect;    // x0 y0 x1 y1: source region that moves
    uint2 gSize;
    uint gInterp;       // 0 nearest, 1 bilinear, 2 bicubic
    uint gFlags;        // 1 = has selection
};

Texture2D<float4> gSrc : register(t0);
Texture2D<float> gSel : register(t1);
RWTexture2D<float4> gOut : register(u0);

float4 CubicWeights(float t)
{
    // Catmull-Rom
    float t2 = t * t, t3 = t2 * t;
    return float4(-0.5 * t3 + t2 - 0.5 * t, 1.5 * t3 - 2.5 * t2 + 1.0, -1.5 * t3 + 2.0 * t2 + 0.5 * t, 0.5 * t3 - 0.5 * t2);
}

float4 LoadSrc(int2 p) { return gSrc.Load(int3(clamp(p, int2(0, 0), int2(gSize) - 1), 0)); }
float LoadSel(int2 p) { return gSel.Load(int3(clamp(p, int2(0, 0), int2(gSize) - 1), 0)); }

float4 SampleSrc(float2 q, out float sel)
{
    bool hasSel = (gFlags & 1) != 0;
    if (gInterp == 0)
    {
        int2 p = int2(floor(q));
        sel = hasSel ? LoadSel(p) : 1.0;
        return LoadSrc(p);
    }
    float2 uv = q / float2(gSize);
    if (gInterp == 1)
    {
        sel = hasSel ? gSel.SampleLevel(sLinearClamp, uv, 0) : 1.0;
        return gSrc.SampleLevel(sLinearClamp, uv, 0);
    }
    float2 f = q - 0.5;
    int2 i = int2(floor(f));
    float2 t = f - floor(f);
    float4 wx = CubicWeights(t.x), wy = CubicWeights(t.y);
    float4 c = 0;
    float s = 0;
    [unroll] for (int y = 0; y < 4; ++y)
        [unroll] for (int x = 0; x < 4; ++x)
        {
            float w = wx[x] * wy[y];
            c += LoadSrc(i + int2(x - 1, y - 1)) * w;
            if (hasSel) s += LoadSel(i + int2(x - 1, y - 1)) * w;
        }
    sel = hasSel ? saturate(s) : 1.0;
    c.a = saturate(c.a);
    c.rgb = max(c.rgb, 0.0);
    return c;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float2 p = float2(id.xy) + 0.5;
    float3 h = float3(dot(gHinv0.xyz, float3(p, 1)), dot(gHinv1.xyz, float3(p, 1)), dot(gHinv2.xyz, float3(p, 1)));
    float4 moved = 0;
    if (h.z > 1e-6)
    {
        float2 q = h.xy / h.z;
        // Soft clip to the source region (1px antialiased edge).
        float inside = saturate(min(min(q.x - gSrcRect.x, gSrcRect.z - q.x), min(q.y - gSrcRect.y, gSrcRect.w - q.y)) + 0.5);
        if (inside > 0)
        {
            float sel;
            moved = SampleSrc(q, sel) * sel * inside;
        }
    }
    float4 base = 0;
    if (gFlags & 1)
    {
        float s = gSel.Load(int3(id.xy, 0));
        base = gSrc.Load(int3(id.xy, 0)) * (1.0 - s);
    }
    gOut[id.xy] = moved + base * (1.0 - moved.a);
}
