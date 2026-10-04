// Render mode: packs the project's PBR channels for the path tracer.
// Channels hold premultiplied linear values; data channels are read as their
// visual (sRGB-encoded) value like the 3D view (mesh_view.slang).
// Outputs: base (straight linear, a = coverage), ORM (roughness, metallic,
// occlusion), emission (linear), tangent normal (n * 0.5 + 0.5, height baked in).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gFlags;       // bit i: channel i present (PbrChannel order)
    float gStrength;   // height to normal tilt (same as bake_normal)
};

Texture2D<float4> gBase : register(t0);
Texture2D<float4> gRough : register(t1);
Texture2D<float4> gMetal : register(t2);
Texture2D<float4> gNormal : register(t3);
Texture2D<float4> gEmissive : register(t4);
Texture2D<float4> gAO : register(t5);
Texture2D<float4> gHeight : register(t6);
RWTexture2D<float4> gOutBase : register(u0);
RWTexture2D<float4> gOutOrm : register(u1);
RWTexture2D<float4> gOutEmis : register(u2);
RWTexture2D<float4> gOutNrm : register(u3);

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
    float4 base = float4(0.5, 0.5, 0.5, 1.0);
    base.rgb = SrgbToLinear(base.rgb);
    if (gFlags & 1)
    {
        float4 c = gBase.Load(int3(p, 0));
        base = float4(lerp(base.rgb, Unpremultiply(c).rgb, saturate(c.a)), 1.0);
    }
    float rough = (gFlags & 2) ? Visual(gRough, p, 0.5).r : 0.5;
    float metal = (gFlags & 4) ? Visual(gMetal, p, 0.0).r : 0.0;
    float ao = (gFlags & 32) ? Visual(gAO, p, 1.0).r : 1.0;
    float3 emis = (gFlags & 16) ? gEmissive.Load(int3(p, 0)).rgb : float3(0, 0, 0);
    float3 n = (gFlags & 8) ? Visual(gNormal, p, float3(0.5, 0.5, 1.0)) * 2.0 - 1.0 : float3(0, 0, 1);
    if (gFlags & 64)
    {
        float dhdu = (Visual(gHeight, p + int2(1, 0), 0.5).r - Visual(gHeight, p - int2(1, 0), 0.5).r) * 0.5;
        float dhdv = (Visual(gHeight, p + int2(0, 1), 0.5).r - Visual(gHeight, p - int2(0, 1), 0.5).r) * 0.5;
        n = float3(n.xy - float2(dhdu, dhdv) * gStrength * n.z, n.z);
    }
    n = normalize(n);
    gOutBase[id.xy] = base;
    gOutOrm[id.xy] = float4(rough, metal, ao, 1.0);
    gOutEmis[id.xy] = float4(emis, 1.0);
    gOutNrm[id.xy] = float4(n * 0.5 + 0.5, 1.0);
}
