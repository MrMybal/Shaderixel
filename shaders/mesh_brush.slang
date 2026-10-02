// Projection painting: every texel of the layer is projected through the 3D
// camera; dabs are screen-space discs, occlusion uses the viewport depth.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gDocSize;
    uint2 gViewSize;
    uint gDabCount;
    float gHardness;
    float gOpacity;
    uint gMode;         // 0 paint, 1 erase
    uint gFlags;        // 1 has selection
    float3 _pad;
};

cbuffer Paint : register(b1)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gColor;      // linear straight
    float4 gDabs[1024]; // screen x, y, radius px, alpha
};

Texture2D<float4> gPre : register(t0);
Texture2D<float4> gPosMap : register(t1);
Texture2D<float4> gNrmMap : register(t2);
Texture2D<float> gDepth : register(t3);
Texture2D<float> gSel : register(t4);
RWTexture2D<float4> gLayer : register(u0);
RWTexture2D<float> gStroke : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gDocSize)) return;
    int2 px = int2(id.xy);
    float4 pos = gPosMap.Load(int3(px, 0));
    if (pos.a <= 0.0) return;
    float4 clip = mul(gViewProj, float4(pos.xyz, 1.0));
    if (clip.w <= 1e-5) return;
    float3 ndc = clip.xyz / clip.w;
    float2 sp = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5) * float2(gViewSize);
    if (any(sp < 0.0) || any(sp >= float2(gViewSize))) return;
    // Occlusion: compare with the viewport depth (small slope-scaled bias).
    float scene = gDepth.Load(int3(int2(sp), 0));
    if (ndc.z > scene + 0.0015) return;
    float3 n = normalize(gNrmMap.Load(int3(px, 0)).xyz);
    float3 v = normalize(gCamPos.xyz - pos.xyz);
    float facing = dot(n, v);
    if (facing <= 0.05) return;
    float facingW = saturate((facing - 0.05) * 4.0);

    float m = gStroke[px];
    float before = m;
    for (uint i = 0; i < gDabCount; ++i)
    {
        float4 dab = gDabs[i];
        float r = max(dab.z, 0.5);
        float d = length(sp - dab.xy);
        float inner = r * saturate(gHardness);
        float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
        float cov = min(soft, saturate(r - d + 0.5)) * dab.w * facingW;
        m = m + (1.0 - m) * cov;
    }
    if (m == before) return;
    gStroke[px] = m;
    float k = saturate(m * gOpacity);
    if (gFlags & 1) k *= gSel.Load(int3(px, 0));
    float4 pre = gPre.Load(int3(px, 0));
    float4 result;
    if (gMode == 1) result = pre * (1.0 - k);
    else
    {
        float4 paint = float4(gColor.rgb * gColor.a, gColor.a);
        result = paint * k + pre * (1.0 - k * gColor.a);
    }
    gLayer[px] = result;
}
