//@name Stitch
//@category CyTexture Détails
//@selfflow
//@spacing 0.45
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Length float 0.92 0.05 1.5 "Length"
//@param Stitches float 7 1 32 "Stitches"
//@param Duty float 0.62 0.05 0.95 "Duty"
//@param ThreadWidth float 0.025 0.001 0.25 "Thread Width"
//@param Softness float 0.006 0.0001 0.1 "Softness"
//@param KnotRadius float 0.035 0.001 0.2 "Knot Radius"
//@param KnotAmount float 0.35 0 1 "Knot Amount"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Surface Detail): Repeatable dashed stitch tip with thread thickness, spacing, angle, and knot controls.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Rotation, float Length, float Stitches, float Duty, float ThreadWidth, float Softness, float KnotRadius, float KnotAmount)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-.5;float c=cos(Rotation),s=sin(Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float u=(p.x/max(Length,.001)+.5)*max(Stitches,1.0);float dash=step(frac(u),Duty)*step(abs(p.x),Length*.5);float thread=(1.0-smoothstep(ThreadWidth,ThreadWidth+Softness,abs(p.y)))*dash;float knot=1.0-smoothstep(KnotRadius,KnotRadius+Softness,length(float2((frac(u)-.5)/max(Stitches,1.0),p.y)));float a=max(thread,knot*KnotAmount)*CyBrushFlow*saturate(CyBrushPosition.z);return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Rotation, Length, Stitches, Duty, ThreadWidth, Softness, KnotRadius, KnotAmount);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
