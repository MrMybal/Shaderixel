//@name Chalk
//@category CyTexture Matières
//@selfflow
//@spacing 0.12
//@param NoiseScale float 48 2 512 "Noise Scale"
//@param Grain float 0.8 0 1 "Grain"
//@param GrainThreshold float 0.25 0 1 "Grain Threshold"
//@param EdgeBreakup float 0.35 0 1 "Edge Breakup"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Natural Media): Grainy chalk tip with noisy coverage and irregular edges.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float NoiseScale, float Grain, float GrainThreshold, float EdgeBreakup, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float2 cell=floor(UV*max(NoiseScale,1.0)+Seed); float n=frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453); float edge=length(p)*2.0*(1.0+EdgeBreakup*(n-0.5)); float shape=1.0-smoothstep(saturate(CyBrushHardness),1.0,edge); float coverage=lerp(1.0,smoothstep(GrainThreshold,1.0,n),saturate(Grain)); float a=shape*coverage*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), NoiseScale, Grain, GrainThreshold, EdgeBreakup, Seed + floor(b.seed * 997.0) * RandomSeed);
}
