//@name Smoke
//@category CyTexture Particules
//@selfflow
//@spacing 0.3
//@param Scale float 3.5 0.1 64 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Bias float 0.42 0 1 "Bias"
//@param Contrast float 1.8 0 8 "Contrast"
//@param Falloff float 1.4 0.05 8 "Falloff"
//@param Turbulence float 0.85 0 1 "Turbulence"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Particles): Layered turbulent smoke stamp with wispy breakup and soft radial extinction.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Scale, float Seed, float Bias, float Contrast, float Falloff, float Turbulence)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float2 q=p*max(Scale,0.1)+Seed; float n=0.0;float amp=.55;[unroll]for(int i=0;i<4;i++){float v=sin(q.x*6.1+sin(q.y*4.3))+sin(q.y*7.7-q.x*2.1);n+=v*amp;q=q*1.93+7.13;amp*=.5;} n=saturate((n*.25+0.5-Bias)*Contrast); float radial=pow(saturate(1.0-length(p)*2.0),Falloff); float wisps=lerp(1.0,n,saturate(Turbulence)); float a=radial*wisps*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Scale, Seed + floor(b.seed * 997.0) * RandomSeed, Bias, Contrast, Falloff, Turbulence);
}
