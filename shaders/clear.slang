// Fill a rectangle of an RGBA texture with a constant (premultiplied) value.
cbuffer Root : register(b0)
{
    int2 gOrigin;
    uint2 gSize;
    float4 gValue;
};

RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    gOut[gOrigin + int2(id.xy)] = gValue;
}
