// Selection from image content, combined with the previous selection.
// Ops: 0 alpha, 1 luminance, 2 color range (tolerance on sRGB values), 3 mask (red channel).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gOp;
    uint gMode;         // 0 replace, 1 add, 2 subtract, 3 intersect
    float4 gColor;      // sRGB straight target (color range)
    float gTolerance;   // 0..1
    uint gFlags;        // 1 = has previous selection, 2 = antialias edge of the range
};

Texture2D<float4> gImage : register(t0);
Texture2D<float> gPrev : register(t1);
RWTexture2D<float> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float4 c = gImage.Load(int3(id.xy, 0));
    float v;
    if (gOp == 0) v = saturate(c.a);
    else if (gOp == 1) v = saturate(LinearToSrgb1(Luma(Unpremultiply(c).rgb)));
    else if (gOp == 3) v = saturate(c.r);  // mask texture
    else
    {
        float4 s = Unpremultiply(c);
        float3 rgb = LinearToSrgb(saturate(s.rgb));
        float d = max(abs(rgb.r - gColor.r), max(abs(rgb.g - gColor.g), abs(rgb.b - gColor.b)));
        float t = max(gTolerance, 1.0 / 255.0);
        v = (gFlags & 2) ? 1.0 - smoothstep(t * 0.85, t, d) : step(d, t);
        v *= step(1e-4, c.a);
    }
    float prev = (gFlags & 1) ? gPrev.Load(int3(id.xy, 0)) : 0.0;
    float r;
    if (gMode == 1) r = max(prev, v);
    else if (gMode == 2) r = prev * (1.0 - v);
    else if (gMode == 3) r = min(prev, v);
    else r = v;
    gOut[id.xy] = r;
}
