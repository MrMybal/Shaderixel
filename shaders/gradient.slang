// Gradient tool: draws a gradient from A to B over the layer (source-over),
// both colors linear straight. Shape: 0 linear, 1 radial, 2 angle, 3 reflected, 4 diamond.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    float2 gStart;
    float2 gEnd;
    uint gShape;
    float gOpacity;
    float4 gColorA;
    float4 gColorB;
};

Texture2D<float4> gPre : register(t0);
Texture2D<float> gSel : register(t1);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float2 p = float2(id.xy) + 0.5;
    float2 axis = gEnd - gStart;
    float len2 = max(dot(axis, axis), 1e-4);
    uint shape = gShape & 0xFF;
    float t;
    if (shape == 1) t = length(p - gStart) / sqrt(len2);
    else if (shape == 2) t = frac((atan2(p.y - gStart.y, p.x - gStart.x) - atan2(axis.y, axis.x)) / 6.2831853 + 1.0);
    else if (shape == 3) t = abs(dot(p - gStart, axis) / len2);
    else if (shape == 4)
    {
        float2 n = normalize(axis);
        float2 d = p - gStart;
        t = (abs(dot(d, n)) + abs(dot(d, float2(-n.y, n.x)))) / sqrt(len2);
    }
    else t = dot(p - gStart, axis) / len2;
    t = saturate(t);
    float4 c = lerp(Premultiply(gColorA), Premultiply(gColorB), t);
    // Small dither breaks banding in 8-bit exports.
    c.rgb += (Hash12(p) - 0.5) / 255.0 * c.a;
    c *= gOpacity;
    if (gShape & 0x100) c *= gSel.Load(int3(id.xy, 0));
    float4 pre = gPre.Load(int3(id.xy, 0));
    gOut[id.xy] = c + pre * (1.0 - c.a);
}
