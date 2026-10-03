//@name Crack
//@category CyTexture Détails
//@selfflow
//@spacing 0.45
//@param Width float 0.012 0.001 0.15 "Width"
//@param Softness float 0.005 0.0001 0.1 "Softness"
//@param Frequency float 18 1 128 "Frequency"
//@param Wobble float 0.04 0 0.3 "Wobble"
//@param BranchLength float 0.36 0.02 0.7 "Branch Length"
//@param Slope float 0.65 0.05 4 "Slope"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Surface Detail): Branching fracture stamp for rock, ceramic, dried mud, and edge-damage masks.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Width, float Softness, float Frequency, float Wobble, float BranchLength, float Slope, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-.5;float trunk=1.0-smoothstep(Width,Width+Softness,abs(p.x-sin((p.y+Seed)*Frequency)*Wobble))*step(abs(p.y),.48);float branch=0;[unroll]for(int i=0;i<7;i++){float h=frac(sin((i+Seed)*31.17)*43758.5453);float y=lerp(-.42,.42,h);float dir=lerp(-1.0,1.0,frac(h*7.13));float2 q=p-float2(0,y);float d=abs(q.y-dir*q.x*Slope);float active=step(0.0,q.x*dir)*step(abs(q.x),BranchLength*lerp(.4,1.0,h));branch=max(branch,(1.0-smoothstep(Width*.7,Width*.7+Softness,d))*active);}float radial=1.0-smoothstep(.75,1.0,length(p)*2.0);float a=max(trunk,branch)*radial*CyBrushFlow*saturate(CyBrushPosition.z);return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Width, Softness, Frequency, Wobble, BranchLength, Slope, Seed + floor(b.seed * 997.0) * RandomSeed);
}
