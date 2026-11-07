// Paint 3D, two views: where the brush hovering the texture (2D view) paints, shown on the model in the 3D view.
// Pass 0 clears the view-sized overlay; pass 1 runs over the texels of the dab's box: each one under the dab (round,
// with the hardness, in document pixels) is placed on screen through its baked position, kept when it is the surface
// seen there (depth test), and splatted over the screen area its texel covers. Output: straight color, coverage alpha.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gViewSize;
    uint2 gDocSize;
    int2 gBoxMin;       // document pixels of the dab's box
    uint2 gBoxSize;
    float2 gCenter;     // document pixels
    float gRadius;      // document pixels
    float gHardness;
    uint gPass;
    float gNear, gFar;
    float _pad;
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
RWTexture2D<float4> gOut : register(u0);

float LinearDepth(float z)
{
    if (gNear > 0.0) return gNear * gFar / max(gFar - z * (gFar - gNear), 1e-6);
    return gNear + z * (gFar - gNear);
}

bool Project(float3 p, out float2 sp, out float z)
{
    const float4 clip = mul(gViewProj, float4(p, 1.0));
    sp = 0;
    z = 0;
    if (clip.w <= 1e-5) return false;
    const float3 ndc = clip.xyz / clip.w;
    sp = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5) * float2(gViewSize);
    z = ndc.z;
    return true;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (gPass == 0)
    {
        if (all(id.xy < gViewSize)) gOut[id.xy] = float4(0, 0, 0, 0);
        return;
    }
    if (any(id.xy >= gBoxSize)) return;
    const int2 px = gBoxMin + int2(id.xy);
    if (any(px < 0) || any(px >= int2(gDocSize))) return;
    const float4 pos = gPosMap.Load(int3(px, 0));
    if (pos.a <= 0.0) return;
    const float r = max(gRadius, 0.5);
    const float d = length(float2(px) + 0.5 - gCenter), inner = r * saturate(gHardness);
    const float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
    const float cov = min(soft, saturate(r - d + 0.5));
    // A light ring on the dab's edge: visible whatever the paint color.
    const float ring = saturate(1.0 - abs(d - r) / max(1.5, r * 0.05));
    if (cov <= 0.0 && ring <= 0.0) return;
    float2 sp;
    float z;
    if (!Project(pos.xyz, sp, z) || any(sp < 0.0) || any(sp >= float2(gViewSize))) return;
    const float3 n = normalize(gNrmMap.Load(int3(px, 0)).xyz);
    const float facing = dot(n, normalize(gCamPos.xyz - pos.xyz));
    const float scene = gDepth.Load(int3(int2(sp), 0));
    const float dl = LinearDepth(z), sl = LinearDepth(scene);
    const float bias = (gNear > 0.0 ? dl : 1.0) * (0.002 + 0.008 * saturate(1.0 - facing)) + 1e-4;
    if (dl > sl + bias) return;  // hidden by another part of the model
    // Screen size of the texel: the distance to its neighbours' projections (up to 8 pixels).
    float extent = 1.0;
    for (int k = 0; k < 2; ++k)
    {
        const int2 q = px + (k == 0 ? int2(1, 0) : int2(0, 1));
        if (any(q >= int2(gDocSize))) continue;
        const float4 pq = gPosMap.Load(int3(q, 0));
        float2 sq;
        float zq;
        if (pq.a > 0.0 && Project(pq.xyz, sq, zq)) extent = max(extent, length(sq - sp));
    }
    const int hs = (int)min(ceil(extent * 0.5), 4.0);
    const float4 c = float4(lerp(gColor.rgb, float3(1, 1, 1), ring), max(gColor.a * cov, ring * 0.9));
    for (int y = -hs; y <= hs; ++y)
        for (int x = -hs; x <= hs; ++x)
        {
            const int2 o = int2(sp) + int2(x, y);
            if (all(o >= 0) && all(o < int2(gViewSize))) gOut[o] = c;
        }
}
