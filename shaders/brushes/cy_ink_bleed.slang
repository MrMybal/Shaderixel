//@name Ink Bleed
//@category Matières
//@selfflow
//@spacing 0.12
//@param Lobes float 13 1 64 "Lobes"
//@param Irregularity float 0.12 0 0.5 "Irregularity"
//@param BleedWidth float 0.22 0.001 1 "Bleed Width"
//@param BleedFalloff float 3 0.1 16 "Bleed Falloff"
//@param BleedAmount float 0.45 0 2 "Bleed Amount"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported preset (Brush / Natural Media): Dense ink stamp with irregular capillary bleed and a configurable wet fringe.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Lobes, float Irregularity, float BleedWidth, float BleedFalloff, float BleedAmount, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float a0=atan2(p.y,p.x); float n=sin(a0*Lobes+Seed)*0.5+sin(a0*Lobes*2.17-Seed*1.3)*0.25; float radius=0.78+Irregularity*n; float d=length(p)*2.0; float core=1.0-smoothstep(radius*CyBrushHardness,radius,d); float bleed=pow(saturate(1.0-(d-radius)/max(BleedWidth,0.0001)),max(BleedFalloff,0.1))*step(radius,d); float alpha=saturate(core+bleed*BleedAmount)*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,alpha);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Lobes, Irregularity, BleedWidth, BleedFalloff, BleedAmount, Seed + floor(b.seed * 997.0) * RandomSeed);
}
