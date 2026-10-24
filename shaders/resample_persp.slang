// Perspective resample (perspective crop): destination pixel -> source pixel through a 3x3 homography.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gDstSize;
    uint2 gSrcSize;
    float3 gH0;         // src = (H0 . p, H1 . p) / (H2 . p), p = (x, y, 1)
    uint gInterp;       // 0 nearest, 1 bilinear, 2 bicubic; bit 8: outside the source reads 1 (masks), else 0
    float3 gH1;
    uint _pad0;
    float3 gH2;
    uint _pad1;
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gDst : register(u0);

float4 Weights(float t)
{
    float t2 = t * t, t3 = t2 * t;
    return float4(-0.5 * t3 + t2 - 0.5 * t, 1.5 * t3 - 2.5 * t2 + 1.0, -1.5 * t3 + 2.0 * t2 + 0.5 * t, 0.5 * t3 - 0.5 * t2);
}

float4 Load(int2 p) { return gSrc.Load(int3(clamp(p, int2(0, 0), int2(gSrcSize) - 1), 0)); }

float4 Fetch(float2 q)
{
    if (any(q < 0.0) || any(q > float2(gSrcSize))) return (gInterp & 256) ? float4(1, 1, 1, 1) : float4(0, 0, 0, 0);
    const uint mode = gInterp & 255;
    if (mode == 0) return Load(int2(floor(q)));
    if (mode == 1) return gSrc.SampleLevel(sLinearClamp, q / float2(gSrcSize), 0);
    float2 f = q - 0.5;
    int2 i = int2(floor(f));
    float2 t = f - floor(f);
    float4 wx = Weights(t.x), wy = Weights(t.y);
    float4 c = 0;
    [unroll] for (int y = 0; y < 4; ++y)
        [unroll] for (int x = 0; x < 4; ++x)
            c += Load(i + int2(x - 1, y - 1)) * (wx[x] * wy[y]);
    c.a = saturate(c.a);
    c.rgb = clamp(c.rgb, 0.0, max(c.a, 1.0) * 65504.0);
    return c;
}

float2 Map(float2 p)
{
    const float3 h = float3(p, 1);
    return float2(dot(gH0, h), dot(gH1, h)) / max(dot(gH2, h), 1e-6);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gDstSize)) return;
    const float2 p = float2(id.xy) + 0.5;
    // Footprint of the pixel in the source: supersampled when it covers several source pixels.
    const float2 q = Map(p);
    const float footprint = max(length(Map(p + float2(1, 0)) - q), length(Map(p + float2(0, 1)) - q));
    const int n = (gInterp & 255) == 0 ? 1 : clamp(int(ceil(footprint)), 1, 8);
    float4 c = 0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            c += Fetch(Map(p + (float2(x, y) + 0.5) / n - 0.5));
    gDst[id.xy] = c / float(n * n);
}
