//@name Halftone
//@category Formes
//@selfflow
//@spacing 0.1
//@param Cells float 8 1 64 "Cells"
//@param DotSize float 0.28 0.01 0.7 "Dot Size"
//@param Softness float 0.04 0.001 0.4 "Softness"

// Imported preset (Brush / Shape Tips): Circular halftone-dot brush with configurable cell count and dot size.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Cells, float DotSize, float Softness)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float shape=1.0-smoothstep(saturate(CyBrushHardness),1.0,length(p)*2.0); float2 f=frac(UV*max(Cells,1.0))-0.5; float dotMask=1.0-smoothstep(DotSize,min(0.71,DotSize+Softness),length(f)); float a=shape*dotMask*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Cells, DotSize, Softness);
}
