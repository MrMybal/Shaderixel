// Convert internal (linear, premultiplied) to straight sRGB RGBA8 for saving.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gFlags;    // 1 = encode sRGB
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gDst : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float4 c = Unpremultiply(gSrc.Load(int3(id.xy, 0)));
    if (gFlags & 1) c.rgb = LinearToSrgb(c.rgb);
    gDst[id.xy] = saturate(c);
}
