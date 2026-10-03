// Small copy of an image for effect previews: box average over the source
// footprint of each output pixel (4x4 bilinear taps), aspect kept, centered.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;      // output size
    float2 gScale;    // source pixels per output pixel
    float2 gOffset;   // source position of output pixel (0,0)
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    uint sw, sh;
    gSrc.GetDimensions(sw, sh);
    float4 sum = 0;
    [unroll] for (int y = 0; y < 4; ++y)
        [unroll] for (int x = 0; x < 4; ++x)
        {
            float2 p = gOffset + (float2(id.xy) + (float2(x, y) + 0.5) / 4.0) * gScale;
            float inside = all(p >= 0) && all(p < float2(sw, sh)) ? 1.0 : 0.0;
            sum += gSrc.SampleLevel(sLinearClamp, p / float2(sw, sh), 0) * inside;
        }
    gOut[id.xy] = sum / 16.0;
}
