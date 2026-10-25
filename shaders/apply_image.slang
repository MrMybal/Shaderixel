// Image > Apply Image helpers. Pass 0: the source prepared (one channel as gray, or RGB; inverted). Pass 1: the
// blended result kept inside the layer's own alpha (preserve transparency).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gPass;
    uint gChannel;  // 0 RGB, 1 red, 2 green, 3 blue, 4 alpha, 5 gray (luminance)
    uint gInvert;
    uint gChannelOnly;  // Calculations: the channel as an opaque 0..1 gray (transparency ignored)
    uint2 _pad;
};

Texture2D<float4> gA : register(t0);  // pass 0: the source; pass 1: the blended result
Texture2D<float4> gB : register(t1);  // pass 1: the layer before
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    const float4 a = gA.Load(int3(id.xy, 0));
    if (gPass == 1)
    {
        const float4 b = gB.Load(int3(id.xy, 0));
        gOut[id.xy] = float4(Unpremultiply(a).rgb * b.a, b.a);
        return;
    }
    float4 s = Unpremultiply(a);
    // (HDR images kept finite: a glow can overflow to infinity; NaN removed)
    if (any(isnan(s.rgb))) s.rgb = 0;
    s.rgb = clamp(s.rgb, -60000.0, 60000.0);
    if (gChannel == 4) s = float4(SrgbToLinear(s.aaa), 1);  // (alpha shown as a gray: 0.5 -> mid gray)
    else if (gChannel == 5) s = float4(Luma(s.rgb).xxx, s.a);
    else if (gChannel >= 1) s = float4(s[gChannel - 1].xxx, s.a);
    if (gChannelOnly) s = float4(saturate(s.rgb), 1);  // (a channel: 0..1, opaque)
    if (gInvert)
    {
        // Inverted in the display space (like Photoshop's Invert): sRGB.
        s.rgb = SrgbToLinear(1.0 - LinearToSrgb(saturate(s.rgb)));
    }
    gOut[id.xy] = Premultiply(s);
}
