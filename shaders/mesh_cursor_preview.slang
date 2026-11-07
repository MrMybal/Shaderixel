// Paint 3D: the brush's dab under the cursor drawn on the model (an overlay of the view, the paint color at half
// opacity), with the same shape as the paint: the programmable tip's first variant or the round tip with its
// hardness, laid on the plane of the average normal (flag 1, mesh_dab_frame.slang's frame of dab 0) or seen from
// the camera. Only the model's pixels show it.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gViewSize;
    float2 gCursor;     // view pixels
    float gRadius;      // pixels
    float gHardness;
    uint gFlags;        // 1 along the average normal, 2 programmable tip
    float gTipExtent;
    float gTipVariants;
    float gCos, gSin;   // the dab's rotation
    float _pad;
};

cbuffer Frame : register(b1)
{
    row_major float4x4 gInvViewProj;
    float4 gColor;      // straight sRGB-ready color of the preview
};

Texture2D<float> gDepth : register(t0);
Texture2D<float4> gFrames : register(t1);
Texture2D<float> gTipAtlas : register(t2);
RWTexture2D<float4> gOut : register(u0);

float TipCoverage(float2 rel, float r)
{
    float2 q = float2(gCos * rel.x + gSin * rel.y, -gSin * rel.x + gCos * rel.y) / r;
    float e = max(gTipExtent, 1.0);
    if (any(abs(q) >= e)) return 0.0;
    float2 uv = q / (2.0 * e) + 0.5;
    uint aw, ah;
    gTipAtlas.GetDimensions(aw, ah);
    float inset = 0.5 / float(ah);
    uv = clamp(uv, inset, 1.0 - inset);
    return saturate(gTipAtlas.SampleLevel(sLinearClamp, float2(uv.x / max(gTipVariants, 1.0), uv.y), 0));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gViewSize)) return;
    const float z = gDepth.Load(int3(id.xy, 0));
    float2 rel;
    bool inside = z < 0.99999;
    if (inside && (gFlags & 1))
    {
        const float4 f0 = gFrames.Load(int3(0, 0, 0));
        if (f0.w <= 0.0) inside = false;
        else
        {
            const float2 ndc = float2((id.x + 0.5) / gViewSize.x * 2.0 - 1.0, 1.0 - (id.y + 0.5) / gViewSize.y * 2.0);
            const float4 w = mul(gInvViewProj, float4(ndc, z, 1.0));
            const float3 v = w.xyz / w.w - f0.xyz;
            const float3 n = gFrames.Load(int3(0, 1, 0)).xyz, t = gFrames.Load(int3(0, 2, 0)).xyz, u = gFrames.Load(int3(0, 3, 0)).xyz;
            if (abs(dot(v, n)) * f0.w > gRadius * 0.75) inside = false;
            rel = float2(dot(v, t), -dot(v, u)) * f0.w;
        }
    }
    else
    {
        rel = float2(id.xy) + 0.5 - gCursor;
    }
    float cov = 0.0;
    if (inside)
    {
        const float r = max(gRadius, 0.5);
        if (gFlags & 2) cov = TipCoverage(rel, r);
        else
        {
            const float d = length(rel), inner = r * saturate(gHardness);
            const float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
            cov = min(soft, saturate(r - d + 0.5));
        }
    }
    gOut[id.xy] = float4(gColor.rgb, gColor.a * cov);
}
