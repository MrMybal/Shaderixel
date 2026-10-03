//@name Watercolor
//@category CyTexture Matières
//@selfflow
//@spacing 0.12
//@param Opacity float 0.7 0 1 "Opacity"
//@param EdgePosition float 0.72 0.05 1.5 "Edge Position"
//@param EdgeWidth float 0.09 0.001 0.5 "Edge Width"
//@param EdgeSharpness float 2 0.1 16 "Edge Sharpness"
//@param EdgePooling float 0.45 0 4 "Edge Pooling"
//@param GrainScale float 96 2 1024 "Grain Scale"
//@param GrainAmount float 0.22 0 1 "Grain Amount"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"

// Imported from CyTexture (Brush / Natural Media): Soft watercolor wash with pigment pooling, paper grain, and translucent interior.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Opacity, float EdgePosition, float EdgeWidth, float EdgeSharpness, float EdgePooling, float GrainScale, float GrainAmount, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float d=length(p)*2.0; float2 q=floor(UV*max(GrainScale,1.0)+Seed); float n=frac(sin(dot(q,float2(127.1,311.7)))*43758.5453); float body=1.0-smoothstep(saturate(CyBrushHardness),1.0,d); float edge=pow(saturate(1.0-abs(d-EdgePosition)/max(EdgeWidth,0.0001)),max(EdgeSharpness,0.1))*EdgePooling; float pigment=saturate(body*lerp(1.0,n,GrainAmount)+edge); float a=pigment*CyBrushFlow*saturate(CyBrushPosition.z)*Opacity; return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Opacity, EdgePosition, EdgeWidth, EdgeSharpness, EdgePooling, GrainScale, GrainAmount, Seed + floor(b.seed * 997.0) * RandomSeed);
}
