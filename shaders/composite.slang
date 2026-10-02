// Blend node: out = base (+) top with blend mode, opacity and optional mask.
#include "common.slang"
#include "blend_modes.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gMode;
    float gOpacity;
    uint gFlags;    // 1 = has mask, 2 = blend in gamma (Photoshop-like), 4 = has top, 8 = has base, 16 = clip to base alpha
    int2 gOutOrigin;    // document pixel of thread (0,0) (tiled rendering)
    int2 gBaseOrigin;   // document pixel of texel (0,0) of each input texture
    int2 gTopOrigin;
    int2 gMaskOrigin;
};

Texture2D<float4> gBase : register(t0);
Texture2D<float4> gTop : register(t1);
Texture2D<float4> gMask : register(t2);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 ap = gOutOrigin + int2(id.xy);
    float4 base = (gFlags & 8) ? gBase.Load(int3(ap - gBaseOrigin, 0)) : float4(0, 0, 0, 0);
    if (!(gFlags & 4)) { gOut[id.xy] = base; return; }
    float4 top = gTop.Load(int3(ap - gTopOrigin, 0));

    float mask = (gFlags & 1) ? saturate(gMask.Load(int3(ap - gMaskOrigin, 0)).r) : 1.0;
    float as = saturate(top.a * gOpacity * mask);
    if (gMode == 13) as = Hash12(float2(ap)) <= as ? 1.0 : 0.0;   // Dissolve
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
    if (gFlags & 16)
    {
        // Clipping mask: the top only shows where the base exists; base alpha is kept.
        rgb = as * ab * mixed + (1.0 - as) * ab * cb;
        a = ab;
    }
    gOut[id.xy] = float4(rgb, a);
}
