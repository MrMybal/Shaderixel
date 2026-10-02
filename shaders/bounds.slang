// Bounding box of visible pixels (alpha * selection > threshold) via atomics.
cbuffer Root : register(b0)
{
    uint2 gSize;
    uint gFlags;    // 1 = has selection, 2 = ignore alpha (mask), 4 = use red channel (selection)
};

Texture2D<float4> gImage : register(t0);
Texture2D<float> gSel : register(t1);
RWTexture2D<uint> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    bool inside = all(id.xy < gSize);
    float v = 0;
    if (inside)
    {
        float4 c = gImage.Load(int3(id.xy, 0));
        v = (gFlags & 4) ? c.r : (gFlags & 2) ? 1.0 : c.a;
        if (gFlags & 1) v *= gSel.Load(int3(id.xy, 0));
    }
    bool hit = inside && v > (1.0 / 512.0);
    uint x0 = WaveActiveMin(hit ? id.x : 0xFFFFFFFFu);
    uint y0 = WaveActiveMin(hit ? id.y : 0xFFFFFFFFu);
    uint x1 = WaveActiveMax(hit ? id.x + 1 : 0u);
    uint y1 = WaveActiveMax(hit ? id.y + 1 : 0u);
    bool anyHit = WaveActiveAnyTrue(hit);
    if (anyHit && WaveIsFirstLane())
    {
        InterlockedMin(gOut[int2(0, 0)], x0);
        InterlockedMin(gOut[int2(1, 0)], y0);
        InterlockedMax(gOut[int2(2, 0)], x1);
        InterlockedMax(gOut[int2(3, 0)], y1);
    }
}
