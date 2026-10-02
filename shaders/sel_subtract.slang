// out = saturate(a - b) on masks (selection ring for Edit > Stroke).
cbuffer Root : register(b0)
{
    uint2 gSize;
};

Texture2D<float> gA : register(t0);
Texture2D<float> gB : register(t1);
RWTexture2D<float> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int3 p = int3(id.xy, 0);
    gOut[id.xy] = saturate(gA.Load(p) - gB.Load(p));
}
