//@name Dry Media
//@category CyTexture Matières
//@selfflow
//@spacing 0.12
//@param Length float 0.49 0.01 0.75 "Length"
//@param Width float 0.25 0.01 0.75 "Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param GrainFrequency float 42 1 512 "Grain Frequency"
//@param GrainWarp float 1.4 0 8 "Grain Warp"
//@param GrainCells float 84 2 1024 "Grain Cells"
//@param Dryness float 0.23 0 1 "Dryness"
//@param Contrast float 3 0.01 16 "Contrast"
//@param Breakup float 0.9 0 1 "Breakup"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Natural Media): Broken dry-media brush with directional grain, edge breakup, and paint loading.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation, float GrainFrequency, float GrainWarp, float GrainCells, float Dryness, float Contrast, float Breakup, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation),s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float shape=1.0-smoothstep(saturate(CyBrushHardness),1.0,length(float2(p.x/max(Length,0.001),p.y/max(Width,0.001)))); float streak=0.5+0.5*sin((p.y/max(Width,0.001))*GrainFrequency+sin(p.x*37.0+Seed)*GrainWarp); float dust=frac(sin(dot(floor((p+0.5)*GrainCells)+Seed,float2(41.37,289.11)))*43758.5453); float grain=saturate((streak*dust-Dryness)*max(Contrast,0.01)); float a=shape*lerp(1.0,grain,saturate(Breakup))*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation, GrainFrequency, GrainWarp, GrainCells, Dryness, Contrast, Breakup, Seed + floor(b.seed * 997.0) * RandomSeed);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
