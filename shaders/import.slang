// Convert an uploaded image (RGBA8 sRGB or float linear, straight alpha) into the
// internal representation (linear, premultiplied), placed at an offset.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSrcSize;
    int2 gOffset;
    uint2 gDstSize;
    uint gFlags;    // 1 = source is sRGB encoded
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gDst : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSrcSize)) return;
    int2 dst = gOffset + int2(id.xy);
    if (any(dst < 0) || any(dst >= int2(gDstSize))) return;
    float4 c = gSrc.Load(int3(id.xy, 0));
    if (gFlags & 1) c.rgb = SrgbToLinear(c.rgb);
    gDst[dst] = Premultiply(c);
}
