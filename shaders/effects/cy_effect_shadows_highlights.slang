//@name Shadows Highlights
//@category CyTexture Effets
//@input Image
//@halo 0
//@param ShadowAmount float 0.25 0 2 "Shadow Amount"
//@param HighlightAmount float 0.25 0 2 "Highlight Amount"
//@param Radius float 2 0.01 8 "Radius"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): Recovers shadows and compresses highlights while retaining HDR detail.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float ShadowAmount, float HighlightAmount, float Radius, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=max(dot(s.rgb,float3(.2126,.7152,.0722)),0.0); float shadow=pow(saturate(1.0-l),max(Radius,0.01))*ShadowAmount; float highlight=pow(saturate(l),max(Radius,0.01))*HighlightAmount; float3 v=s.rgb*(1.0+shadow)/(1.0+highlight*max(l,0.0)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ShadowAmount, HighlightAmount, Radius, Strength);
}
