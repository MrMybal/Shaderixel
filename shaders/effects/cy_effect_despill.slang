//@name Despill
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Amount float 2 0 16 "Amount"
//@param SpillColor color 0 1 0 1 "Spill Color"

// Imported from CyTexture (Effect / Color & Tone): Reduces a selected spill color while preserving luminance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float3 SpillColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float dominance=max(dot(s.rgb,SpillColor.rgb)-max(s.r,s.b),0.0); float3 neutral=l.xxx; float3 v=lerp(s.rgb,neutral,saturate(dominance*Amount)); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, SpillColor.rgb);
}
