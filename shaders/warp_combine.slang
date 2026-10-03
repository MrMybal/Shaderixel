// Warp, second pass: the warped pixels (rendered over a bounding box) over what
// stays in place (unselected pixels), written to the whole surface.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    int2 gOrigin;       // document pixel of the warped texture's texel (0,0)
    uint2 gWarpSize;
    uint gFlags;        // 1 = selection (unselected pixels stay)
};

Texture2D<float4> gOriginal : register(t0);
Texture2D<float> gSel : register(t1);
Texture2D<float4> gWarped : register(t2);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 w = int2(id.xy) - gOrigin;
    float4 moved = all(w >= 0) && all(w < int2(gWarpSize)) ? gWarped.Load(int3(w, 0)) : float4(0, 0, 0, 0);
    float4 base = 0;
    if (gFlags & 1) base = gOriginal.Load(int3(id.xy, 0)) * (1.0 - gSel.Load(int3(id.xy, 0)));
    gOut[id.xy] = moved + base * (1.0 - moved.a);
}
