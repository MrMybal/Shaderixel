// Path tracer denoiser: edge-avoiding à-trous wavelet filter (5x5 B3 spline,
// step 1, 2, 4, 8), weights from the relative color difference compared with the
// expected noise (which falls with the sample count). Pass 0 reads the accumulation.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gStep;        // 1, 2, 4, 8
    uint gFirst;       // 1: source = accumulation (rgb sum, a = samples)
    float gSigma;      // relative noise tolerance
    float gMix;        // final pass: blend with the source (0 = all filtered)
    uint gLast;
    uint gHasDepth;    // depth of the same view (raster pass): geometry edges are kept
};

Texture2D<float4> gSrc : register(t0);
Texture2D<float4> gAccum : register(t1);  // original (last pass: detail kept by gMix)
Texture2D<float> gDepth : register(t2);

float LinDepth(int2 p)
{
    p = clamp(p, int2(0, 0), int2(gSize) - 1);
    float z = gDepth.Load(int3(p, 0));
    const float n = 0.02, f = 200.0;  // MeshScene::UpdateCamera
    return n * f / max(f - z * (f - n), 1e-6);
}
RWTexture2D<float4> gOut : register(u0);

float3 Load(int2 p)
{
    p = clamp(p, int2(0, 0), int2(gSize) - 1);
    float4 c = gSrc.Load(int3(p, 0));
    return gFirst ? c.rgb / max(c.a, 1.0) : c.rgb;
}

float Lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    const float k[3] = {3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0};
    float3 c0 = Load(p);
    // Reference: the 3x3 mean around the pixel (the pixel alone is as noisy as its neighbors).
    float l0 = 0;
    for (int yy = -1; yy <= 1; ++yy)
        for (int xx = -1; xx <= 1; ++xx) l0 += Lum(Load(p + int2(xx, yy) * int(gStep)));
    l0 /= 9.0;
    const float z0 = gHasDepth ? LinDepth(p) : 0.0;
    float3 acc = 0;
    float wsum = 0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
        {
            float3 c = Load(p + int2(x, y) * int(gStep));
            float d = abs(Lum(c) - l0) / max(max(Lum(c), l0), 0.02);
            float w = k[abs(x)] * k[abs(y)] * exp(-d * d / (2.0 * gSigma * gSigma + 1e-6));
            if (gHasDepth)
            {
                float z = LinDepth(p + int2(x, y) * int(gStep));
                w *= exp(-abs(z - z0) / (0.01 * z0 * float(gStep) + 1e-4));
            }
            acc += c * w;
            wsum += w;
        }
    float3 o = acc / max(wsum, 1e-6);
    if (gLast)
    {
        float4 a = gAccum.Load(int3(p, 0));
        o = lerp(o, a.rgb / max(a.a, 1.0), gMix);
    }
    gOut[p] = float4(o, 1.0);
}
