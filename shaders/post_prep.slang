// Post-process, step 1: linear HDR image -> prepared HDR (exposure, sharpen,
// chromatic aberration, white balance). The source may be an accumulation
// (path tracer: sum of samples, alpha = count).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gFlags;        // 1 accumulated source, 4 raw (display values: copied as they are)
    float gExposure;    // multiplier
    float gSharpen;     // 0..1
    float gChroma;      // chromatic aberration, pixels at the corners
    float gTemperature; // -1 cold .. 1 warm
    float gTint;        // -1 green .. 1 magenta
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gOut : register(u0);

float3 Load(int2 p)
{
    p = clamp(p, int2(0, 0), int2(gSize) - 1);
    float4 c = gSrc.Load(int3(p, 0));
    if (gFlags & 1) return c.rgb / max(c.a, 1.0);
    return c.rgb;
}

float3 SampleAt(float2 px)
{
    // Bilinear over Load (accumulation needs the per-texel division).
    float2 p = px - 0.5;
    int2 i = int2(floor(p));
    float2 f = p - float2(i);
    return lerp(lerp(Load(i), Load(i + int2(1, 0)), f.x), lerp(Load(i + int2(0, 1)), Load(i + int2(1, 1)), f.x), f.y);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    if (gFlags & 4)
    {
        gOut[p] = float4(Load(p), 1.0);
        return;
    }
    float3 c = Load(p);
    if (gChroma > 0.0)
    {
        // Red pushed outwards, blue inwards, more at the corners.
        float2 center = float2(gSize) * 0.5;
        float2 d = (float2(p) + 0.5 - center) / center;
        float2 off = d * dot(d, d) * gChroma * 0.5;
        c.r = SampleAt(float2(p) + 0.5 + off).r;
        c.b = SampleAt(float2(p) + 0.5 - off).b;
    }
    if (gSharpen > 0.0)
    {
        float3 blur = (Load(p + int2(1, 0)) + Load(p - int2(1, 0)) + Load(p + int2(0, 1)) + Load(p - int2(0, 1))) * 0.25;
        c = max(c + (c - blur) * gSharpen * 1.5, 0.0);
    }
    // White balance: warm = more red, less blue; tint = magenta vs green.
    float3 wb = float3(1.0 + gTemperature * 0.25, 1.0 - gTint * 0.2, 1.0 - gTemperature * 0.25);
    // Alpha 0: background of the 3D view (shown as a fixed display gray by post_final).
    const float a = (gFlags & 1) ? 1.0 : gSrc.Load(int3(p, 0)).a;
    gOut[p] = float4(c * wb * gExposure, a);
}
