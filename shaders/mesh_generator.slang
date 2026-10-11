// Smart masks (Paint 3D): a material's mask generated from the mesh maps
// (baked AO, convexity, concavity, thickness), the world position / normal of
// each texel and a 3D noise (seamless across UV seams).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gType;      // 0 edge wear, 1 cavity dirt, 2 dust on top, 3 gradient, 4 occlusion, 5 noise, 6 thin parts, 7 full, 8 follows a layer
    uint gFlags;     // 1 invert, 2 mesh maps bound, 4 source coverage in alpha (paint layer; else red: mask), 8 painted corrections (t4)
    float gAmount;   // 0..1: how much of the surface is covered
    float gContrast; // edge sharpness (1 soft .. 20 hard)
    float gNoise;    // noise breaking the mask (0..1)
    float gNoiseScale;
    float gSeed;
    float gBlur;     // unused for now
    float2 _pad;
};

cbuffer Gen : register(b1)
{
    float4 gDir;        // dust / gradient direction (object space)
    float4 gBoundsMin;
    float4 gBoundsMax;
    float4 gAnchor;     // follows a layer: mode (0 inside, 1 border, 2 around, 3 outside), radius (texels)
};

Texture2D<float4> gPos : register(t0);
Texture2D<float4> gNrm : register(t1);
Texture2D<float4> gMaps : register(t2);
Texture2D<float4> gSource : register(t3);  // layer followed (type 8), same UV space
Texture2D<float4> gEdit : register(t4);    // painted corrections: premultiplied gray, alpha = how much they override
RWTexture2D<float4> gOut : register(u0);

float H3(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float N3(float3 x)
{
    float3 i = floor(x), f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(H3(i), H3(i + float3(1, 0, 0)), f.x), lerp(H3(i + float3(0, 1, 0)), H3(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(H3(i + float3(0, 0, 1)), H3(i + float3(1, 0, 1)), f.x), lerp(H3(i + float3(0, 1, 1)), H3(i + float3(1, 1, 1)), f.x), f.y), f.z);
}
float Fbm3(float3 p)
{
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) { s += N3(p) * a; p = p * 2.02 + 7.3; a *= 0.5; }
    return s / 0.97;
}

float Src(float2 uv)
{
    float4 c = gSource.SampleLevel(sLinearClamp, uv, 0);
    return saturate((gFlags & 4) ? c.a : c.r);
}

// Layer followed (Substance anchor-like): its coverage blurred over `radius` texels.
float Anchor(float2 uv, float noise)
{
    const float c = Src(uv);
    float b = c;
    const float r = gAnchor.y;
    if (r > 0.5)
    {
        float acc = c;
        const int N = 32;
        for (int i = 1; i <= N; ++i)
        {
            float a = i * 2.3999632, rr = sqrt(i / (float)N) * r;
            acc += Src(uv + float2(cos(a), sin(a)) * rr / float2(gSize));
        }
        b = acc / (N + 1);
    }
    const int mode = (int)(gAnchor.x + 0.5);
    if (mode == 1) return saturate(1.0 - abs(b * 2.0 - 1.0));  // band across the edge of the paint
    if (mode == 2) return saturate(b * 2.0) * (1.0 - c);       // halo around it (not on it)
    if (mode == 3) return 1.0 - b;                             // everywhere else
    return b;                                                  // on it (softened)
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 px = int2(id.xy);
    float4 pos = gPos.Load(int3(px, 0));
    if (pos.a <= 0.0) { gOut[px] = float4(0, 0, 0, 1); return; }
    float3 n = normalize(gNrm.Load(int3(px, 0)).xyz);
    float2 uv = (float2(px) + 0.5) / float2(gSize);
    float4 maps = (gFlags & 2) ? gMaps.SampleLevel(sLinearClamp, uv, 0) : float4(1, 0, 0, 1);
    float noise = Fbm3(pos.xyz * gNoiseScale + gSeed * 13.17);
    float base = 0.0;
    switch (gType)
    {
    case 0: base = maps.g; break;                                         // convex edges
    case 1: base = max(maps.b, (1.0 - maps.r) * 0.8); break;              // creases and occluded areas
    case 2: base = saturate(dot(n, normalize(gDir.xyz)) * 0.5 + 0.5); base = base * base; break;  // facing up
    case 3:
    {
        float3 d = normalize(gDir.xyz);
        float3 c = (gBoundsMin.xyz + gBoundsMax.xyz) * 0.5;
        float ext = max(dot(abs(gBoundsMax.xyz - gBoundsMin.xyz), abs(d)) * 0.5, 1e-4);
        base = saturate(dot(pos.xyz - c, d) / ext * 0.5 + 0.5);
        break;
    }
    case 4: base = 1.0 - maps.r; break;
    case 5: base = noise; break;
    case 6: base = 1.0 - maps.a; break;
    case 8: base = Anchor(uv, noise); break;
    default: base = 1.0; break;
    }
    float v = base + (noise - 0.5) * 2.0 * gNoise;
    float t = 1.0 - gAmount;
    float w = 0.5 / max(gContrast, 0.5);
    float m = (gType == 7 && gNoise <= 0.0) ? 1.0 : smoothstep(t - w, t + w, v);
    if (gFlags & 1) m = 1.0 - m;
    if (gFlags & 8)
    {
        float4 e = gEdit.Load(int3(px, 0));
        m = saturate(m * (1.0 - e.a) + e.r);
    }
    gOut[px] = float4(m, m, m, 1.0);
}
