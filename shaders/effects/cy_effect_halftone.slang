//@name Halftone
//@category Traitements
//@input Image
//@halo 0
//@param Scale float 96 2 2048 "Scale"
//@param Angle float 0.785 -6.283 6.283 "Angle"
//@param Softness float 0.03 0.0001 0.5 "Softness"
//@param PreserveColor float 0 0 1 "Preserve Color"
//@param InkColor color 0 0 0 1 "Ink Color"
//@param PaperColor color 1 1 1 1 "Paper Color"

// Imported preset (Effect / Stylize): Print-style halftone dots with angle, scale, softness, and color preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Angle, float Softness, float PreserveColor, float3 InkColor, float3 PaperColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float c=cos(Angle),sn=sin(Angle);float2 p=float2(c*UV.x-sn*UV.y,sn*UV.x+c*UV.y)*Scale;float2 cell=frac(p)-.5;float l=dot(s.rgb,float3(.2126,.7152,.0722));float dotMask=1.0-smoothstep(sqrt(saturate(1.0-l))*.48,sqrt(saturate(1.0-l))*.48+Softness,length(cell));float3 mono=lerp(PaperColor.rgb,InkColor.rgb,dotMask);return float4(lerp(mono,s.rgb,saturate(PreserveColor)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Angle, Softness, PreserveColor, InkColor.rgb, PaperColor.rgb);
}
