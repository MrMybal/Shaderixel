// Selection filters on R8 masks. Ops: 0 gaussian pass, 1 dilate pass, 2 erode pass, 3 invert, 4 copy.
// Separable ops run twice (gPass 0 = horizontal, 1 = vertical).
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gOp;
    uint gPass;
    float gRadius;
};

Texture2D<float> gIn : register(t0);
RWTexture2D<float> gOut : register(u0);

float L(int2 p) { return gIn.Load(int3(clamp(p, int2(0, 0), int2(gSize) - 1), 0)); }

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    int2 dir = gPass == 0 ? int2(1, 0) : int2(0, 1);
    float r;
    if (gOp == 0)
    {
        float sigma = max(gRadius * 0.5, 0.01);
        int n = int(ceil(sigma * 3.0));
        int stride = max(1, n / 128);
        float s = 0, w = 0;
        for (int i = -n; i <= n; i += stride)
        {
            float k = exp(-0.5 * (i * i) / (sigma * sigma));
            s += L(p + dir * i) * k;
            w += k;
        }
        r = s / w;
    }
    else if (gOp == 1 || gOp == 2)
    {
        int n = int(round(gRadius));
        r = gOp == 1 ? 0.0 : 1.0;
        for (int i = -n; i <= n; ++i)
        {
            float v = L(p + dir * i);
            r = gOp == 1 ? max(r, v) : min(r, v);
        }
    }
    else if (gOp == 3) r = 1.0 - L(p);
    else r = L(p);
    gOut[id.xy] = r;
}
