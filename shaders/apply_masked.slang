// Destructive filter: the effect result replaces the original where selected.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gFlags;    // 1 = selection
};

Texture2D<float4> gOriginal : register(t0);
Texture2D<float4> gResult : register(t1);
Texture2D<float> gSel : register(t2);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float s = (gFlags & 1) ? saturate(gSel.Load(int3(id.xy, 0))) : 1.0;
    gOut[id.xy] = lerp(gOriginal.Load(int3(id.xy, 0)), gResult.Load(int3(id.xy, 0)), s);
}
