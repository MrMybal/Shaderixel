// Smart object placement: the contents (any size) drawn on a layer surface
// through a projective transform. Bicubic sampling, 1 px antialiased edges,
// 2x2 supersampling when the contents are shrunk (less aliasing).
#include "common.slang"

cbuffer Params : register(b1)
{
    float4 gHinv0;      // inverse homography rows (layer pixel -> contents pixel)
    float4 gHinv1;
    float4 gHinv2;
    uint2 gDstSize;
    uint gSuper;        // 1 or 2 (samples per axis)
    uint gInterp;       // 0 nearest, 1 bilinear, 2 bicubic
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gOut : register(u0);

float4 CubicWeights(float t)
{
    float t2 = t * t, t3 = t2 * t;
    return float4(-0.5 * t3 + t2 - 0.5 * t, 1.5 * t3 - 2.5 * t2 + 1.0, -1.5 * t3 + 2.0 * t2 + 0.5 * t, 0.5 * t3 - 0.5 * t2);
}

float4 Sample(float2 q, uint2 size)
{
    if (gInterp == 0)
        return gSrc.Load(int3(clamp(int2(floor(q)), int2(0, 0), int2(size) - 1), 0));
    if (gInterp == 1)
        return gSrc.SampleLevel(sLinearClamp, q / float2(size), 0);
    float2 f = q - 0.5;
    int2 i = int2(floor(f));
    float2 t = f - floor(f);
    float4 wx = CubicWeights(t.x), wy = CubicWeights(t.y);
    float4 c = 0;
    [unroll] for (int y = 0; y < 4; ++y)
        [unroll] for (int x = 0; x < 4; ++x)
            c += gSrc.Load(int3(clamp(i + int2(x - 1, y - 1), int2(0, 0), int2(size) - 1), 0)) * (wx[x] * wy[y]);
    c.a = saturate(c.a);
    c.rgb = clamp(c.rgb, 0.0, c.a > 0 ? 65504.0 : 0.0);
    return c;
}

float4 At(float2 p, uint2 size)
{
    float3 h = float3(dot(gHinv0.xyz, float3(p, 1)), dot(gHinv1.xyz, float3(p, 1)), dot(gHinv2.xyz, float3(p, 1)));
    if (h.z <= 1e-6) return 0;
    float2 q = h.xy / h.z;
    float inside = saturate(min(min(q.x, size.x - q.x), min(q.y, size.y - q.y)) + 0.5);
    return inside > 0 ? Sample(q, size) * inside : 0;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gDstSize)) return;
    uint w, h;
    gSrc.GetDimensions(w, h);
    uint2 size = uint2(w, h);
    float4 c = 0;
    if (gSuper > 1)
    {
        [unroll] for (int y = 0; y < 2; ++y)
            [unroll] for (int x = 0; x < 2; ++x)
                c += At(float2(id.xy) + float2(0.25 + 0.5 * x, 0.25 + 0.5 * y), size);
        c *= 0.25;
    }
    else
    {
        c = At(float2(id.xy) + 0.5, size);
    }
    gOut[id.xy] = c;
}
