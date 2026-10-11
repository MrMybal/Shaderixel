// Path tracer denoiser: edge-avoiding a-trous wavelet filter (5x5 B3 spline,
// step 1, 2, 4). With the first-hit guides of the path tracer, the illumination
// (color / albedo) is filtered and the albedo multiplied back: texture details stay
// sharp; normals and hit distances keep the geometry edges. Weights also compare the
// color with the expected noise (which falls with the sample count).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gStep;        // 1, 2, 4
    uint gFirst;       // 1: source = accumulation (rgb sum, a = samples)
    float gSigma;      // relative noise tolerance
    float gMix;        // final pass: blend with the source (0 = all filtered)
    uint gLast;
    uint gGuide;       // 1: guides of the path tracer (t2, t3); 2: depth of the raster view (t4)
};

Texture2D<float4> gSrc : register(t0);
Texture2D<float4> gAccum : register(t1);   // original accumulation (samples in a)
Texture2D<float4> gGuideA : register(t2);  // sum of the first-hit albedo
Texture2D<float4> gGuideN : register(t3);  // sum of the first-hit normal, w: hit distance
Texture2D<float> gDepth : register(t4);
RWTexture2D<float4> gOut : register(u0);

int2 Clamp(int2 p) { return clamp(p, int2(0, 0), int2(gSize) - 1); }

float Samples(int2 p) { return max(gAccum.Load(int3(Clamp(p), 0)).a, 1.0); }

float3 Albedo(int2 p)
{
    if (gGuide != 1) return float3(1, 1, 1);
    p = Clamp(p);
    return max(gGuideA.Load(int3(p, 0)).rgb / Samples(p), 0.02);
}

// Normal (zero on the background) and distance of the pixel.
float4 NormalDist(int2 p)
{
    p = Clamp(p);
    if (gGuide == 1)
    {
        float4 g = gGuideN.Load(int3(p, 0)) / Samples(p);
        float l = length(g.xyz);
        return float4(l > 0.2 ? g.xyz / l : float3(0, 0, 0), g.w);
    }
    if (gGuide == 2)
    {
        float z = gDepth.Load(int3(p, 0));
        const float n = 0.02, f = 200.0;  // MeshScene::UpdateCamera
        return float4(0, 0, 0, n * f / max(f - z * (f - n), 1e-6));
    }
    return float4(0, 0, 0, 0);
}

// Illumination of the source (pass 0: the accumulation divided by the albedo).
float3 Load(int2 p)
{
    p = Clamp(p);
    float4 c = gSrc.Load(int3(p, 0));
    return gFirst ? c.rgb / max(c.a, 1.0) / Albedo(p) : c.rgb;
}

float Lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    const float k[3] = {3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0};
    // Reference: the 3x3 mean around the pixel (the pixel alone is as noisy as its neighbors).
    float l0 = 0;
    for (int yy = -1; yy <= 1; ++yy)
        for (int xx = -1; xx <= 1; ++xx) l0 += Lum(Load(p + int2(xx, yy) * int(gStep)));
    l0 /= 9.0;
    const float4 g0 = NormalDist(p);
    const bool hasNormal = dot(g0.xyz, g0.xyz) > 0.5;
    float3 acc = 0;
    float wsum = 0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
        {
            int2 q = p + int2(x, y) * int(gStep);
            float3 c = Load(q);
            float d = abs(Lum(c) - l0) / max(max(Lum(c), l0), 0.02);
            float w = k[abs(x)] * k[abs(y)] * exp(-d * d / (2.0 * gSigma * gSigma + 1e-6));
            if (gGuide != 0)
            {
                float4 g = NormalDist(q);
                w *= exp(-abs(g.w - g0.w) / (0.01 * g0.w * float(gStep) + 1e-4));
                if (hasNormal) w *= pow(saturate(dot(g.xyz, g0.xyz)), 32.0);
                else if (gGuide == 1) w *= dot(g.xyz, g.xyz) > 0.5 ? 0.0 : 1.0;  // background with background
            }
            acc += c * w;
            wsum += w;
        }
    float3 o = wsum > 1e-8 ? acc / wsum : Load(p);
    if (gLast)
    {
        o *= Albedo(p);
        float4 a = gAccum.Load(int3(p, 0));
        o = lerp(o, a.rgb / max(a.a, 1.0), gMix);
    }
    gOut[p] = float4(o, 1.0);
}
