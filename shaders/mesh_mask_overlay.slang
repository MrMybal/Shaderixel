#include "common.slang"
cbuffer Root : register(b0) { uint2 gSize; };
Texture2D<float4> gImage : register(t0);
Texture2D<float4> gView : register(t1);
Texture2D<float> gSelection : register(t2);
RWTexture2D<float4> gOutput : register(u0);
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy >= gSize)) return;
    float4 c = gImage.Load(int3(id.xy,0));
    const float4 uv = gView.Load(int3(id.xy,0));
    if (uv.w > 0) c.rgb = lerp(c.rgb, float3(1,0.12,0.08), (1-gSelection.SampleLevel(sLinearClamp,uv.xy,0))*0.55);
    gOutput[id.xy] = c;
}
