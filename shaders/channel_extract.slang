// Graph links from a single channel output (R, G, B or A): an opaque gray image
// of that channel (color channels un-premultiplied, as painted).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    int2 gOffset;    // source texel of output (0,0)
    uint gChannel;   // 1 R, 2 G, 3 B, 4 A
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    uint sw, sh;
    gSrc.GetDimensions(sw, sh);
    int2 p = clamp(int2(id.xy) + gOffset, int2(0, 0), int2(sw, sh) - 1);
    float4 c = gSrc.Load(int3(p, 0));
    float3 s = c.a > 0.0 ? c.rgb / c.a : float3(0, 0, 0);
    float v = gChannel == 4 ? c.a : gChannel == 1 ? s.r : gChannel == 2 ? s.g : s.b;
    gOut[id.xy] = float4(v, v, v, 1.0);
}
