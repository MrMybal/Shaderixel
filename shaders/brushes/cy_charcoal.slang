//@name Charcoal
//@category CyTexture Matières
//@selfflow
//@spacing 0.12
//@param Length float 0.48 0.01 0.7 "Length"
//@param Width float 0.24 0.01 0.7 "Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param GrainScale float 72 2 512 "Grain Scale"
//@param Threshold float 0.2 0 1 "Threshold"
//@param Breakup float 0.7 0 1 "Breakup"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Natural Media): Directional charcoal tip with streaky broken grain.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation, float GrainScale, float Threshold, float Breakup, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation); float s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float q=length(float2(p.x/max(Length,0.001),p.y/max(Width,0.001))); float shape=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); float2 cell=floor(float2((p.x+0.5)*GrainScale*0.2,(p.y+0.5)*GrainScale)+Seed); float n=frac(sin(dot(cell,float2(41.37,289.11)))*43758.5453); float grain=lerp(1.0,smoothstep(Threshold,1.0,n),saturate(Breakup)); float a=shape*grain*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation, GrainScale, Threshold, Breakup, Seed + floor(b.seed * 997.0) * RandomSeed);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
