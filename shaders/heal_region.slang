// Region healing (Patch tool, content-aware move) and pasting a fill result.
//   mode 0  diff     -> out0 = (pre - src(p + offset)) * w, out1.r = w   (w = outside the region, source inside the image)
//   mode 3  compose  -> src(p + offset) + blurred diff / blurred weight, blended into pre by the region mask
//   mode 4  paste    -> crop texture (at gOrigin) blended into pre by the region mask
// The two blurred textures are produced with select_refine.slang (box modes):
// a membrane that carries the color differences of the border inside the region.
#include "common.slang"

cbuffer Params : register(b1)
{
    uint2 gSize;
    uint gMode;
    uint gPad0;
    int2 gOffset;       // source = destination + offset
    int2 gOrigin;       // paste: crop origin
};

Texture2D<float4> gPre : register(t0);
Texture2D<float4> gSrc : register(t1);
Texture2D<float> gMask : register(t2);
Texture2D<float4> gDiff : register(t3);     // compose: blurred diff
Texture2D<float4> gWeight : register(t4);   // compose: blurred weight (r)
RWTexture2D<float4> gOut0 : register(u0);
RWTexture2D<float4> gOut1 : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    float m = gMask.Load(int3(p, 0));
    float4 pre = gPre.Load(int3(p, 0));
    int2 q = p + gOffset;
    bool inside = all(q >= 0) && all(q < int2(gSize));
    if (gMode == 0)
    {
        float w = inside ? (1.0 - m) : 0.0;
        float4 s = inside ? gSrc.Load(int3(q, 0)) : 0;
        gOut0[p] = (pre - s) * w;
        gOut1[p] = float4(w, 0, 0, 0);
    }
    else if (gMode == 3)
    {
        float4 s = gSrc.Load(int3(clamp(q, int2(0, 0), int2(gSize) - 1), 0));
        float w = gWeight.Load(int3(p, 0)).r;
        float4 corr = w > 1e-5 ? gDiff.Load(int3(p, 0)) / w : 0;
        float4 r = max(s + corr, 0.0);
        r.a = saturate(r.a);
        r.rgb = min(r.rgb, max(r.a, 1e-4) * 65504.0);
        gOut0[p] = lerp(pre, r, m);
    }
    else
    {
        uint cw, ch;
        gSrc.GetDimensions(cw, ch);
        int2 c = p - gOrigin;
        float4 s = (all(c >= 0) && all(c < int2(cw, ch))) ? gSrc.Load(int3(c, 0)) : pre;
        gOut0[p] = lerp(pre, s, m);
    }
}
