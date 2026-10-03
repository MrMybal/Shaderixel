//@name Bristle
//@category Matières
//@selfflow
//@spacing 0.12
//@param Length float 0.48 0.01 0.7 "Length"
//@param Width float 0.22 0.01 0.7 "Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param BristleCount float 14 1 64 "Bristle Count"
//@param BristleWidth float 0.55 0.01 0.99 "Bristle Width"
//@param Softness float 0.06 0.001 0.5 "Softness"
//@param Separation float 0.85 0 1 "Separation"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported preset (Brush / Natural Media): Flat directional brush with individually separated bristle bands.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation, float BristleCount, float BristleWidth, float Softness, float Separation)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation); float s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float q=length(float2(p.x/max(Length,0.001),p.y/max(Width,0.001))); float shape=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); float stripe=abs(frac((p.y/max(Width,0.001)*0.5+0.5)*max(BristleCount,1.0))-0.5)*2.0; float bands=1.0-smoothstep(BristleWidth,min(1.0,BristleWidth+Softness),stripe); float a=shape*lerp(1.0,bands,saturate(Separation))*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation, BristleCount, BristleWidth, Softness, Separation);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
