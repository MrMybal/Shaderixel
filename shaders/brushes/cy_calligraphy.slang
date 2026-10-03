//@name Calligraphy
//@category Formes
//@selfflow
//@spacing 0.1
//@param Length float 0.46 0.01 0.7 "Length"
//@param Width float 0.08 0.005 0.5 "Width"
//@param Rotation float -0.65 -6.283 6.283 "Rotation"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported preset (Brush / Shape Tips): Angled calligraphy nib with editable width, length, and rotation.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation); float s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float q=length(float2(p.x/max(Length,0.001),p.y/max(Width,0.001))); float a=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
