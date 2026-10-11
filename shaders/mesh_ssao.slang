// Screen-space ambient occlusion of the 3D view (quality): from the depth only.
// Pass 0: occlusion (view-space positions rebuilt from the depth, normal from the
// neighbors, 12 samples on a disc turned per pixel). Pass 1: 4x4 depth-aware blur,
// multiplied into the view color (background untouched).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gPass;
    float gTanY;      // tan(fov / 2)
    float gAspect;
    float gRadius;    // world units
    float gStrength;
    float gBias;
};

Texture2D<float> gDepth : register(t0);
Texture2D<float4> gAo : register(t1);   // pass 1: occlusion of pass 0
RWTexture2D<float4> gOut : register(u0); // pass 0: occlusion, pass 1: the view color

static const float kNear = 0.02, kFar = 200.0;  // MeshScene::UpdateCamera

float LinDepth(int2 p)
{
    p = clamp(p, int2(0, 0), int2(gSize) - 1);
    float z = gDepth.Load(int3(p, 0));
    return kNear * kFar / max(kFar - z * (kFar - kNear), 1e-6);
}

float3 ViewPos(float2 px, float z)
{
    float2 ndc = float2(px.x / gSize.x * 2.0 - 1.0, 1.0 - px.y / gSize.y * 2.0);
    return float3(ndc.x * gTanY * gAspect * z, ndc.y * gTanY * z, z);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    const float raw = gDepth.Load(int3(p, 0));
    if (gPass == 0)
    {
        if (raw >= 0.99999) { gOut[p] = float4(1, 1, 1, 1); return; }
        const float z = LinDepth(p);
        const float3 P = ViewPos(float2(p) + 0.5, z);
        // Normal: the smaller depth step on each axis (no smearing across silhouettes).
        float3 l = ViewPos(float2(p) + float2(-0.5, 0.5), LinDepth(p - int2(1, 0))), r = ViewPos(float2(p) + float2(1.5, 0.5), LinDepth(p + int2(1, 0)));
        float3 u = ViewPos(float2(p) + float2(0.5, -0.5), LinDepth(p - int2(0, 1))), d = ViewPos(float2(p) + float2(0.5, 1.5), LinDepth(p + int2(0, 1)));
        float3 dx = abs(r.z - P.z) < abs(P.z - l.z) ? r - P : P - l;
        float3 dy = abs(d.z - P.z) < abs(P.z - u.z) ? d - P : P - u;
        float3 n = normalize(cross(dy, dx));
        if (dot(n, -P) < 0) n = -n;
        // Disc of samples in screen space, radius = gRadius projected at this depth.
        const float rpx = gRadius * gSize.y * 0.5 / (gTanY * z);
        const float rot = frac(52.9829189 * frac(dot(float2(p), float2(0.06711056, 0.00583715)))) * 6.2831853;
        float occ = 0.0;
        const int N = 12;
        for (int i = 0; i < N; ++i)
        {
            const float a = rot + i * 2.3999632;
            const float rr = rpx * sqrt((i + 0.5) / N);
            const float2 q = float2(p) + 0.5 + float2(cos(a), sin(a)) * max(rr, 1.0);
            const int2 qi = int2(q);
            if (any(qi < 0) || any(qi >= int2(gSize))) continue;
            const float3 S = ViewPos(q, LinDepth(qi));
            const float3 v = S - P;
            const float len = length(v);
            const float fall = saturate(1.0 - len / (gRadius * 2.0));
            occ += max(dot(n, v / max(len, 1e-5)) - gBias, 0.0) * fall;
        }
        const float ao = saturate(1.0 - gStrength * occ / N * 3.0);
        gOut[p] = float4(ao, ao, ao, 1);
        return;
    }
    // Pass 1: blur (same surface only) and apply.
    if (raw >= 0.99999) return;
    const float z0 = LinDepth(p);
    float sum = 0.0, wsum = 0.0;
    for (int y = -2; y <= 1; ++y)
        for (int x = -2; x <= 1; ++x)
        {
            int2 q = clamp(p + int2(x, y), int2(0, 0), int2(gSize) - 1);
            float w = exp(-abs(LinDepth(q) - z0) / (0.02 * z0));
            sum += gAo.Load(int3(q, 0)).r * w;
            wsum += w;
        }
    const float ao = sum / max(wsum, 1e-5);
    float4 c = gOut[p];
    gOut[p] = float4(c.rgb * ao, c.a);
}
