// Fill a single-channel float texture with a constant.
cbuffer Root : register(b0)
{
    uint2 gSize;
    float gValue;
};

RWTexture2D<float> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    gOut[id.xy] = gValue;
}
