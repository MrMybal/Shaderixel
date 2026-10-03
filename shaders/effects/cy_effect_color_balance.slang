//@name Color Balance
//@category Traitements
//@input Image
//@halo 0
//@param PreserveLuminosity float 1 0 1 "Preserve Luminosity"
//@param Strength float 1 0 1 "Strength"
//@param ShadowBalance color 0 0 0 0 "Shadow Balance"
//@param MidtoneBalance color 0 0 0 0 "Midtone Balance"
//@param HighlightBalance color 0 0 0 0 "Highlight Balance"

// Imported preset (Effect / Color & Tone): Independent shadow, midtone, and highlight color balance with luminosity preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float PreserveLuminosity, float Strength, float3 ShadowBalance, float3 MidtoneBalance, float3 HighlightBalance)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float sh=saturate((0.5-l)*2.0); float hi=saturate((l-0.5)*2.0); float mid=saturate(1.0-sh-hi); float3 v=s.rgb+ShadowBalance.rgb*sh+MidtoneBalance.rgb*mid+HighlightBalance.rgb*hi; float vl=dot(v,float3(.2126,.7152,.0722)); v+=PreserveLuminosity*(l-vl); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), PreserveLuminosity, Strength, ShadowBalance.rgb, MidtoneBalance.rgb, HighlightBalance.rgb);
}
