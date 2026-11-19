// Sample one emitter point per coarse UV cell, using the temporary selection in document space.
cbuffer Root : register(b0) { uint2 size; uint step; };
Texture2D<float4> positions : register(t0);
Texture2D<float4> normals : register(t1);
Texture2D<float> selection : register(t2);
RWTexture2D<float4> sources : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    const uint2 origin = tid.xy * step;
    if (any(origin >= size)) return;
    uint2 output = uint2(tid.x * 2, tid.y);
    sources[output] = 0;
    sources[output + uint2(1,0)] = 0;
    const uint2 end = min(origin + step, size);
    for (uint y = origin.y; y < end.y; ++y) {
        for (uint x = origin.x; x < end.x; ++x) {
            int3 p = int3(x, y, 0);
            const float4 position = positions.Load(p);
            if (position.a < 0.75 || selection.Load(p) <= 0.5) continue;
            sources[output] = float4(position.xyz, 1);
            sources[output + uint2(1,0)] = normals.Load(p);
            return;
        }
    }
}
