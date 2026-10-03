// Warp (Edit > Transform > Warp): the source region drawn as a tessellated
// bicubic Bezier patch. Vertices carry their destination position (document
// pixels) and the source pixel they come from; the target covers `gOrigin` +
// its size (bounding box of the warped patch).
#include "common.slang"

cbuffer Warp : register(b1)
{
    float4 gSrcRect;    // x0 y0 x1 y1: source region (document pixels)
    float2 gOrigin;     // document pixel of the target's texel (0,0)
    float2 gTarget;     // target size
    float2 gDocSize;    // source texture size
    uint gFlags;        // 1 = selection, 2 = mask mode (source is a selection, written to every channel)
};

Texture2D<float4> gSrc : register(t0);
Texture2D<float> gSel : register(t1);

struct VSIn
{
    float3 pos : POSITION;   // destination, document pixels
    float3 nrm : NORMAL;     // unused
    float2 uv : TEXCOORD0;   // source, document pixels
};
struct VSOut
{
    float4 clip : SV_Position;
    float2 src : TEXCOORD0;
};

VSOut vsMain(VSIn i)
{
    VSOut o;
    float2 t = (i.pos.xy - gOrigin) / gTarget;
    o.clip = float4(t.x * 2.0 - 1.0, 1.0 - t.y * 2.0, 0.5, 1.0);
    o.src = i.uv;
    return o;
}

float4 psMain(VSOut i) : SV_Target0
{
    float2 q = i.src;
    float2 uv = q / gDocSize;
    // Soft clip to the source region (1px antialiased edge).
    float inside = saturate(min(min(q.x - gSrcRect.x, gSrcRect.z - q.x), min(q.y - gSrcRect.y, gSrcRect.w - q.y)) + 0.5);
    if (gFlags & 2)
    {
        float m = gSrc.SampleLevel(sLinearClamp, uv, 0).r * inside;
        return float4(m, m, m, m);
    }
    float sel = (gFlags & 1) ? gSel.SampleLevel(sLinearClamp, uv, 0) : 1.0;
    return gSrc.SampleLevel(sLinearClamp, uv, 0) * sel * inside;
}
