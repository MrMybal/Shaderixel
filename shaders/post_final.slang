// Post-process, final step: prepared HDR + bloom -> tone curve, contrast,
// saturation, vignette, film grain, sRGB display image.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gFlags;        // 2 bloom bound, 4 raw (display values: copied)
    uint gTonemap;      // 0 linear, 1 ACES, 2 Reinhard, 3 PBR Neutral, 4 AgX, 5 soft
    float gBloom;       // intensity
    float gContrast;    // -1..1
    float gSaturation;  // 0..2
    float gVignette;    // 0..1
    float gVignetteSoft;
    float gGrain;       // 0..1
    float gTime;
    float _pad;
};

Texture2D<float4> gHdr : register(t0);
Texture2D<float4> gBloomTex : register(t1);
RWTexture2D<float4> gOut : register(u0);

float3 Aces(float3 x) { return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14)); }
float3 PbrNeutral(float3 c)
{
    const float start = 0.8 - 0.04, desat = 0.15;
    float x = min(c.r, min(c.g, c.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    c -= offset;
    float peak = max(c.r, max(c.g, c.b));
    if (peak < start) return c;
    const float d = 1.0 - start;
    float newPeak = 1.0 - d * d / (peak + d - start);
    c *= newPeak / peak;
    float g = 1.0 - 1.0 / (desat * (peak - newPeak) + 1.0);
    return lerp(c, float3(newPeak, newPeak, newPeak), g);
}
float3 AgxContrast(float3 x)
{
    float3 x2 = x * x, x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
float3 Agx(float3 c)
{
    const float3x3 inMat = float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                                    0.0784335999999992, 0.878468636469772, 0.0784336,
                                    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const float3x3 outMat = float3x3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                                     -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                                     -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    const float minEv = -12.47393, maxEv = 4.026069;
    c = mul(max(c, 1e-10), inMat);
    c = (clamp(log2(c), minEv, maxEv) - minEv) / (maxEv - minEv);
    c = AgxContrast(c);
    c = mul(c, outMat);
    return pow(max(c, 0.0), 2.2);
}
float3 Tone(float3 c)
{
    switch (gTonemap)
    {
    case 1: return Aces(c * 0.8);
    case 2: return c / (1.0 + c);
    case 3: return PbrNeutral(c);
    case 4: return Agx(c);
    case 5: return c / (1.0 + c * 0.15);
    default: return saturate(c);
    }
}

float Hash(float2 p) { return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453); }

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float3 c = gHdr.Load(int3(id.xy, 0)).rgb;
    if (gFlags & 4)
    {
        gOut[id.xy] = float4(saturate(c), 1.0);
        return;
    }
    float2 uv = (float2(id.xy) + 0.5) / float2(gSize);
    if (gHdr.Load(int3(id.xy, 0)).a < 0.5)
    {
        gOut[id.xy] = float4(0.09, 0.095, 0.11, 1.0);  // empty background of the 3D view
        return;
    }
    if (gFlags & 2) c += gBloomTex.SampleLevel(sLinearClamp, uv, 0).rgb * gBloom;
    c = saturate(Tone(max(c, 0.0)));
    // Grading in display space: contrast around mid gray, saturation around the luminance.
    float3 s = LinearToSrgb(c);
    s = saturate((s - 0.5) * (1.0 + gContrast) + 0.5);
    float l = dot(s, float3(0.2126, 0.7152, 0.0722));
    s = saturate(lerp(float3(l, l, l), s, gSaturation));
    if (gVignette > 0.0)
    {
        float2 d = (uv - 0.5) * float2(float(gSize.x) / float(gSize.y), 1.0);
        float r = length(d) * 1.2;
        float v = smoothstep(1.0 - gVignetteSoft * 0.9, 1.0 + gVignetteSoft * 0.2, r);
        s *= 1.0 - gVignette * v;
    }
    if (gGrain > 0.0)
    {
        float n = Hash(float2(id.xy) + frac(gTime) * 917.0) - 0.5;
        s = saturate(s + n * gGrain * 0.12);
    }
    gOut[id.xy] = float4(s, 1.0);
}
