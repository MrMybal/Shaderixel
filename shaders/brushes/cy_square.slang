//@name Square
//@category CyTexture Formes
//@selfflow
//@spacing 0.1

// Imported from CyTexture (Brush / Shape Tips): White local-space square material brush. Foreground color, pressure, hardness, and flow are applied by the brush contract.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 d=abs(UV-0.5); float q=max(d.x,d.y)/0.5; float a=1.0-smoothstep(saturate(CyBrushHardness),1.0,q); a*=CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize));
}
