//@name Sponge
//@category CyTexture Matières
//@selfflow
//@spacing 0.12
//@param CellScale float 10 2 64 "Cell Scale"
//@param PoreSize float 0.16 0.01 0.7 "Pore Size"
//@param Softness float 0.05 0.001 0.4 "Softness"
//@param Porosity float 0.9 0 1 "Porosity"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Natural Media): Porous sponge texture with adjustable cells and holes.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float CellScale, float PoreSize, float Softness, float Porosity, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float shape=1.0-smoothstep(saturate(CyBrushHardness),1.0,length(p)*2.0); float2 g=UV*max(CellScale,1.0); float2 id=floor(g)+Seed; float2 f=frac(g)-0.5; float h=frac(sin(dot(id,float2(127.1,311.7)))*43758.5453); float pore=1.0-smoothstep(PoreSize,PoreSize+Softness,length(f-float2(h-0.5,frac(h*17.17)-0.5)*0.45)); float coverage=1.0-pore*saturate(Porosity); float a=shape*coverage*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), CellScale, PoreSize, Softness, Porosity, Seed + floor(b.seed * 997.0) * RandomSeed);
}
