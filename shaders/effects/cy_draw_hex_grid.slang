//@name Hex Grid
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Scale float 12 1 256 "Scale"
//@param LineWidth float 0.025 0.001 0.25 "Line Width"
//@param Softness float 0.008 0.0001 0.1 "Softness"
//@param NodeSize float 0.055 0.001 0.3 "Node Size"
//@param NodeIntensity float 0.5 0 1 "Node Intensity"
//@param LineOpacity float 1 0 1 "Line Opacity"
//@param CellOpacity float 1 0 1 "Cell Opacity"
//@param LineColor color 0.05 0.8 1.4 1 "Line Color"
//@param CellColor color 0.005 0.015 0.025 1 "Cell Color"

// Imported from CyTexture (Draw / Pattern): Hexagonal technical grid with independently controlled cells, borders, and corner nodes.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float LineWidth, float Softness, float NodeSize, float NodeIntensity, float LineOpacity, float CellOpacity, float3 LineColor, float3 CellColor)
{
    float2 p=UV*max(Scale,1.0); p.x*=1.1547005; float2 a=frac(p)-0.5; float2 b=frac(p+0.5)-0.5; float2 g=dot(a,a)<dot(b,b)?a:b; float h=max(abs(g.y),abs(g.x)*0.866025+abs(g.y)*0.5); float lineMask=1.0-smoothstep(LineWidth,LineWidth+Softness,abs(0.5-h)); float node=1.0-smoothstep(NodeSize,NodeSize+Softness,length(g)); float m=saturate(max(lineMask,node*NodeIntensity)); return float4(lerp(CellColor.rgb,LineColor.rgb,m),lerp(CellOpacity,LineOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, LineWidth, Softness, NodeSize, NodeIntensity, LineOpacity, CellOpacity, LineColor.rgb, CellColor.rgb);
}
