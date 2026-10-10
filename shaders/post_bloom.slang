// Post-process bloom: mode 0 = downsample (13 taps, the first level keeps only
// what is above the threshold), mode 1 = upsample (tent) added to the larger level.
#include "common.slang"

cbuffer Root : register(b0)
{
    uint2 gSize;       // destination size
    uint gMode;        // 0 down, 1 up
    uint gFirst;       // down: first level (threshold)
    float gThreshold;
    float gKnee;
    float gRadius;     // up: blend of the smaller level
    float _pad;
};

Texture2D<float4> gSrc : register(t0);
Texture2D<float4> gDst : register(t1);  // up: the larger level before the add
RWTexture2D<float4> gOut : register(u0);

float3 Tap(float2 uv) { return gSrc.SampleLevel(sLinearClamp, uv, 0).rgb; }

float3 Threshold(float3 c)
{
    float b = max(c.r, max(c.g, c.b));
    float soft = clamp(b - gThreshold + gKnee, 0.0, 2.0 * gKnee);
    soft = soft * soft / (4.0 * gKnee + 1e-5);
    float w = max(soft, b - gThreshold) / max(b, 1e-5);
    return c * saturate(w);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float2 uv = (float2(id.xy) + 0.5) / float2(gSize);
    uint sw, sh;
    gSrc.GetDimensions(sw, sh);
    float2 t = 1.0 / float2(sw, sh);
    if (gMode == 0)
    {
        // Call of Duty: Advanced Warfare downsample.
        float3 a = Tap(uv + t * float2(-2, -2)), b = Tap(uv + t * float2(0, -2)), c = Tap(uv + t * float2(2, -2));
        float3 d = Tap(uv + t * float2(-2, 0)), e = Tap(uv), f = Tap(uv + t * float2(2, 0));
        float3 g = Tap(uv + t * float2(-2, 2)), h = Tap(uv + t * float2(0, 2)), i = Tap(uv + t * float2(2, 2));
        float3 j = Tap(uv + t * float2(-1, -1)), k = Tap(uv + t * float2(1, -1)), l = Tap(uv + t * float2(-1, 1)), m = Tap(uv + t * float2(1, 1));
        float3 o = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
        if (gFirst) o = Threshold(o);
        gOut[id.xy] = float4(min(o, 65000.0), 1.0);
    }
    else
    {
        float3 o = (Tap(uv + t * float2(-1, -1)) + Tap(uv + t * float2(1, -1)) + Tap(uv + t * float2(-1, 1)) + Tap(uv + t * float2(1, 1))) * 0.0625
                 + (Tap(uv + t * float2(0, -1)) + Tap(uv + t * float2(-1, 0)) + Tap(uv + t * float2(1, 0)) + Tap(uv + t * float2(0, 1))) * 0.125
                 + Tap(uv) * 0.25;
        float3 base = gDst.Load(int3(id.xy, 0)).rgb;
        gOut[id.xy] = float4(base + o * gRadius, 1.0);
    }
}
