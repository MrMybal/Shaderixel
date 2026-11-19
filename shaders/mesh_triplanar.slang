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

float3 UvDerivative(int2 px, int2 axis, float3 pos) {
    int2 last = int2(gSize) - 1;
    float4 a = gPos.Load(int3(clamp(px-axis, int2(0), last), 0));
    float4 b = gPos.Load(int3(clamp(px+axis, int2(0), last), 0));
    float3 back = pos-a.xyz, forward = b.xyz-pos;
    float da = dot(back,back), db = dot(forward,forward);
    bool va = a.a>0 && da>1e-14, vb = b.a>0 && db>1e-14;
    if (va && vb && max(da,db)<4*min(da,db)) return (forward+back)*0.5;
    return va && (!vb || da<db) ? back : vb ? forward : float3(0,0,0);
}

float3 AxisSlope(float4 tap, float3 axisU, float3 axisV) {
    float3 tn = LinearToSrgb(saturate(tap.rgb))*2-1;
    float2 slope = tn.xy/max(0.05,tn.z);
    float c = cos(gRotation), s = sin(gRotation);
    return axisU*(c*slope.x+s*slope.y) + axisV*(-s*slope.x+c*slope.y);
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
    if (gMode == 2) {
        float3 slope = w.x*AxisSlope(cx, float3(0,0,n.x<0?-1:1), float3(0,-1,0)) +
                       w.y*AxisSlope(cy, float3(1,0,0), float3(0,0,n.y<0?-1:1)) +
                       w.z*AxisSlope(cz, float3(n.z<0?1:-1,0,0), float3(0,-1,0));
        float3 du = UvDerivative(px,int2(1,0),pos.xyz), dv = UvDerivative(px,int2(0,1),pos.xyz);
        float3 t = du - n*dot(du,n);
        float3 encoded = float3(0.5,0.5,1);
        if (dot(t,t)>1e-14 && dot(dv,dv)>1e-14) {
            t = normalize(t);
            float3 b = cross(n,t);
            if (dot(b,dv)<0) b = -b;
            encoded = normalize(float3(dot(slope,t),dot(slope,b),1))*0.5+0.5;
        }
        gOut[px] = float4(SrgbToLinear(encoded),1);
        return;
    }
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
