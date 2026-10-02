// Bake the Height channel into the Normal channel (tangent space, DirectX/Unreal
// convention) for export. Channels hold visual (sRGB-encoded) values; the output
// stores the encoded normal as linear so an sRGB export writes the right bytes.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gFlags;        // 1 = has normal channel, 2 = has height channel
    float gStrength;
};

Texture2D<float4> gNormal : register(t0);
Texture2D<float4> gHeight : register(t1);
RWTexture2D<float4> gOut : register(u0);

float3 Visual(Texture2D<float4> t, int2 p, float3 def)
{
    p = clamp(p, int2(0, 0), int2(gSize) - 1);
    float4 c = t.Load(int3(p, 0));
    return lerp(def, LinearToSrgb(saturate(Unpremultiply(c).rgb)), saturate(c.a));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    float3 n = (gFlags & 1) ? Visual(gNormal, p, float3(0.5, 0.5, 1.0)) * 2.0 - 1.0 : float3(0, 0, 1);
    if (gFlags & 2)
    {
        float dhdu = (Visual(gHeight, p + int2(1, 0), 0.5).r - Visual(gHeight, p - int2(1, 0), 0.5).r) * 0.5;
        float dhdv = (Visual(gHeight, p + int2(0, 1), 0.5).r - Visual(gHeight, p - int2(0, 1), 0.5).r) * 0.5;
        n = float3(n.xy - float2(dhdu, dhdv) * gStrength * n.z, n.z);
    }
    n = normalize(n);
    gOut[id.xy] = float4(SrgbToLinear(n * 0.5 + 0.5), 1.0);
}
