// Select and Mask: edge refinement of a selection with a guided filter (the
// image guides the mask: hair, fur, foliage), then smoothing, feather,
// contrast and edge shift; and the preview shown on the canvas.
//   mode 0  prep      A = image, B = selection        -> (I, p, I*I, I*p)
//   mode 1  box H     A = src                          -> horizontal box mean, radius gRadius
//   mode 2  box V     A = src                          -> vertical box mean
//   mode 3  coeffs    A = means(I, p, II, Ip)          -> (a, b, mean p, 0)
//   mode 4  output    A = image, B = means(a, b, mp)   -> refined mask in band (gray)
//   mode 5  finish    A = mask                         -> contrast + shift (gray)
//   mode 6  preview   A = image, B = mask              -> view (premultiplied color)
#include "common.slang"

cbuffer Params : register(b1)
{
    uint2 gSize;
    uint gMode;
    int gRadius;
    float gEps;
    float gContrast;    // 0..1
    float gShift;       // -1..1
    uint gView;         // preview: 0 overlay, 1 black & white, 2 on black, 3 on white, 4 layer only
    float gBand;        // mode 4: refine only near the edge (1) or everywhere (0)
    float3 gPad;
};

Texture2D<float4> gA : register(t0);
Texture2D<float4> gB : register(t1);
RWTexture2D<float4> gOut : register(u0);

float Guide(float4 c)
{
    float4 s = Unpremultiply(c);
    return LinearToSrgb1(Luma(saturate(s.rgb))) * saturate(c.a) + 0.5 * (1.0 - saturate(c.a));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    if (gMode == 0)
    {
        float I = Guide(gA.Load(int3(p, 0)));
        float s = gB.Load(int3(p, 0)).r;
        gOut[p] = float4(I, s, I * I, I * s);
    }
    else if (gMode == 1 || gMode == 2)
    {
        int2 dir = gMode == 1 ? int2(1, 0) : int2(0, 1);
        float4 sum = 0;
        for (int k = -gRadius; k <= gRadius; ++k)
            sum += gA.Load(int3(clamp(p + dir * k, int2(0, 0), int2(gSize) - 1), 0));
        gOut[p] = sum / float(2 * gRadius + 1);
    }
    else if (gMode == 3)
    {
        float4 m = gA.Load(int3(p, 0));
        float var = m.z - m.x * m.x, cov = m.w - m.x * m.y;
        float a = cov / (var + gEps);
        gOut[p] = float4(a, m.y - a * m.x, m.y, 0);
    }
    else if (gMode == 4)
    {
        float I = Guide(gA.Load(int3(p, 0)));
        float4 ab = gB.Load(int3(p, 0));
        float q = saturate(ab.x * I + ab.y);
        // Original selection where it is clearly inside / outside (far from the edge).
        float meanP = ab.z;
        float band = gBand > 0.5 ? 1.0 - smoothstep(0.42, 0.5, abs(meanP - 0.5)) : 1.0;
        float orig = meanP > 0.5 ? 1.0 : 0.0;
        q = lerp(orig, q, band);
        gOut[p] = float4(q, q, q, 1);
    }
    else if (gMode == 5)
    {
        float q = gA.Load(int3(p, 0)).r;
        q = saturate(q + gShift * 0.5);
        float k = 1.0 + gContrast * 30.0;
        q = saturate((q - 0.5) * k + 0.5);
        gOut[p] = float4(q, q, q, 1);
    }
    else
    {
        float4 c = gA.Load(int3(p, 0));
        float m = gB.Load(int3(p, 0)).r;
        float4 r;
        float4 under = c + float4(0.5, 0.5, 0.5, 1) * (1 - c.a);  // over gray
        if (gView == 0) r = lerp(lerp(under, float4(0.8, 0.05, 0.08, 1), 0.55), under, m);  // red overlay outside
        else if (gView == 1) r = float4(m, m, m, 1);
        else if (gView == 2) r = c * m + float4(0, 0, 0, 1) * (1 - c.a * m);
        else if (gView == 3) r = c * m + float4(1, 1, 1, 1) * (1 - c.a * m);
        else r = c * m;
        gOut[p] = r;
    }
}
