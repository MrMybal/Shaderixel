//@name Rake
//@category CyTexture Matières
//@selfflow
//@spacing 0.12
//@param Length float 0.49 0.01 0.75 "Length"
//@param Width float 0.36 0.01 0.75 "Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Tines float 9 1 64 "Tines"
//@param TineWidth float 0.42 0.01 0.99 "Tine Width"
//@param Softness float 0.05 0.001 0.5 "Softness"
//@param Taper float 0.7 0.01 8 "Taper"
//@param Phase float 0 -10 10 "Phase"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Natural Media): Multi-tine rake brush for hair, fur, grass, scratches, and parallel texture strokes.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation, float Tines, float TineWidth, float Softness, float Taper, float Phase)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation),s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float body=1.0-smoothstep(saturate(CyBrushHardness),1.0,length(float2(p.x/max(Length,0.001),p.y/max(Width,0.001)))); float lane=abs(frac((p.y/max(Width,0.001)*0.5+0.5)*max(Tines,1.0)+Phase)-0.5)*2.0; float tine=1.0-smoothstep(TineWidth,TineWidth+Softness,lane); float taper=pow(saturate(1.0-abs(p.x)/max(Length,0.001)),Taper); float a=body*tine*taper*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation, Tines, TineWidth, Softness, Taper, Phase);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
