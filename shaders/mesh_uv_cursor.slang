// Paint 3D, two views: where the brush hovering the 3D view would paint, shown on the texture (UV) in the 2D view.
// Every texel of the document is placed through the 3D camera like the paint does (mesh_brush.slang): seen at its
// pixel (depth test), facing the camera, under the dab (round with its hardness, or the programmable tip), or laid on
// the plane of the average normal (flag 1, frame of dab 0 from mesh_dab_frame.slang). Output: a document-sized
// overlay, the paint color with the coverage as alpha (straight).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gDocSize;
    uint2 gViewSize;
    float2 gCursor;     // 3D view pixels
    float gRadius;      // pixels
    float gHardness;
    uint gFlags;        // 1 along the average normal, 2 programmable tip
    float gFacingMin;
    float gNear, gFar;
    float gTipExtent, gTipVariants;
    float gCos, gSin;
};

cbuffer Frame : register(b1)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gColor;
};

Texture2D<float4> gPosMap : register(t0);
Texture2D<float4> gNrmMap : register(t1);
Texture2D<float> gDepth : register(t2);
Texture2D<float4> gFrames : register(t3);
Texture2D<float> gTipAtlas : register(t4);
RWTexture2D<float4> gOut : register(u0);

float LinearDepth(float z)
{
    if (gNear > 0.0) return gNear * gFar / max(gFar - z * (gFar - gNear), 1e-6);
    return gNear + z * (gFar - gNear);
}

bool Visible(float z, float scene, float facing)
{
    if (scene < 0.0) return false;
    const float d = LinearDepth(z), s = LinearDepth(scene);
    const float range = gNear > 0.0 ? d : 1.0;
    const float bias = range * (0.0015 + 0.006 * saturate(1.0 - facing)) + 1e-4;
    return d <= s + bias;
}

static float gRing = 0.0;  // edge of the round dab (light ring), set by Coverage

float Coverage(float2 rel, float r)
{
    if (gFlags & 2)
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
    const float d = length(rel), inner = r * saturate(gHardness);
    const float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
    gRing = saturate(1.0 - abs(d - r) / 1.5);
    return min(soft, saturate(r - d + 0.5));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gDocSize)) return;
    const int2 px = int2(id.xy);
    const float4 pos = gPosMap.Load(int3(px, 0));
    float cov = 0.0;
    const float r = max(gRadius, 0.5);
    if (pos.a > 0.0)
    {
        const float3 n = normalize(gNrmMap.Load(int3(px, 0)).xyz);
        if (gFlags & 1)
        {
            const float4 f0 = gFrames.Load(int3(0, 0, 0));
            if (f0.w > 0.0)
            {
                const float3 fn = gFrames.Load(int3(0, 1, 0)).xyz, t = gFrames.Load(int3(0, 2, 0)).xyz, u = gFrames.Load(int3(0, 3, 0)).xyz;
                const float3 v = pos.xyz - f0.xyz;
                const float fw = saturate((dot(n, fn) - gFacingMin) * 4.0);
                if (abs(dot(v, fn)) * f0.w <= r * 0.75 && fw > 0.0) { cov = Coverage(float2(dot(v, t), -dot(v, u)) * f0.w, r) * fw; gRing *= fw; }
            }
        }
        else
        {
            const float4 clip = mul(gViewProj, float4(pos.xyz, 1.0));
            if (clip.w > 1e-5)
            {
                const float3 ndc = clip.xyz / clip.w;
                const float2 sp = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5) * float2(gViewSize);
                if (all(sp >= 0.0) && all(sp < float2(gViewSize)) && length(sp - gCursor) < r * max(gTipExtent, 1.0) * 1.5)
                {
                    const float facing = dot(n, normalize(gCamPos.xyz - pos.xyz));
                    if (facing > gFacingMin && Visible(ndc.z, gDepth.Load(int3(int2(sp), 0)), facing))
                    {
                        const float fw = saturate((facing - gFacingMin) * 4.0);
                        cov = Coverage(sp - gCursor, r) * fw;
                        gRing *= fw;
                    }
                }
            }
        }
    }
    gOut[px] = float4(lerp(gColor.rgb, float3(1, 1, 1), gRing), max(gColor.a * cov, gRing * 0.9));
}
