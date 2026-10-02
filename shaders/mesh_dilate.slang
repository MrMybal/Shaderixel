// Grow the baked UV islands by one texel (positions/normals copied from a valid
// neighbor) so painting covers the seam padding.
cbuffer Root : register(b0)
{
    uint2 gSize;
};

Texture2D<float4> gPosIn : register(t0);
Texture2D<float4> gNrmIn : register(t1);
RWTexture2D<float4> gPosOut : register(u0);
RWTexture2D<float4> gNrmOut : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    int2 p = int2(id.xy);
    float4 pos = gPosIn.Load(int3(p, 0));
    float4 nrm = gNrmIn.Load(int3(p, 0));
    if (pos.a <= 0.0)
    {
        const int2 offs[8] = {int2(1, 0), int2(-1, 0), int2(0, 1), int2(0, -1), int2(1, 1), int2(-1, 1), int2(1, -1), int2(-1, -1)};
        for (int i = 0; i < 8; ++i)
        {
            int2 q = clamp(p + offs[i], int2(0, 0), int2(gSize) - 1);
            float4 c = gPosIn.Load(int3(q, 0));
            if (c.a > 0.0)
            {
                pos = float4(c.xyz, 0.5);   // 0.5 marks a dilated (padding) texel
                nrm = gNrmIn.Load(int3(q, 0));
                break;
            }
        }
    }
    gPosOut[p] = pos;
    gNrmOut[p] = nrm;
}
