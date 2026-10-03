//@name Grass
//@category CyTexture Détails
//@selfflow
//@spacing 0.45
//@param Spread float 0.85 0.05 1.5 "Spread"
//@param MinHeight float 0.28 0.02 1 "Min Height"
//@param MaxHeight float 0.86 0.02 1.5 "Max Height"
//@param Lean float 0.28 0 1 "Lean"
//@param BladeWidth float 0.018 0.001 0.15 "Blade Width"
//@param Softness float 0.006 0.0001 0.1 "Softness"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Surface Detail): Clustered curved grass blades with seeded height, lean, and width variation.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Spread, float MinHeight, float MaxHeight, float Lean, float BladeWidth, float Softness, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-.5;float field=0;[unroll]for(int i=0;i<13;i++){float h=frac(sin((i+Seed)*19.71)*43758.5453);float x=(i/12.0-.5)*Spread;float height=lerp(MinHeight,MaxHeight,h);float y0=.48;float t=saturate((y0-p.y)/max(height,.001));float center=x+(h-.5)*Lean*t*t;float blade=(1.0-smoothstep(BladeWidth*(1.0-t),BladeWidth*(1.0-t)+Softness,abs(p.x-center)))*step(p.y,y0)*step(y0-height,p.y);field=max(field,blade);}float a=field*CyBrushFlow*saturate(CyBrushPosition.z);return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Spread, MinHeight, MaxHeight, Lean, BladeWidth, Softness, Seed + floor(b.seed * 997.0) * RandomSeed);
}
