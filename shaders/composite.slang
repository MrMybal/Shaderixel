// Blend node: out = base (+) top with blend mode, opacity and optional mask.
#include "common.slang"
#include "blend_modes.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gMode;
    float gOpacity;
    uint gFlags;    // 1 = has mask, 2 = blend in gamma (Photoshop-like), 4 = has top, 8 = has base
};

Texture2D<float4> gBase : register(t0);
Texture2D<float4> gTop : register(t1);
Texture2D<float4> gMask : register(t2);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int3 p = int3(id.xy, 0);
    float4 base = (gFlags & 8) ? gBase.Load(p) : float4(0, 0, 0, 0);
    if (!(gFlags & 4)) { gOut[id.xy] = base; return; }
    float4 top = gTop.Load(p);

    float mask = (gFlags & 1) ? saturate(gMask.Load(p).r) : 1.0;
    float as = saturate(top.a * gOpacity * mask);
    if (gMode == 13) as = Hash12(float2(id.xy)) <= as ? 1.0 : 0.0;   // Dissolve
    float ab = saturate(base.a);

    float3 cs = Unpremultiply(top).rgb;
    float3 cb = Unpremultiply(base).rgb;
    float3 mixed;
    if (gFlags & 2)
        mixed = SrgbToLinear(BlendColor(LinearToSrgb(cb), LinearToSrgb(cs), gMode));
    else
        mixed = BlendColor(cb, cs, gMode);

    // W3C compositing: separable blend then source-over, premultiplied result.
    float3 rgb = as * (1.0 - ab) * cs + as * ab * mixed + (1.0 - as) * ab * cb;
    float a = as + ab * (1.0 - as);
    gOut[id.xy] = float4(rgb, a);
}
