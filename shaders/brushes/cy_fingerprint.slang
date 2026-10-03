//@name Fingerprint
//@category Détails
//@selfflow
//@spacing 0.45
//@param Aspect float 0.72 0.1 4 "Aspect"
//@param RidgeCount float 18 2 96 "Ridge Count"
//@param RidgeWidth float 0.36 0.01 0.95 "Ridge Width"
//@param Softness float 0.05 0.001 0.4 "Softness"
//@param WhorlCount float 2.2 0 16 "Whorl Count"
//@param WhorlAmount float 0.08 0 0.5 "Whorl Amount"
//@param GapScale float 22 1 256 "Gap Scale"
//@param GapAmount float 0.16 0 1 "Gap Amount"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported preset (Brush / Surface Detail): Fingerprint-like ridge stamp for forensic detail, grime, decals, and roughness variation.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Aspect, float RidgeCount, float RidgeWidth, float Softness, float WhorlCount, float WhorlAmount, float GapScale, float GapAmount, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=(UV-.5)*2.0;p.x*=Aspect;float r=length(p);float angle=atan2(p.y,p.x);float ridge=abs(frac((r+sin(angle*WhorlCount)*WhorlAmount)*RidgeCount)-.5)*2.0;float ridges=1.0-smoothstep(RidgeWidth,RidgeWidth+Softness,ridge);float gapNoise=frac(sin(dot(floor(p*GapScale)+Seed,float2(12.9898,78.233)))*43758.5453);float gaps=step(GapAmount,gapNoise);float shape=1.0-smoothstep(.78,1.0,r);float a=ridges*gaps*shape*CyBrushFlow*saturate(CyBrushPosition.z);return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Aspect, RidgeCount, RidgeWidth, Softness, WhorlCount, WhorlAmount, GapScale, GapAmount, Seed + floor(b.seed * 997.0) * RandomSeed);
}
