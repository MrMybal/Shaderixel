//@name Cloud
//@category CyTexture Particules
//@selfflow
//@spacing 0.3
//@param CloudScale float 3.5 0.1 64 "Cloud Scale"
//@param Falloff float 1.3 0.05 8 "Falloff"
//@param Contrast float 1.6 0 8 "Contrast"
//@param Modulation float 0.75 0 1 "Modulation"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Particles): Soft cloudy brush with procedural multi-frequency modulation.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float CloudScale, float Falloff, float Contrast, float Modulation, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float shape=pow(saturate(1.0-length(p)*2.0),max(Falloff,0.05)); float2 q=UV*max(CloudScale,0.1)+Seed; float n=sin(q.x*6.283+sin(q.y*3.1))*0.25+sin(q.y*9.17-q.x*2.3)*0.18+sin((q.x+q.y)*17.13)*0.07+0.5; n=saturate((n-0.5)*Contrast+0.5); float a=shape*lerp(1.0,n,saturate(Modulation))*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), CloudScale, Falloff, Contrast, Modulation, Seed + floor(b.seed * 997.0) * RandomSeed);
}
