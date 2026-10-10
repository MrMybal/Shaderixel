// Projected material (Paint 3D): an image projected on the model along the three
// axes (triplanar, object space: no UV seam), written into a channel layer.
// Mode 0: color (linear, from the sRGB image); 1: a gray value from the image's
// luminance mapped to [lo, hi] (roughness, height, metal), stored as a visual gray.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gMode;
    float gScale;       // repeats per model unit
    float gRotation;    // radians, around the projection axis
    float gSharpness;   // blend between the 3 projections (1 soft .. 16 hard)
    float gLo, gHi;     // mode 1 output range
    float2 gOffset;
    float gContrast;    // mode 1: contrast of the luminance
    uint gInvert;       // mode 1: 1 - luminance
};

Texture2D<float4> gPos : register(t0);
Texture2D<float4> gNrm : register(t1);
Texture2D<float4> gImage : register(t2);
RWTexture2D<float4> gOut : register(u0);

float4 Tap(float2 uv)
{
    float c = cos(gRotation), s = sin(gRotation);
    uv = float2(c * uv.x - s * uv.y, s * uv.x + c * uv.y) * gScale + gOffset;
    float4 t = gImage.SampleLevel(sLinearWrap, uv, 0);
    return float4(SrgbToLinear(t.rgb), t.a);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 px = int2(id.xy);
    float4 pos = gPos.Load(int3(px, 0));
    if (pos.a <= 0.0) { gOut[px] = float4(0, 0, 0, 0); return; }
    float3 n = normalize(gNrm.Load(int3(px, 0)).xyz);
    float3 w = pow(abs(n), gSharpness);
    w /= max(w.x + w.y + w.z, 1e-5);
    // Each axis projection, mirrored on the back faces so the image reads the same way.
    float4 cx = Tap(float2(pos.z * (n.x < 0 ? -1.0 : 1.0), -pos.y));
    float4 cy = Tap(float2(pos.x, pos.z * (n.y < 0 ? -1.0 : 1.0)));
    float4 cz = Tap(float2(pos.x * (n.z < 0 ? 1.0 : -1.0), -pos.y));
    float4 c = cx * w.x + cy * w.y + cz * w.z;
    if (gMode == 0)
    {
        gOut[px] = float4(c.rgb, 1.0);
        return;
    }
    float l = dot(c.rgb, float3(0.2126, 0.7152, 0.0722));
    l = LinearToSrgb(float3(l, l, l)).r;  // perceptual
    l = saturate((l - 0.5) * gContrast + 0.5);
    if (gInvert) l = 1.0 - l;
    float v = lerp(gLo, gHi, l);
    float g = SrgbToLinear(float3(v, v, v)).r;  // data channels hold visual gray levels
    gOut[px] = float4(g, g, g, 1.0);
}
