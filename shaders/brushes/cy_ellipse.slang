//@name Ellipse
//@category Formes
//@selfflow
//@spacing 0.1
//@param Aspect float 2.5 0.05 16 "Aspect"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported preset (Brush / Shape Tips): Rotatable elliptical material brush for broad and narrow marks.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Aspect, float Rotation)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation); float s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); p.x*=max(Aspect,0.01); float q=length(p)/0.5; float a=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Aspect, Rotation);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
