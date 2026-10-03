// Retouch brushes. Same coverage model as brush.slang (dabs accumulate into a
// per-stroke mask), but the stroke applies an operation on the pre-stroke layer:
//   0 clone stamp, 1 healing, 2 blur, 3 sharpen, 4 dodge, 5 burn, 6 sponge saturate, 7 sponge desaturate,
//   8 history brush (t2 = snapshot), 9 background eraser (colors close to the dab center), 10 red eye
#include "common.slang"

cbuffer Root : register(b0)
{
    int2 gRectOrigin;
    uint2 gRectSize;
    uint2 gDocSize;
    uint gDabCount;
    float gHardness;
    float gOpacity;
    uint gMode;
    uint gFlags;        // 1 = has selection
    float gStrength;
    int2 gSrcOffset;    // clone / heal: source = destination + offset
    uint gRange;        // dodge/burn: 0 shadows, 1 midtones, 2 highlights
    float gTolerance;   // background eraser: color distance (0..1)
};

cbuffer Dabs : register(b1)
{
    float4 gDabs[4096]; // x, y, radius, alpha
};

Texture2D<float4> gPre : register(t0);
Texture2D<float> gSel : register(t1);
Texture2D<float4> gSnapshot : register(t2);  // history brush source
RWTexture2D<float4> gLayer : register(u0);
RWTexture2D<float> gStroke : register(u1);

float DabCoverage(float2 p, float4 dab)
{
    float r = max(dab.z, 0.5);
    float d = length(p - dab.xy);
    float inner = r * saturate(gHardness);
    float soft = r - inner > 1.0 ? 1.0 - smoothstep(inner, r, d) : 1.0;
    float aa = saturate(r - d + 0.5);
    return min(soft, aa) * dab.w;
}

float4 Pre(int2 p) { return gPre.Load(int3(clamp(p, int2(0, 0), int2(gDocSize) - 1), 0)); }

float4 Mean(int2 p, int s)
{
    float4 m = 0;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x)
            m += Pre(p + int2(x, y) * s);
    return m / 9.0;
}

float RangeWeight(float l)
{
    if (gRange == 0) return 1.0 - smoothstep(0.0, 0.5, l);
    if (gRange == 2) return smoothstep(0.5, 1.0, l);
    return 1.0 - abs(l - 0.5) * 2.0;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gRectSize)) return;
    int2 px = gRectOrigin + int2(id.xy);
    if (any(px < 0) || any(px >= int2(gDocSize))) return;
    float2 p = float2(px) + 0.5;

    float m = gStroke[px];
    if (gMode == 9)
    {
        // Background eraser: a dab only removes the colors close to the one under its center.
        float3 c0 = LinearToSrgb(saturate(Unpremultiply(Pre(px)).rgb));
        for (uint i = 0; i < gDabCount; ++i)
        {
            float3 cc = LinearToSrgb(saturate(Unpremultiply(Pre(int2(gDabs[i].xy))).rgb));
            float d = length(c0 - cc) / 1.732;
            float close = 1.0 - smoothstep(gTolerance * 0.7, gTolerance + 1e-3, d);
            m = m + (1.0 - m) * DabCoverage(p, gDabs[i]) * close;
        }
    }
    else
    {
        for (uint i = 0; i < gDabCount; ++i)
            m = m + (1.0 - m) * DabCoverage(p, gDabs[i]);
    }
    gStroke[px] = m;

    float k = saturate(m * gOpacity);
    if (gFlags & 1) k *= gSel.Load(int3(px, 0));
    float4 pre = Pre(px);
    float4 result = pre;
    float avgRadius = gDabCount > 0 ? gDabs[0].z : 8.0;

    if (gMode == 0)
    {
        result = Pre(px + gSrcOffset);
    }
    else if (gMode == 1)
    {
        // Texture from the source, tone from the destination (high-pass + low-pass).
        int s = max(2, int(avgRadius * 0.35));
        result = Pre(px + gSrcOffset) - Mean(px + gSrcOffset, s) + Mean(px, s);
        result = max(result, 0.0);
        result.a = saturate(result.a);
    }
    else if (gMode == 8)
    {
        result = gSnapshot.Load(int3(px, 0));
    }
    else if (gMode == 9)
    {
        result = 0;
    }
    else if (gMode == 10)
    {
        // Red eye: strongly red pixels become a dark neutral pupil.
        float4 c = Unpremultiply(pre);
        float redness = c.r - max(c.g, c.b);
        float w = smoothstep(0.03, 0.15, redness) * smoothstep(0.0, 0.05, c.r);
        float pupil = min(c.g, c.b) * (1.0 - gStrength * 0.6);
        c.rgb = lerp(c.rgb, float3(pupil, pupil, pupil), w);
        result = Premultiply(c);
    }
    else if (gMode == 2 || gMode == 3)
    {
        int s = max(1, int(round(1.0 + gStrength * 3.0)));
        float4 blur = Mean(px, s);
        result = gMode == 2 ? blur : max(pre + (pre - blur) * (gStrength * 2.0), 0.0);
        if (gMode == 3) result.a = pre.a;
    }
    else
    {
        float4 c = Unpremultiply(pre);
        float l = LinearToSrgb1(Luma(c.rgb));
        float w = RangeWeight(l) * gStrength;
        if (gMode == 4) c.rgb = c.rgb * (1.0 + w * 1.5);
        else if (gMode == 5) c.rgb = c.rgb * (1.0 - w * 0.75);
        else
        {
            float y = Luma(c.rgb);
            c.rgb = max(lerp(y.xxx, c.rgb, gMode == 6 ? 1.0 + gStrength : 1.0 - gStrength), 0.0);
        }
        result = Premultiply(c);
    }
    gLayer[px] = lerp(pre, result, k);
}
