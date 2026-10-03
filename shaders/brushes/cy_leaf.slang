//@name Leaf
//@category CyTexture Formes
//@selfflow
//@spacing 0.1
//@param Length float 0.9 0.05 2 "Length"
//@param Width float 0.55 0.05 2 "Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Serration float 0.08 0 0.5 "Serration"
//@param SerrationCount float 9 1 64 "Serration Count"
//@param Softness float 0.02 0.0001 0.2 "Softness"
//@param VeinWidth float 0.03 0.001 0.3 "Vein Width"
//@param VeinStrength float 0.45 0 1 "Vein Strength"
//@param VeinColor color 0.25 0.55 0.08 1 "Vein Color"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Shape Tips): Procedural leaf stamp with serrated edge, central vein, and optional preserved color.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation, float Serration, float SerrationCount, float Softness, float VeinWidth, float VeinStrength, float4 VeinColor)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=(UV-0.5)*2.0; float c=cos(Rotation),s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); p.x/=max(Length,0.01); p.y/=max(Width,0.01); float profile=sqrt(saturate(1.0-p.x*p.x)); float serration=1.0-Serration*(0.5+0.5*sin((p.x+1.0)*SerrationCount*3.14159)); float edge=abs(p.y)-profile*serration; float body=1.0-smoothstep(0.0,Softness,edge); body*=step(abs(p.x),1.0); float vein=1.0-smoothstep(VeinWidth,VeinWidth+Softness,abs(p.y))*body; float3 rgb=lerp(CyBrushColor,VeinColor.rgb,vein*VeinStrength); float a=body*CyBrushFlow*saturate(CyBrushPosition.z); return float4(rgb,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation, Serration, SerrationCount, Softness, VeinWidth, VeinStrength, VeinColor);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
