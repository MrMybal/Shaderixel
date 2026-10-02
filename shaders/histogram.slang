// Histogram of an image: 4 rows x 256 bins (luminance, R, G, B) of the straight
// sRGB 8-bit values, weighted by alpha (x255). Large images are subsampled
// with a stride (gStep) to bound the work and keep the counts within 32 bits.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gStep;
    uint gClear;    // 1: zero the bins
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<uint> gBins : register(u0);

groupshared uint sBins[1024];

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    if (gClear)
    {
        if (id.x < 256 && id.y < 4) gBins[id.xy] = 0;
        return;
    }
    for (uint i = gi; i < 1024; i += 256) sBins[i] = 0;
    GroupMemoryBarrierWithGroupSync();
    uint2 p = id.xy * gStep;
    if (all(p < gSize))
    {
        float4 c = gSrc.Load(int3(p, 0));
        if (c.a > 0.002)
        {
            float3 s = saturate(LinearToSrgb(saturate(c.rgb / c.a)));
            uint3 b = min(uint3(s * 255.0 + 0.5), 255u);
            uint w = uint(saturate(c.a) * 255.0 + 0.5);
            uint y = min(uint(dot(s, float3(0.299, 0.587, 0.114)) * 255.0 + 0.5), 255u);
            InterlockedAdd(sBins[y], w);
            InterlockedAdd(sBins[256 + b.r], w);
            InterlockedAdd(sBins[512 + b.g], w);
            InterlockedAdd(sBins[768 + b.b], w);
        }
    }
    GroupMemoryBarrierWithGroupSync();
    for (uint i = gi; i < 1024; i += 256)
        if (sBins[i]) InterlockedAdd(gBins[uint2(i & 255, i >> 8)], sBins[i]);
}
