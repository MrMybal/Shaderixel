// Write a selection (R8) into an RGBA layer mask.
cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gInvert;
};

Texture2D<float> gSel : register(t0);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float s = gSel.Load(int3(id.xy, 0));
    if (gInvert) s = 1.0 - s;
    gOut[id.xy] = float4(s, s, s, 1.0);
}
