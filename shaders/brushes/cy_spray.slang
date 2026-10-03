//@name Spray
//@category CyTexture Particules
//@selfflow
//@spacing 0.3
//@param Density float 0.22 0.001 1 "Density"
//@param GrainScale float 96 4 512 "Grain Scale"
//@param Falloff float 1.5 0.05 8 "Falloff"
//@param Softness float 0.04 0.001 0.5 "Softness"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Particles): Pressure-aware spray paint with density, grain size, falloff, and seed controls.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Density, float GrainScale, float Falloff, float Softness, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float radial=pow(saturate(1.0-length(p)*2.0),max(Falloff,0.05)); float2 cell=floor(UV*max(GrainScale,1.0)+Seed); float n=frac(sin(dot(cell,float2(12.9898,78.233)))*43758.5453); float speck=smoothstep(1.0-saturate(Density),min(1.0,1.0-saturate(Density)+Softness),n); float a=radial*speck*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Density, GrainScale, Falloff, Softness, Seed + floor(b.seed * 997.0) * RandomSeed);
}
