// Paint 3D, projection along the average normal: for each dab (screen center, radius), its point on the model and the
// average normal of the surface under the brush disc, read from the view's depth. Output, one column per dab:
//   row 0: world center, pixels per world unit (0: the dab misses the model)
//   row 1: average normal (towards the camera)
//   row 2, 3: the disc's axes in that plane, aligned with the screen's right and up
// The brush kernel (mesh_brush.slang, flag 2048) then lays each dab on that plane instead of projecting it from the
// camera: grazing surfaces get round marks, not stretched ones.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint gCount;
    uint2 gViewSize;
    uint _pad;
};

cbuffer Frame : register(b1)
{
    row_major float4x4 gInvViewProj;
    float4 gCamPos;
    float4 gCamRight;
    float4 gCamUp;
    float4 gDabs[1024];  // screen x, y, radius px, (unused)
};

Texture2D<float> gDepth : register(t0);
RWTexture2D<float4> gOut : register(u0);

// The world point seen at a view pixel; w = 0 when the pixel shows the background.
float4 WorldAt(float2 sp)
{
    int2 p = clamp(int2(sp), int2(0, 0), int2(gViewSize) - 1);
    float z = gDepth.Load(int3(p, 0));
    if (z >= 0.99999) return float4(0, 0, 0, 0);
    float2 ndc = float2((p.x + 0.5) / gViewSize.x * 2.0 - 1.0, 1.0 - (p.y + 0.5) / gViewSize.y * 2.0);
    float4 w = mul(gInvViewProj, float4(ndc, z, 1.0));
    return float4(w.xyz / w.w, 1.0);
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const uint i = id.x;
    if (i >= gCount) return;
    const float4 dab = gDabs[i];
    const float r = max(dab.z, 1.0);
    float4 c = WorldAt(dab.xy);
    if (c.w == 0.0)
    {
        gOut[uint2(i, 0)] = float4(0, 0, 0, 0);
        return;
    }
    // Two rings of samples (half and full radius): the plane through them (sum of the fan's cross products).
    const int N = 16;
    float3 n = 0;
    float3 prev = 0, first = 0;
    bool havePrev = false, haveFirst = false;
    float scale = 0;
    int scaleCount = 0;
    [unroll] for (int ring = 0; ring < 2; ++ring)
    {
        const float rr = r * (ring == 0 ? 0.5 : 1.0);
        havePrev = false;
        haveFirst = false;
        for (int k = 0; k < N; ++k)
        {
            const float a = 6.2831853 * k / N;
            const float4 q = WorldAt(dab.xy + float2(cos(a), sin(a)) * rr);
            if (q.w == 0.0) { havePrev = false; continue; }
            const float3 d = q.xyz - c.xyz;
            if (havePrev) n += cross(prev, d);
            if (!haveFirst) { first = d; haveFirst = true; }
            prev = d;
            havePrev = true;
        }
        if (havePrev && haveFirst) n += cross(prev, first);
    }
    float len = length(n);
    if (len < 1e-12)
    {
        gOut[uint2(i, 0)] = float4(0, 0, 0, 0);
        return;
    }
    n /= len;
    if (dot(n, gCamPos.xyz - c.xyz) < 0.0) n = -n;
    // The disc's axes: the screen's right and up laid on the plane.
    float3 t = gCamRight.xyz - n * dot(gCamRight.xyz, n);
    t = length(t) > 1e-5 ? normalize(t) : normalize(cross(n, gCamUp.xyz));
    float3 u = normalize(cross(n, t));
    if (dot(u, gCamUp.xyz) < 0.0) u = -u;
    // Pixels per world unit at the dab: the outer ring's distances measured in the plane.
    for (int k2 = 0; k2 < N; ++k2)
    {
        const float a = 6.2831853 * k2 / N;
        const float4 q = WorldAt(dab.xy + float2(cos(a), sin(a)) * r);
        if (q.w == 0.0) continue;
        const float3 d = q.xyz - c.xyz;
        const float planar = length(d - n * dot(d, n));
        if (planar > 1e-6) { scale += r / planar; ++scaleCount; }
    }
    const float k = scaleCount > 0 ? scale / scaleCount : 0.0;
    gOut[uint2(i, 0)] = float4(c.xyz, k);
    gOut[uint2(i, 1)] = float4(n, 0);
    gOut[uint2(i, 2)] = float4(t, 0);
    gOut[uint2(i, 3)] = float4(u, 0);
}
