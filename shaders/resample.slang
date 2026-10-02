// Resample a surface into a new canvas: destination pixel -> source pixel via a
// 2x3 affine map. Used by image size, canvas size, crop, rotate and flip.
// Minification supersamples the pixel footprint to avoid aliasing.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gDstSize;
    uint2 gSrcSize;
    float4 gM;          // a b c d : src = (a*x + b*y + tx, c*x + d*y + ty)
    float2 gT;
    uint gInterp;       // 0 nearest, 1 bilinear, 2 bicubic
    uint gFlags;
    float4 gOutside;    // value outside the source
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
    if (any(q < 0.0) || any(q > float2(gSrcSize))) return gOutside;
    if (gInterp == 0) return Load(int2(floor(q)));
    if (gInterp == 1) return gSrc.SampleLevel(sLinearClamp, q / float2(gSrcSize), 0);
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

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gDstSize)) return;
    float2 p = float2(id.xy) + 0.5;
    float2 dx = float2(gM.x, gM.z), dy = float2(gM.y, gM.w);
    float footprint = max(length(dx), length(dy));
    int n = gInterp == 0 ? 1 : clamp(int(ceil(footprint)), 1, 8);
    float4 c = 0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            float2 o = (float2(x, y) + 0.5) / n - 0.5;
            float2 d = p + o;
            float2 q = float2(dot(gM.xy, d) + gT.x, dot(gM.zw, d) + gT.y);
            c += Fetch(q);
        }
    gDst[id.xy] = c / float(n * n);
}
