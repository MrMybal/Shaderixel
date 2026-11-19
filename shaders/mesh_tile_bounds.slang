// World-space bounds of each 64 x 64 UV tile, including dilated seam texels.
Texture2D<float4> positions : register(t0);
RWTexture2D<float4> bounds : register(u0);
groupshared float3 minima[64];
groupshared float3 maxima[64];

[numthreads(8,8,1)]
void main(uint3 group : SV_GroupID, uint3 lane : SV_GroupThreadID, uint index : SV_GroupIndex) {
    uint w, h;
    positions.GetDimensions(w,h);
    float3 lo = float3(1e30,1e30,1e30), hi = -lo;
    for (uint y=0; y<64; y+=8) for (uint x=0; x<64; x+=8) {
        uint2 at = group.xy*64 + lane.xy + uint2(x,y);
        if (all(at < uint2(w,h))) {
            float4 p = positions.Load(int3(at,0));
            if (p.a>0) { lo=min(lo,p.xyz); hi=max(hi,p.xyz); }
        }
    }
    minima[index]=lo; maxima[index]=hi;
    GroupMemoryBarrierWithGroupSync();
    for (uint step=32; step>0; step>>=1) {
        if (index<step) {
            minima[index]=min(minima[index],minima[index+step]);
            maxima[index]=max(maxima[index],maxima[index+step]);
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (index==0) {
        uint2 at=uint2(group.x*2,group.y);
        bounds[at]=float4(minima[0],minima[0].x<=maxima[0].x ? 1.0 : 0.0);
        bounds[at+uint2(1,0)]=float4(maxima[0],0);
    }
}
