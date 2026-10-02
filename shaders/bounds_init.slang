// Reset the 4-texel bounds accumulator.
RWTexture2D<uint> gOut : register(u0);

[numthreads(4, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    gOut[int2(id.x, 0)] = id.x < 2 ? 0xFFFFFFFFu : 0u;
}
