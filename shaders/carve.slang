// Content-aware resize: every output pixel reads the source through the seam carving maps (computed on the
// CPU, possibly on a reduced copy). mapY: output (column, row) -> row of the width-carved image; mapX: that
// (column, row) -> source column. Both are interpolated, so a reduced map scales to the full image.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gDstSize;
    uint2 gSrcSize;
    uint2 gMapXSize;    // reduced new width x reduced height
    uint2 gMapYSize;    // reduced new width x reduced new height
    float2 gRedSize;    // reduced source size
    uint2 _pad;
};

Texture2D<float4> gSrc : register(t0);
Texture2D<float> gMapX : register(t1);
Texture2D<float> gMapY : register(t2);
RWTexture2D<float4> gDst : register(u0);

// Bilinear read of a map at continuous texel coordinates (texel i covers [i, i + 1]).
float MapAt(Texture2D<float> t, uint2 size, float2 q)
{
    float2 f = q - 0.5;
    int2 i = int2(floor(f));
    float2 a = f - float2(i);
    int2 m = int2(size) - 1;
    float v00 = t.Load(int3(clamp(i, int2(0, 0), m), 0));
    float v10 = t.Load(int3(clamp(i + int2(1, 0), int2(0, 0), m), 0));
    float v01 = t.Load(int3(clamp(i + int2(0, 1), int2(0, 0), m), 0));
    float v11 = t.Load(int3(clamp(i + int2(1, 1), int2(0, 0), m), 0));
    return lerp(lerp(v00, v10, a.x), lerp(v01, v11, a.x), a.y);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gDstSize)) return;
    float2 p = float2(id.xy) + 0.5;
    float2 qy = p * float2(gMapYSize) / float2(gDstSize);
    float ry = MapAt(gMapY, gMapYSize, qy);
    float rx = MapAt(gMapX, gMapXSize, float2(qy.x, ry + 0.5));
    float2 s = (float2(rx, ry) + 0.5) * float2(gSrcSize) / gRedSize;
    gDst[id.xy] = gSrc.SampleLevel(sLinearClamp, s / float2(gSrcSize), 0);
}
