// Select > Color Range: coverage of the pixels close to sampled colors (Lab
// distance, fuzziness), or of a preset range (hues, highlights / midtones /
// shadows, skin tones). Output: gray mask (rgb = coverage, a = 1).
#include "common.slang"

cbuffer Params : register(b1)
{
    float4 gSamples[8];   // Lab (L 0..100), w = 1 when used
    uint2 gSize;
    uint gPreset;         // 0 samples, 1 reds, 2 yellows, 3 greens, 4 cyans, 5 blues, 6 magentas, 7 highlights, 8 midtones, 9 shadows, 10 skin
    uint gInvert;
    float gFuzz;          // Lab distance of full selection (samples)
    uint gCount;
    float2 gPad;
};

Texture2D<float4> gImage : register(t0);
RWTexture2D<float4> gOut : register(u0);

float3 ToLab(float3 lin)
{
    float3 xyz = mul(float3x3(0.4124, 0.3576, 0.1805, 0.2126, 0.7152, 0.0722, 0.0193, 0.1192, 0.9505), lin);
    xyz /= float3(0.95047, 1.0, 1.08883);
    float3 f = xyz > 0.008856 ? pow(max(xyz, 1e-6), 1.0 / 3.0) : 7.787 * xyz + 16.0 / 116.0;
    return float3(116.0 * f.y - 16.0, 500.0 * (f.x - f.y), 200.0 * (f.y - f.z));
}

float3 RgbToHsv(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)), d / (q.x + 1e-10), q.x);
}

float HueBand(float h, float center)  // 1 at the center hue, 0 beyond +-1/12 turn
{
    float d = abs(frac(h - center + 0.5) - 0.5);
    return saturate(1.0 - d * 12.0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float4 c = gImage.Load(int3(id.xy, 0));
    float4 s = Unpremultiply(c);
    float3 lin = saturate(s.rgb);
    float v = 0;
    if (gPreset == 0)
    {
        float3 lab = ToLab(lin);
        for (uint i = 0; i < gCount; ++i)
        {
            float d = length(lab - gSamples[i].xyz);
            v = max(v, saturate(1.0 - d / max(gFuzz, 0.5)));
        }
        v = smoothstep(0.0, 1.0, v);
    }
    else if (gPreset <= 6)
    {
        float3 hsv = RgbToHsv(LinearToSrgb(lin));
        float centers[6] = {0.0, 1.0 / 6.0, 2.0 / 6.0, 3.0 / 6.0, 4.0 / 6.0, 5.0 / 6.0};
        v = HueBand(hsv.x, centers[gPreset - 1]) * smoothstep(0.08, 0.35, hsv.y) * smoothstep(0.05, 0.2, hsv.z);
    }
    else if (gPreset <= 9)
    {
        float l = LinearToSrgb1(Luma(lin));
        if (gPreset == 7) v = smoothstep(0.6, 0.8, l);
        else if (gPreset == 8) v = smoothstep(0.2, 0.4, l) * (1.0 - smoothstep(0.6, 0.8, l));
        else v = 1.0 - smoothstep(0.2, 0.4, l);
    }
    else
    {
        // Skin: orange hues, moderate saturation (YCbCr-like test in sRGB).
        float3 srgb = LinearToSrgb(lin);
        float cb = -0.1687 * srgb.r - 0.3313 * srgb.g + 0.5 * srgb.b;
        float cr = 0.5 * srgb.r - 0.4187 * srgb.g - 0.0813 * srgb.b;
        float dcb = (cb + 0.10) / 0.10, dcr = (cr - 0.12) / 0.09;
        v = saturate(1.0 - sqrt(dcb * dcb + dcr * dcr)) * smoothstep(0.15, 0.3, srgb.r);
        v = smoothstep(0.0, 0.6, v);
    }
    v *= step(1e-4, c.a);
    if (gInvert) v = 1.0 - v;
    gOut[id.xy] = float4(v, v, v, 1.0);
}
