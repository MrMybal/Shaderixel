// A view-space address for material tools. Read with point sampling so adjacent UV islands never blend.
cbuffer View : register(b1) { row_major float4x4 gViewProj; };
struct Vertex { float3 pos : POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; };
struct Interpolated { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
Interpolated vsMain(Vertex v) {
    Interpolated o;
    o.pos = mul(gViewProj, float4(v.pos, 1));
    o.uv = v.uv;
    return o;
}
float4 psMain(Interpolated p) : SV_Target0 { return float4(p.uv, p.pos.z, 1); }
