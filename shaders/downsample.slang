// Box downsample by an integer factor (display reduction for strong zoom-out).
// Premultiplied data averages correctly.
cbuffer Root : register(b0)
{
    uint2 gSrcSize;
    int2 gOrigin;      // destination pixel of thread (0,0)
    uint2 gSize;       // destination region size
    uint gFactor;
};

Texture2D<float4> gSrc : register(t0);
RWTexture2D<float4> gOut : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = gOrigin + int2(id.xy);
    int2 s0 = p * int(gFactor);
    int2 s1 = min(s0 + int(gFactor), int2(gSrcSize));
    float4 sum = 0;
    for (int y = s0.y; y < s1.y; ++y)
        for (int x = s0.x; x < s1.x; ++x)
            sum += gSrc.Load(int3(x, y, 0));
    int2 n = max(s1 - s0, int2(1, 1));
    gOut[p] = sum / float(n.x * n.y);
}
