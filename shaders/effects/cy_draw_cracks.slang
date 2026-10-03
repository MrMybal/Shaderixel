//@name Cracks
//@category Dessin
//@generator
//@halo 0
//@param Scale float 14 1 256 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Width float 0.035 0.001 0.5 "Width"
//@param Softness float 0.012 0.0001 0.2 "Softness"
//@param Breakup float 0.22 0 1 "Breakup"
//@param BreakupAmount float 0.18 0 1 "Breakup Amount"
//@param FillThreshold float 0.88 0 1 "Fill Threshold"
//@param FillAmount float 0 0 1 "Fill Amount"
//@param CrackOpacity float 1 0 1 "Crack Opacity"
//@param BackgroundOpacity float 0 0 1 "Background Opacity"
//@param CrackColor color 1 1 1 1 "Crack Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Surface): Voronoi fracture network with branching breakup, variable width, and optional filled cells.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Width, float Softness, float Breakup, float BreakupAmount, float FillThreshold, float FillAmount, float CrackOpacity, float BackgroundOpacity, float3 CrackColor, float3 BackgroundColor)
{
    float2 p=UV*max(Scale,1.0);float2 ip=floor(p),fp=frac(p);float nearest=10.0,second=10.0,cellValue=0.0;[unroll]for(int y=-1;y<=1;y++){[unroll]for(int x=-1;x<=1;x++){float2 o=float2(x,y),cell=ip+o;float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+Seed)*43758.5453);float d=length(o+rnd-fp);if(d<nearest){second=nearest;nearest=d;cellValue=rnd.x;}else if(d<second)second=d;}}float edge=second-nearest;float crack=1.0-smoothstep(Width,Width+Softness,edge);float breakup=step(Breakup,frac(sin((ip.x+ip.y+cellValue)*37.13+Seed)*43758.5453));crack*=lerp(1.0,breakup,BreakupAmount);float fill=step(FillThreshold,cellValue)*FillAmount;float m=max(crack,fill);return float4(lerp(BackgroundColor.rgb,CrackColor.rgb,m),lerp(BackgroundOpacity,CrackOpacity,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Width, Softness, Breakup, BreakupAmount, FillThreshold, FillAmount, CrackOpacity, BackgroundOpacity, CrackColor.rgb, BackgroundColor.rgb);
}
