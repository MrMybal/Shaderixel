// Render mode: accumulation (sum of radiance, a = sample count) to a display
// image: exposure, tone curve, sRGB encoding.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    float gExposure;   // multiplier
    uint gTonemap;     // 0 linear (clipped), 1 ACES, 2 Reinhard, 3 Khronos PBR Neutral, 4 AgX
};

Texture2D<float4> gAccum : register(t0);
RWTexture2D<float4> gOut : register(u0);

float3 Aces(float3 x)
{
    // Narkowicz fit of the ACES filmic curve.
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

// Khronos PBR Neutral: base colors stay true under neutral light, highlights roll off.
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

// AgX (Troy Sobotka, polynomial fit by B. Wrensch): filmic, gentle desaturation of highlights.
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

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float4 a = gAccum.Load(int3(id.xy, 0));
    float3 c = a.a > 0 ? a.rgb / a.a : float3(0, 0, 0);
    c *= gExposure;
    if (gTonemap == 1) c = Aces(c);
    else if (gTonemap == 2) c = c / (1.0 + Luma(c));
    else if (gTonemap == 3) c = PbrNeutral(c);
    else if (gTonemap == 4) c = Agx(c);
    gOut[id.xy] = float4(LinearToSrgb(saturate(c)), 1.0);
}
