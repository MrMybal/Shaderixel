//@name Scratch
//@category CyTexture Détails
//@selfflow
//@spacing 0.45
//@param Length float 0.48 0.02 0.75 "Length"
//@param Width float 0.34 0.02 0.75 "Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param LineWidth float 0.008 0.001 0.12 "Line Width"
//@param Softness float 0.004 0.0001 0.08 "Softness"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Surface Detail): Directional clustered scratch tip for wear, brushed metal, and damage masks.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation, float LineWidth, float Softness, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-.5;float c=cos(Rotation),s=sin(Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float body=1.0-smoothstep(CyBrushHardness,1.0,length(float2(p.x/max(Length,.001),p.y/max(Width,.001))));float lanes=0;[unroll]for(int i=0;i<9;i++){float h=frac(sin((i+Seed)*17.13)*43758.5453);float y=(i/8.0-.5)*Width*1.7;float lane=(1.0-smoothstep(LineWidth,LineWidth+Softness,abs(p.y-y)))*step(abs(p.x),Length*lerp(.35,1.0,h));lanes=max(lanes,lane);}float a=body*lanes*CyBrushFlow*saturate(CyBrushPosition.z);return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation, LineWidth, Softness, Seed + floor(b.seed * 997.0) * RandomSeed);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
