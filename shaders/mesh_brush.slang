// Projection painting: every texel of the layer is projected through the 3D
// camera; dabs are screen-space discs, occlusion uses the viewport depth.
// Flags: 1 selection, 2 coverage already accumulated (other channels of a
// multi-channel stroke: only composite), 4 decal (gDabs[0] = center, half size;
// gDabs[1] = cos, sin of the rotation, opacity; coverage = decal alpha),
// 8 decal color (the paint color is the decal's own color), 16/64/128 symmetry X/Y/Z
// (object space: a texel is also painted when its mirror image is under a dab),
// 32 brush tip: the dab's shape is read in a tip atlas (variants side by side),
// rotated by gDabsEx (cos, sin, variant), 256 world dabs (particle simulation:
// gDabs = model-space center + radius, gDabsEx = alpha + surface normal; no camera), 2048 projection along the
// average normal (gFrames from mesh_dab_frame.slang: each dab laid on the plane of the surface under it).
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
    float gNear, gFar;
    uint2 gOrigin;  // the camera's depth range (perspective: near > 0; orthographic: near < 0)
};

cbuffer Paint : register(b1)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gColor;      // linear straight
    float4 gTip;        // extent (radii covered by a variant), variant count
    float4 gStencil;    // stencil (flag 512): screen center, half size
    float4 gStencil2;   // cos, sin of its rotation, invert, use the luminance
    float4 gStencil3;   // luminance threshold, contrast
    float4 gClip;       // flag 1024: only the texels in this rectangle (x0, y0, x1, y1: a material's UV tile)
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
Texture2D<float4> gFrames : register(t7);  // flag 2048: per dab (columns) its center, normal and disc axes

// Stencil: the image fixed on screen, the brush paints through it (outside: unchanged).
float StencilAt(float2 sp)
{
    if ((gFlags & 512) == 0) return 1.0;
    float2 rel = sp - gStencil.xy;
    float2 q = float2(gStencil2.x * rel.x + gStencil2.y * rel.y, -gStencil2.y * rel.x + gStencil2.x * rel.y) / max(gStencil.zw, 1.0);
    if (any(abs(q) > 1.0)) return 1.0;
    float4 t = gDecal.SampleLevel(sLinearClamp, q * 0.5 + 0.5, 0);
    float l = gStencil2.w > 0.5 ? saturate((dot(t.rgb, float3(0.2126, 0.7152, 0.0722)) - gStencil3.x) * gStencil3.y + 0.5) : 1.0;
    float v = l * t.a;
    return gStencil2.z > 0.5 ? 1.0 - v : v;
}

// A depth of the [0, 1] buffer as a distance along the view (world units): perspective (D3D, left-handed) or
// orthographic.
float LinearDepth(float z)
{
    if (gNear > 0.0) return gNear * gFar / max(gFar - z * (gFar - gNear), 1e-6);
    return gNear + z * (gFar - gNear);
}

// Is a surface point at depth `z` the one seen at its pixel (depth `scene`)? Compared as distances: the [0, 1]
// depth is very non-linear (near plane 0.02), a fixed tolerance on it covers several centimetres further away and
// painted through thin walls (the inside of a mug, a handle behind). The tolerance grows with the distance and
// when the surface is seen edge-on (one depth sample per pixel).
bool Visible(float z, float scene, float facing)
{
    if (scene < 0.0) return false;
    const float d = LinearDepth(z), s = LinearDepth(scene);
    const float range = gNear > 0.0 ? d : 1.0;
    const float bias = range * (0.0015 + 0.006 * saturate(1.0 - facing)) + 1e-4;
    return d <= s + bias;
}

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
RWTexture2D<float> gImpact : register(u2); // flag 4096: a separate coverage field for particle impacts

// Coverage m (accumulated stroke coverage) -> the layer: paint, erase or decal color.
void Composite(int2 px, float m, float before, float4 decal)
{
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

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    id.xy += gOrigin;
    if (any(id.xy >= gDocSize)) return;
    int2 px = int2(id.xy);
    float4 pos = gPosMap.Load(int3(px, 0));
    if (pos.a <= 0.0) return;
    if ((gFlags & 1024) && (px.x < gClip.x || px.y < gClip.y || px.x >= gClip.z || px.y >= gClip.w)) return;  // another material
    if (gFlags & 2048)
    {
        // Projection along the average normal: the texel is placed on the plane of the dab (its offset in the
        // disc's axes, in screen pixels at the dab), so the shape stays round on grazing surfaces. No depth test:
        // the texels near the plane facing the same way are painted (a thin wall's other side faces away).
        float mn = gStroke[px];
        const float beforeN = mn;
        if ((gFlags & 2) == 0)
        {
            const float3 nTex = normalize(gNrmMap.Load(int3(px, 0)).xyz);
            for (uint i = 0; i < gDabCount; ++i)
            {
                const float4 f0 = gFrames.Load(int3(i, 0, 0));
                if (f0.w <= 0.0) continue;  // (the dab missed the model)
                const float3 n = gFrames.Load(int3(i, 1, 0)).xyz, t = gFrames.Load(int3(i, 2, 0)).xyz, u = gFrames.Load(int3(i, 3, 0)).xyz;
                const float4 dab = gDabs[i];
                const float r = max(dab.z, 0.5);
                const float3 v = pos.xyz - f0.xyz;
                if (abs(dot(v, n)) * f0.w > r * 0.75) continue;  // (within three quarters of the radius of the plane)
                const float fw = saturate((dot(nTex, n) - gFacingMin) * 4.0);
                if (fw <= 0.0) continue;
                const float2 rel = float2(dot(v, t), -dot(v, u)) * f0.w;
                float cov;
                if (gFlags & 32) cov = TipCoverage(rel, r, gDabsEx[i]);
                else
                {
                    const float d = length(rel), inner = r * saturate(gHardness);
                    const float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
                    cov = min(soft, saturate(r - d + 0.5));
                }
                mn = mn + (1.0 - mn) * cov * dab.w * fw;
            }
        }
        Composite(px, mn, beforeN, float4(0, 0, 0, 0));
        return;
    }
    if (gFlags & 256)
    {
        // World dabs: spheres on the model, on the side the surface faces (no camera, no occlusion test).
        float mw = gStroke[px];
        const float beforeW = mw;
        const bool particlePaint = (gFlags & 4096) != 0;
        float impact = 0;
        if (particlePaint) impact = gImpact[px];
        const float beforeImpact = impact;
        if ((gFlags & 2) == 0)
        {
            float3 nw = normalize(gNrmMap.Load(int3(px, 0)).xyz);
            for (uint i = 0; i < gDabCount; ++i)
            {
                float4 dab = gDabs[i];
                float4 ex = gDabsEx[i];
                float r = max(dab.w, 1e-5);
                float d = length(pos.xyz - dab.xyz);
                if (d >= r || dot(nw, ex.yzw) < 0.1) continue;
                float soft = 1.0 - smoothstep(r * saturate(gHardness), r, d);
                if (particlePaint && ex.x < 0)
                    impact = impact + (1.0 - impact) * soft * (-ex.x);
                else
                    mw = mw + (1.0 - mw) * soft * ex.x;
            }
        }
        if (particlePaint)
        {
            if ((gFlags & 2) == 0)
            {
                if (mw == beforeW && impact == beforeImpact) return;
                gStroke[px] = mw;
                gImpact[px] = impact;
            }
            else if (max(mw, impact) <= 0) return;
            const float sel = (gFlags & 1) != 0 ? gSel.Load(int3(px, 0)) : 1.0;
            const float trailAlpha = saturate(mw * gOpacity * gStencil2.x) * sel * gColor.a;
            const float impactAlpha = saturate(impact * gOpacity * gStencil2.y) * sel * gStencil.a;
            const float4 trail = float4(gColor.rgb * trailAlpha, trailAlpha);
            const float4 splat = float4(gStencil.rgb * impactAlpha, impactAlpha);
            gLayer[px] = splat + (trail + gPre.Load(int3(px, 0)) * (1.0 - trailAlpha)) * (1.0 - impactAlpha);
        }
        else Composite(px, mw, beforeW, float4(0, 0, 0, 0));
        return;
    }
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
    float facingW = (onScreen && Visible(ndc.z, scene, facing) && facing > fmin) ? saturate((facing - fmin) * 4.0) : 0.0;
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
            if (all(s2 >= 0.0) && all(s2 < float2(gViewSize)))
            {
                float3 nrm = normalize(gNrmMap.Load(int3(px, 0)).xyz);
                float f = dot(normalize(nrm * mirror), normalize(gCamPos.xyz - pm));
                if (f > fmin && Visible(nm.z, gDepth.Load(int3(int2(s2), 0)), f)) { spm = s2; facingM = saturate((f - fmin) * 4.0); }
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
    const float stencilW = StencilAt(sp), stencilM = facingM > 0.0 ? StencilAt(spm) : 1.0;
    for (uint i = 0; (gFlags & 4) == 0 && (gFlags & 2) == 0 && i < gDabCount; ++i)
    {
        float4 dab = gDabs[i];
        float r = max(dab.z, 0.5);
        if (gFlags & 32)
        {
            float4 ex = gDabsEx[i];
            if (facingW > 0.0) m = m + (1.0 - m) * TipCoverage(sp - dab.xy, r, ex) * dab.w * facingW * stencilW;
            if (facingM > 0.0) m = m + (1.0 - m) * TipCoverage(spm - dab.xy, r, ex) * dab.w * facingM * stencilM;
            continue;
        }
        float d = length(sp - dab.xy);
        float inner = r * saturate(gHardness);
        float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
        float cov = min(soft, saturate(r - d + 0.5)) * dab.w * facingW * stencilW;
        m = m + (1.0 - m) * cov;
        if (facingM > 0.0)
        {
            float dm = length(spm - dab.xy);
            float softM = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, dm) : 1.0;
            float covM = min(softM, saturate(r - dm + 0.5)) * dab.w * facingM * stencilM;
            m = m + (1.0 - m) * covM;
        }
    }
    Composite(px, m, before, decal);
}
