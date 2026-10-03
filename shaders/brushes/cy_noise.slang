//@name Noise
//@category CyTexture Formes
//@selfflow
//@spacing 0.1
//@param NoiseScale float 8 1 128 "Noise Scale"
//@param NoiseThreshold float 0.35 0 1 "Noise Threshold"

// Imported from CyTexture (Brush / Shape Tips): White local-space round material brush whose coverage is broken up by procedural noise.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float NoiseScale, float NoiseThreshold)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 d=UV-0.5; float q=length(d)/0.5; float shape=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); float n=frac(sin(dot(floor(UV*max(NoiseScale,1.0)),float2(12.9898,78.233)))*43758.5453); float coverage=saturate((n-NoiseThreshold)*8.0); float a=shape*coverage*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), NoiseScale, NoiseThreshold);
}
