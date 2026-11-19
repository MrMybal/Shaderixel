// One flag per 64 x 64 UV tile, shared by every channel of a mesh stroke.
cbuffer Root : register(b0) { uint2 origin, limit; };
Texture2D<float> coverage : register(t0);
Texture2D<float> impacts : register(t1);
RWTexture2D<float> tiles : register(u0);
groupshared uint occupied;

[numthreads(8, 8, 1)]
void main(uint3 group : SV_GroupID, uint3 lane : SV_GroupThreadID) {
    if (!any(lane.xy)) occupied = 0;
    GroupMemoryBarrierWithGroupSync();
    uint2 start = origin + group.xy * 64 + lane.xy;
    bool found = false;
    for (uint y = 0; y < 64 && !found; y += 8)
        for (uint x = 0; x < 64 && !found; x += 8) {
            uint2 p = start + uint2(x, y);
            if (all(p < limit)) found = max(coverage.Load(int3(p, 0)), impacts.Load(int3(p, 0))) > 0;
        }
    if (found) InterlockedOr(occupied, 1);
    GroupMemoryBarrierWithGroupSync();
    if (!any(lane.xy)) tiles[group.xy] = occupied != 0 ? 1.0 : 0.0;
}
