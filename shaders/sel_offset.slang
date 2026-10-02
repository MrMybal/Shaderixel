// Translate a selection mask by an integer offset (outline move).
cbuffer Root : register(b0)
{
    uint2 gSize;
    int2 gOffset;
};

Texture2D<float> gSrc : register(t0);
RWTexture2D<float> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 q = int2(id.xy) - gOffset;
    gOut[id.xy] = all(q >= 0) && all(q < int2(gSize)) ? gSrc.Load(int3(q, 0)) : 0.0;
}
