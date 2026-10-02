// Brush stroke: dabs accumulate into a per-stroke coverage mask (flow builds up
// to 1), then the layer is rebuilt from its pre-stroke copy with opacity capped
// by the stroke opacity (Photoshop "opacity vs flow" behaviour).
#include "common.slang"

cbuffer Root : register(b0)
{
    int2 gRectOrigin;
    uint2 gRectSize;
    uint2 gDocSize;
    uint gDabCount;
    float gHardness;
    float gOpacity;
    uint gMode;         // 0 = paint, 1 = erase
    float2 _pad;
    float4 gColor;      // linear, straight
};

cbuffer Dabs : register(b1)
{
    float4 gDabs[4096]; // x, y, radius, alpha
};

Texture2D<float4> gPre : register(t0);       // layer before the stroke
RWTexture2D<float4> gLayer : register(u0);
RWTexture2D<float> gStroke : register(u1);   // stroke coverage

float DabCoverage(float2 p, float4 dab)
{
    float r = max(dab.z, 0.5);
    float d = length(p - dab.xy);
    float inner = r * saturate(gHardness);
    float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
    float aa = saturate(r - d + 0.5);          // 1px antialiased edge
    return min(soft, aa) * dab.w;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gRectSize)) return;
    int2 px = gRectOrigin + int2(id.xy);
    if (any(px < 0) || any(px >= int2(gDocSize))) return;
    float2 p = float2(px) + 0.5;

    float m = gStroke[px];
    for (uint i = 0; i < gDabCount; ++i)
    {
        float c = DabCoverage(p, gDabs[i]);
        m = m + (1.0 - m) * c;
    }
    gStroke[px] = m;

    float4 pre = gPre.Load(int3(px, 0));
    float k = saturate(m * gOpacity);
    float4 result;
    if (gMode == 1)
        result = pre * (1.0 - k);
    else
    {
        float4 paint = float4(gColor.rgb * gColor.a, gColor.a);
        result = paint * k + pre * (1.0 - k * gColor.a);   // source-over, premultiplied
    }
    gLayer[px] = result;
}
