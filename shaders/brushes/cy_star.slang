//@name Star
//@category Formes
//@selfflow
//@spacing 0.1
//@param Points float 5 2 32 "Points"
//@param InnerRadius float 0.42 0.01 0.99 "Inner Radius"
//@param PointSharpness float 1 0.05 8 "Point Sharpness"
//@param Rotation float -1.5708 -6.283 6.283 "Rotation"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported preset (Brush / Shape Tips): Configurable star or flower-shaped material brush.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Points, float InnerRadius, float PointSharpness, float Rotation)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=(UV-0.5)*2.0; float ang=atan2(p.y,p.x)+Rotation; float wave=0.5+0.5*cos(ang*max(Points,2.0)); float boundary=lerp(saturate(InnerRadius),1.0,pow(wave,max(PointSharpness,0.05))); float q=length(p)/max(boundary,0.0001); float a=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Points, InnerRadius, PointSharpness, Rotation);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
