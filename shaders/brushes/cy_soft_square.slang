//@name Soft Square
//@category CyTexture Formes
//@selfflow
//@spacing 0.1
//@param CornerRadius float 0.25 0 1 "Corner Radius"

// Imported from CyTexture (Brush / Shape Tips): Rounded soft-square tip with controllable corner radius.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float CornerRadius)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=abs((UV-0.5)*2.0); float r=saturate(CornerRadius); float2 q=max(p-(1.0-r),0.0); float d=length(q)+min(max(p.x-(1.0-r),p.y-(1.0-r)),0.0)-r; float e=max((1.0-saturate(CyBrushHardness))*0.35,0.002); float a=1.0-smoothstep(-e,e,d); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), CornerRadius);
}
