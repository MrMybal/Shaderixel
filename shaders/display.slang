// Viewport/thumbnail presentation: zoom/pan, checkerboard, linear -> sRGB.
#include "common.slang"

cbuffer View : register(b1)
{
    float2 gOrigin;     // document pixel at view pixel (0,0)
    float gInvZoom;     // document pixels per view pixel
    uint gFlags;        // 1 = has image, 2 = nearest when magnified
    uint2 gViewSize;
    uint2 gDocSize;
    float4 gOutside;    // sRGB color outside the document
    float gChecker;     // checker square size in view pixels
    float gExposure;    // stops
    float2 _pad;
};

Texture2D<float4> gImage : register(t0);
RWTexture2D<float4> gOut : register(u0);

float4 Fetch(float2 docPos)
{
    float2 uv = docPos / float2(gDocSize);
    if (gInvZoom <= 0.5 && (gFlags & 2))
        return gImage.Load(int3(int2(floor(docPos)), 0));
    return gImage.SampleLevel(sLinearClamp, uv, 0);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gViewSize)) return;
    float2 v = float2(id.xy) + 0.5;
    float2 d = gOrigin + v * gInvZoom;
    if (any(d < 0.0) || any(d >= float2(gDocSize)))
    {
        gOut[id.xy] = gOutside;
        return;
    }

    float4 c = float4(0, 0, 0, 0);
    if (gFlags & 1)
    {
        // Minification: box-filter n x n taps over the footprint of the view pixel.
        int n = clamp(int(ceil(gInvZoom)), 1, 4);
        if (n == 1)
            c = Fetch(d);
        else
        {
            float step = gInvZoom / n;
            float2 start = gOrigin + (v - 0.5) * gInvZoom + step * 0.5;
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                    c += gImage.SampleLevel(sLinearClamp, (start + float2(x, y) * step) / float2(gDocSize), 0);
            c /= float(n * n);
        }
    }
    c.rgb *= exp2(gExposure);

    int2 cell = int2(floor(v / max(gChecker, 1.0)));
    float checker = ((cell.x + cell.y) & 1) ? 0.80 : 1.0;
    float3 back = SrgbToLinear(float3(checker, checker, checker));
    float3 rgb = c.rgb + back * (1.0 - saturate(c.a));
    gOut[id.xy] = float4(LinearToSrgb(rgb), 1.0);
}
