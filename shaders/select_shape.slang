// New selection = combine(previous selection, shape coverage).
// Shapes: 0 rect, 1 ellipse, 2 coverage texture (t1 placed at gTexOffset), 3 everything, 4 nothing.
// Modes: 0 replace, 1 add, 2 subtract, 3 intersect.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gShape;
    uint gMode;
    float4 gBounds;     // x0, y0, x1, y1 (document pixels, float)
    int2 gTexOffset;
    uint gFlags;        // 1 = has previous selection, 2 = antialias
};

Texture2D<float> gPrev : register(t0);
Texture2D<float> gCoverage : register(t1);
RWTexture2D<float> gOut : register(u0);

float Overlap(float a0, float a1, float b0, float b1) { return saturate(min(a1, b1) - max(a0, b0)); }

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float2 p = float2(id.xy);
    bool aa = (gFlags & 2) != 0;
    float c = 0;
    if (gShape == 0)
    {
        c = Overlap(p.x, p.x + 1, gBounds.x, gBounds.z) * Overlap(p.y, p.y + 1, gBounds.y, gBounds.w);
        if (!aa) c = step(0.5, c);
    }
    else if (gShape == 1)
    {
        float2 center = (gBounds.xy + gBounds.zw) * 0.5;
        float2 r = max(abs(gBounds.zw - gBounds.xy) * 0.5, 0.5);
        float2 q = (p + 0.5 - center);
        float f = dot(q / r, q / r) - 1.0;
        float2 grad = 2.0 * q / (r * r);
        float d = f / max(length(grad), 1e-4);
        c = aa ? saturate(0.5 - d) : step(d, 0.0);
    }
    else if (gShape == 2)
    {
        int2 t = int2(id.xy) - gTexOffset;
        uint w, h;
        gCoverage.GetDimensions(w, h);
        c = (all(t >= 0) && all(t < int2(w, h))) ? gCoverage.Load(int3(t, 0)) : 0.0;
    }
    else if (gShape == 3) c = 1.0;

    float prev = (gFlags & 1) ? gPrev.Load(int3(id.xy, 0)) : 0.0;
    float r;
    if (gMode == 1) r = max(prev, c);
    else if (gMode == 2) r = prev * (1.0 - c);
    else if (gMode == 3) r = min(prev, c);
    else r = c;
    gOut[id.xy] = r;
}
