// Selection-aware edits on a layer. Modes:
//   0 fill: source-over gColor with opacity
//   1 erase: remove content
//   2 keep: content * selection (copy via selection)
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gMode;
    uint gFlags;        // 1 = has selection, 2 = has coverage mask (t2)
    float4 gColor;      // linear straight
    float gOpacity;
};

Texture2D<float4> gPre : register(t0);
Texture2D<float> gSel : register(t1);
Texture2D<float> gCoverage : register(t2);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int3 p = int3(id.xy, 0);
    float4 pre = gPre.Load(p);
    float s = (gFlags & 1) ? gSel.Load(p) : 1.0;
    if (gFlags & 2) s *= gCoverage.Load(p);
    float4 r;
    if (gMode == 0)
    {
        float k = s * gOpacity * gColor.a;
        r = float4(gColor.rgb, 1.0) * k + pre * (1.0 - k);
    }
    else if (gMode == 1) r = pre * (1.0 - s * gOpacity);
    else r = pre * s;
    gOut[id.xy] = r;
}
