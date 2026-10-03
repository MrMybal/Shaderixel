//@name Cross
//@category CyTexture Formes
//@selfflow
//@spacing 0.1
//@param ArmLength float 0.42 0.01 0.5 "Arm Length"
//@param ArmWidth float 0.09 0.005 0.49 "Arm Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Shape Tips): Rotatable cross stamp with adjustable arms.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float ArmLength, float ArmWidth, float Rotation)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation); float s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float d1=max(abs(p.x)-ArmLength,abs(p.y)-ArmWidth); float d2=max(abs(p.x)-ArmWidth,abs(p.y)-ArmLength); float d=min(d1,d2); float e=max((1.0-saturate(CyBrushHardness))*0.08,0.001); float a=1.0-smoothstep(-e,e,d); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), ArmLength, ArmWidth, Rotation);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
