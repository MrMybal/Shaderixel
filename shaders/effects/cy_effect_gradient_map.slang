//@name Gradient Map
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Midpoint float 0.5 0.01 0.99 "Midpoint"
//@param Strength float 1 0 1 "Strength"
//@param ShadowColor color 0 0 0 1 "Shadow Color"
//@param MidColor color 0.5 0.5 0.5 1 "Mid Color"
//@param HighlightColor color 1 1 1 1 "Highlight Color"

// Imported from CyTexture (Effect / Color & Tone): Maps luminance through shadow, midtone, and highlight HDR colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Midpoint, float Strength, float3 ShadowColor, float3 MidColor, float3 HighlightColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=saturate(dot(s.rgb,float3(.2126,.7152,.0722))); float m=saturate(Midpoint); float3 low=lerp(ShadowColor.rgb,MidColor.rgb,saturate(l/max(m,0.0001))); float3 high=lerp(MidColor.rgb,HighlightColor.rgb,saturate((l-m)/max(1.0-m,0.0001))); float3 mapped=lerp(low,high,step(m,l)); return float4(lerp(s.rgb,mapped,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Midpoint, Strength, ShadowColor.rgb, MidColor.rgb, HighlightColor.rgb);
}
