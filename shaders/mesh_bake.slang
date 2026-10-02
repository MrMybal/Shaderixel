// Rasterize the mesh in UV space: world position and normal per texel.
struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};
struct VSOut
{
    float4 clip : SV_Position;
    float3 world : TEXCOORD0;
    float3 nrm : TEXCOORD1;
};
struct PSOut
{
    float4 pos : SV_Target0;
    float4 nrm : SV_Target1;
};

VSOut vsMain(VSIn i)
{
    VSOut o;
    float2 uv = i.uv;
    o.clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.5, 1.0);
    o.world = i.pos;
    o.nrm = i.nrm;
    return o;
}

PSOut psMain(VSOut i)
{
    PSOut o;
    o.pos = float4(i.world, 1.0);
    o.nrm = float4(normalize(i.nrm), 1.0);
    return o;
}
