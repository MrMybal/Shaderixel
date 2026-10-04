// Render mode: packs PBR channels into one tile of the path tracer's texture atlas.
// Channels hold premultiplied linear values; data channels are read as their
// visual (sRGB-encoded) value like the 3D view (mesh_view.slang). Sources of any
// size are sampled over the tile (projects, imported .sx3d, model textures).
// Outputs: base (straight linear, a = coverage), ORM (roughness, metallic,
// occlusion), emission (linear), tangent normal (n * 0.5 + 0.5, height baked in).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gOffset;     // tile origin in the atlas (pixels)
    uint gTile;        // tile size (pixels)
    uint gFlags;       // bit i: channel i present (PbrChannel order)
    float gStrength;   // height to normal tilt per source texel (same as bake_normal)
    float gDefRough;   // absent channels (imported model parts)
    float gDefMetal;
    float gPad;
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

float3 Visual(Texture2D<float4> t, float2 uv, float3 def)
{
    float4 c = t.SampleLevel(sLinearWrap, uv, 0);
    return lerp(def, LinearToSrgb(saturate(Unpremultiply(c).rgb)), saturate(c.a));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= uint2(gTile, gTile))) return;
    float2 uv = (float2(id.xy) + 0.5) / gTile;
    float4 base = float4(SrgbToLinear(float3(0.5, 0.5, 0.5)), 1.0);
    if (gFlags & 1)
    {
        float4 c = gBase.SampleLevel(sLinearWrap, uv, 0);
        base = float4(lerp(base.rgb, Unpremultiply(c).rgb, saturate(c.a)), 1.0);
    }
    float rough = (gFlags & 2) ? Visual(gRough, uv, gDefRough).r : gDefRough;
    float metal = (gFlags & 4) ? Visual(gMetal, uv, gDefMetal).r : gDefMetal;
    float ao = (gFlags & 32) ? Visual(gAO, uv, 1.0).r : 1.0;
    float3 emis = (gFlags & 16) ? gEmissive.SampleLevel(sLinearWrap, uv, 0).rgb : float3(0, 0, 0);
    float3 n = (gFlags & 8) ? Visual(gNormal, uv, float3(0.5, 0.5, 1.0)) * 2.0 - 1.0 : float3(0, 0, 1);
    if (gFlags & 64)
    {
        uint hw, hh;
        gHeight.GetDimensions(hw, hh);
        float2 dx = float2(1.0 / hw, 0), dy = float2(0, 1.0 / hh);
        float dhdu = (Visual(gHeight, uv + dx, 0.5).r - Visual(gHeight, uv - dx, 0.5).r) * 0.5;
        float dhdv = (Visual(gHeight, uv + dy, 0.5).r - Visual(gHeight, uv - dy, 0.5).r) * 0.5;
        n = float3(n.xy - float2(dhdu, dhdv) * gStrength * n.z, n.z);
    }
    n = normalize(n);
    uint2 o = gOffset + id.xy;
    gOutBase[o] = base;
    gOutOrm[o] = float4(rough, metal, ao, 1.0);
    gOutEmis[o] = float4(emis, 1.0);
    gOutNrm[o] = float4(n * 0.5 + 0.5, 1.0);
}
