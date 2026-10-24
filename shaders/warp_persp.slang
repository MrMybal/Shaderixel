// Paste in perspective: a source image (a rectangle of a layer) laid on a plane of the document through a
// homography (document pixel -> plane coordinates 0..1), optionally repeated (tiles) over the whole plane.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gDstSize;
    uint2 gSrcSize;
};

cbuffer Params : register(b1)
{
    float4 gH0;       // plane coordinates = (H0 . p, H1 . p) / (H2 . p), p = (x, y, 1)
    float4 gH1;
    float4 gH2;
    float4 gSrcRect;  // source rectangle: x, y, width, height (pixels)
    float2 gTiles;    // repetitions across / down the plane (0: the source once, stretched over the plane)
    float gOpacity;
    float _pad;
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gDst : register(u0);

float2 Plane(float2 p)
{
    const float3 h = float3(p, 1);
    const float w = dot(gH2.xyz, h);
    return w > 1e-6 ? float2(dot(gH0.xyz, h), dot(gH1.xyz, h)) / w : float2(-1, -1);
}

float4 Fetch(float2 p)
{
    float2 uv = Plane(p);
    if (any(uv < 0.0) || any(uv > 1.0)) return 0;  // outside the plane
    if (gTiles.x > 0) uv = frac(uv * gTiles);
    // Inside the source rectangle (its outside is transparent: no seams between the tiles).
    const float2 q = clamp(gSrcRect.xy + uv * gSrcRect.zw, gSrcRect.xy + 0.5, gSrcRect.xy + gSrcRect.zw - 0.5);
    return gSrc.SampleLevel(sLinearClamp, q / float2(gSrcSize), 0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gDstSize)) return;
    const float2 p = float2(id.xy) + 0.5;
    // Footprint of the pixel in the source (far parts of the plane shrink a lot): supersampled.
    const float2 uv = Plane(p);
    const float2 k = gSrcRect.zw * max(gTiles, float2(1, 1));
    const float fp = max(length((Plane(p + float2(1, 0)) - uv) * k), length((Plane(p + float2(0, 1)) - uv) * k));
    const int n = clamp(int(ceil(fp)), 2, 6);  // at least 2 x 2: smooth plane edges
    float4 c = 0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            c += Fetch(p + (float2(x, y) + 0.5) / n - 0.5);
    gDst[id.xy] = c / float(n * n) * gOpacity;
}
