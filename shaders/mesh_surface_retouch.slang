#include "common.slang"
cbuffer Root : register(b0) {
    uint2 gSize, gViewSize;
    float2 gFrom, gTo;
    float2 gSourceOffset;
    float gRadius, gHard;
    float gStrength, gNear, gFar;
    uint gFlags;
};
cbuffer View : register(b1) { row_major float4x4 gVP; uint4 gRect; };
Texture2D<float4> gPosition : register(t0);
Texture2D<float4> gView : register(t1);
Texture2D<float4> gSource : register(t2);
Texture2D<float> gSelection : register(t3);
RWTexture2D<float4> gTarget : register(u0);
RWTexture2D<float> gMask : register(u1);

float ViewDepth(float z) {
    return gNear > 0 ? gNear * gFar / max(gFar - z * (gFar - gNear), 1e-6) : lerp(gNear, gFar, z);
}
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    id.xy += gRect.xy;
    if (any(id.xy >= gRect.zw)) return;
    const float4 wp = gPosition.Load(int3(id.xy, 0));
    if (wp.a <= 0) return;
    const float4 clip = mul(gVP, float4(wp.xyz, 1));
    if (clip.w <= 1e-6) return;
    const float3 ndc = clip.xyz / clip.w;
    const float2 sp = (ndc.xy * float2(0.5, -0.5) + 0.5) * gViewSize;
    if (any(sp < 0) || any(sp >= float2(gViewSize))) return;
    const float4 front = gView.Load(int3(int2(sp),0));
    const float depth = ViewDepth(ndc.z);
    if (front.w == 0 || depth - ViewDepth(front.z) > (gNear > 0 ? depth : 1) * 0.002 + 0.0001) return;
    float2 segment = gTo - gFrom;
    float t = saturate(dot(sp-gFrom, segment) / max(dot(segment, segment), 0.0001));
    float dist = length(sp - lerp(gFrom,gTo,t));
    float k = gStrength * (1.0 - smoothstep(gHard*gRadius, max(gRadius, gHard*gRadius+0.5), dist));
    if (k <= 0) return;
    if ((gFlags & 3u) != 0) {
        gMask[id.xy] = lerp(gMask[id.xy], (gFlags & 3u) == 2 ? 1.0 : 0.0, k);
        return;
    }
    const float2 sourcePos = sp + gSourceOffset;
    if (any(sourcePos < 0) || any(sourcePos >= float2(gViewSize))) return;
    const float4 source = gView.Load(int3(int2(sourcePos),0));
    if (source.w <= 0) return;
    if ((gFlags & 4u) != 0) k *= gSelection.Load(int3(id.xy,0));
    if (k <= 0) return;
    gTarget[id.xy] = lerp(gTarget[id.xy], gSource.SampleLevel(sLinearClamp, source.xy, 0), k);
    gMask[id.xy] = 1.0;
}
