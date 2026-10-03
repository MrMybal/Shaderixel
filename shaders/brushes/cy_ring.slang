//@name Ring
//@category Formes
//@selfflow
//@spacing 0.1
//@param Radius float 0.32 0.01 0.49 "Radius"
//@param Thickness float 0.075 0.001 0.4 "Thickness"

// Imported preset (Brush / Shape Tips): Ring-shaped brush for outlines, highlights, and circular stamps.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Radius, float Thickness)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float d=abs(length(UV-0.5)-Radius)/max(Thickness,0.0001); float a=1.0-smoothstep(saturate(CyBrushHardness),1.0,d); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Radius, Thickness);
}
