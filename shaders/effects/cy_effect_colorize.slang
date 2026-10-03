//@name Colorize
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Gamma float 1 0.01 8 "Gamma"
//@param Strength float 1 0 1 "Strength"
//@param ShadowColor color 0.02 0.04 0.12 1 "Shadow Color"
//@param HighlightColor color 1 0.35 0.08 1 "Highlight Color"

// Imported from CyTexture (Effect / Color & Tone): Maps luminance between two HDR colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Gamma, float Strength, float3 ShadowColor, float3 HighlightColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=saturate(dot(s.rgb,float3(.2126,.7152,.0722))); float3 v=lerp(ShadowColor.rgb,HighlightColor.rgb,pow(l,max(Gamma,0.01))); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Gamma, Strength, ShadowColor.rgb, HighlightColor.rgb);
}
