// Projection painting: every texel of the layer is projected through the 3D
// camera; dabs are screen-space discs, occlusion uses the viewport depth.
// Flags: 1 selection, 2 coverage already accumulated (other channels of a
// multi-channel stroke: only composite), 4 decal (gDabs[0] = center, half size;
// gDabs[1] = cos, sin of the rotation, opacity; coverage = decal alpha),
// 8 decal color (the paint color is the decal's own color), 16/64/128 symmetry X/Y/Z
// (object space: a texel is also painted when its mirror image is under a dab),
// 32 brush tip: the dab's shape is read in a tip atlas (variants side by side),
// rotated by gDabsEx (cos, sin, variant).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gDocSize;
    uint2 gViewSize;
    uint gDabCount;
    float gHardness;
    float gOpacity;
    uint gMode;         // 0 paint, 1 erase
    uint gFlags;        // 1 has selection, 2 coverage only read, 4 decal, 8 decal color
    float gFacingMin;   // texels seen more edge-on than this (dot(n, v)) are not painted
    float2 _pad;
};

cbuffer Paint : register(b1)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gColor;      // linear straight
    float4 gTip;        // extent (radii covered by a variant), variant count
    float4 gDabs[1024]; // screen x, y, radius px, alpha
    float4 gDabsEx[1024]; // cos, sin of the dab angle, variant
};

Texture2D<float4> gPre : register(t0);
Texture2D<float4> gPosMap : register(t1);
Texture2D<float4> gNrmMap : register(t2);
Texture2D<float> gDepth : register(t3);
Texture2D<float> gSel : register(t4);
Texture2D<float4> gDecal : register(t5);
Texture2D<float> gTipAtlas : register(t6);

float TipCoverage(float2 rel, float r, float4 ex)
{
    // rel: screen offset from the dab center. The variant covers [-extent, extent] radii.
    float2 q = float2(ex.x * rel.x + ex.y * rel.y, -ex.y * rel.x + ex.x * rel.y) / r;
    float e = max(gTip.x, 1.0);
    if (any(abs(q) >= e)) return 0.0;
    float2 uv = q / (2.0 * e) + 0.5;
    uint aw, ah;
    gTipAtlas.GetDimensions(aw, ah);
    float tiles = max(gTip.y, 1.0);
    float inset = 0.5 / float(ah);
    uv = clamp(uv, inset, 1.0 - inset);
    return saturate(gTipAtlas.SampleLevel(sLinearClamp, float2((ex.z + uv.x) / tiles, uv.y), 0));
}
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
    // Occlusion: compare with the viewport depth (small slope-scaled bias); hidden texels may still be
    // painted through the symmetry (their weight is 0 for the direct dabs).
    bool onScreen = all(sp >= 0.0) && all(sp < float2(gViewSize));
    float scene = onScreen ? gDepth.Load(int3(int2(sp), 0)) : -1.0;
    float3 n = normalize(gNrmMap.Load(int3(px, 0)).xyz);
    float3 v = normalize(gCamPos.xyz - pos.xyz);
    float facing = dot(n, v);
    const float fmin = gFacingMin;
    float facingW = (onScreen && ndc.z <= scene + 0.0015 && facing > fmin) ? saturate((facing - fmin) * 4.0) : 0.0;
    const uint symFlags = gFlags & (16u | 64u | 128u);
    if (facingW <= 0.0 && symFlags == 0) return;

    // Symmetry: the mirrored point, where it shows on screen (visible and facing).
    float2 spm = float2(-1e9, -1e9);
    float facingM = 0.0;
    if (symFlags != 0)
    {
        float3 mirror = (gFlags & 16) ? float3(-1, 1, 1) : (gFlags & 64) ? float3(1, -1, 1) : float3(1, 1, -1);
        float3 pm = pos.xyz * mirror;
        float4 cm = mul(gViewProj, float4(pm, 1.0));
        if (cm.w > 1e-5)
        {
            float3 nm = cm.xyz / cm.w;
            float2 s2 = float2(nm.x * 0.5 + 0.5, 0.5 - nm.y * 0.5) * float2(gViewSize);
            if (all(s2 >= 0.0) && all(s2 < float2(gViewSize)) && nm.z <= gDepth.Load(int3(int2(s2), 0)) + 0.0015)
            {
                float3 nrm = normalize(gNrmMap.Load(int3(px, 0)).xyz);
                float f = dot(normalize(nrm * mirror), normalize(gCamPos.xyz - pm));
                if (f > fmin) { spm = s2; facingM = saturate((f - fmin) * 4.0); }
            }
        }
    }
    float m = gStroke[px];
    float before = m;
    float4 decal = 0;
    if (gFlags & 4)
    {
        // Decal: the image placed on screen (rotated rectangle), projected like the dabs.
        float2 rel = sp - gDabs[0].xy;
        float2 cr = gDabs[1].xy;
        float2 q = float2(cr.x * rel.x + cr.y * rel.y, -cr.y * rel.x + cr.x * rel.y) / max(gDabs[0].zw, 1.0);
        if (all(abs(q) <= 1.0))
        {
            uint dw, dh;
            gDecal.GetDimensions(dw, dh);
            decal = gDecal.SampleLevel(sLinearClamp, q * 0.5 + 0.5, 0);  // straight sRGB-encoded
            float cov = decal.a * gDabs[1].z * facingW;
            if (!(gFlags & 2)) m = m + (1.0 - m) * cov;
        }
        else if (gFlags & 2) return;
    }
    for (uint i = 0; (gFlags & 4) == 0 && (gFlags & 2) == 0 && i < gDabCount; ++i)
    {
        float4 dab = gDabs[i];
        float r = max(dab.z, 0.5);
        if (gFlags & 32)
        {
            float4 ex = gDabsEx[i];
            if (facingW > 0.0) m = m + (1.0 - m) * TipCoverage(sp - dab.xy, r, ex) * dab.w * facingW;
            if (facingM > 0.0) m = m + (1.0 - m) * TipCoverage(spm - dab.xy, r, ex) * dab.w * facingM;
            continue;
        }
        float d = length(sp - dab.xy);
        float inner = r * saturate(gHardness);
        float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
        float cov = min(soft, saturate(r - d + 0.5)) * dab.w * facingW;
        m = m + (1.0 - m) * cov;
        if (facingM > 0.0)
        {
            float dm = length(spm - dab.xy);
            float softM = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, dm) : 1.0;
            float covM = min(softM, saturate(r - dm + 0.5)) * dab.w * facingM;
            m = m + (1.0 - m) * covM;
        }
    }
    if ((gFlags & 2) == 0)
    {
        if (m == before) return;
        gStroke[px] = m;
    }
    else if (m <= 0.0) return;
    float k = saturate(m * gOpacity);
    if (gFlags & 1) k *= gSel.Load(int3(px, 0));
    float4 pre = gPre.Load(int3(px, 0));
    float4 result;
    if (gMode == 1) result = pre * (1.0 - k);
    else
    {
        float4 col = gColor;
        if ((gFlags & 8) && decal.a > 0.0) col = float4(SrgbToLinear(decal.rgb), 1.0);
        float4 paint = float4(col.rgb * col.a, col.a);
        result = paint * k + pre * (1.0 - k * gColor.a);
    }
    gLayer[px] = result;
}
