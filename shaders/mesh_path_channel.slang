cbuffer Root : register(b0) { uint2 gSize; uint2 _pad; float4 gColor; };
Texture2D<float> gCoverage : register(t0);
RWTexture2D<float4> gOutput : register(u0);
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if (any(id.xy>=gSize)) return;
    const float a=gColor.a*gCoverage.Load(int3(id.xy,0));
    gOutput[id.xy]=float4(gColor.rgb*a,a);
}
