//@name Triangle
//@category CyTexture Formes
//@selfflow
//@spacing 0.1
//@param Rotation float -1.5708 -6.283 6.283 "Rotation"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Shape Tips): Procedural triangular brush tip with rotation.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Rotation)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=(UV-0.5)*2.0; float a0=atan2(p.y,p.x)+Rotation; float sector=2.094395; float local=(frac(a0/sector+0.5)-0.5)*sector; float boundary=0.5/max(cos(local),0.0001); float q=length(p)/max(boundary,0.0001); float a=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Rotation);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
