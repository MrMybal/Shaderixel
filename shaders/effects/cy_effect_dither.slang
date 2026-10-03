//@name Dither
//@category Traitements
//@input Image
//@halo 0
//@param Levels float 16 2 256 "Levels"
//@param Amount float 1 0 4 "Amount"

// Imported preset (Effect / Stylize): Ordered-style procedural dithering and bit-depth reduction.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Levels, float Amount)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=floor(UV*CyCanvasSize.xy); float n=frac(sin(dot(p,float2(12.9898,78.233)))*43758.5453)-0.5; float levels=max(Levels,2.0); float3 v=floor(saturate(s.rgb+n*Amount/levels)*levels)/max(levels-1.0,1.0); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Levels, Amount);
}
