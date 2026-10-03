//@name Equalize
//@category Traitements
//@input Image
//@halo 0
//@param Midpoint float 0.5 0 2 "Midpoint"
//@param Contrast float 6 0.01 32 "Contrast"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Adaptive sigmoidal contrast equalization suitable for SDR and HDR sources.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Midpoint, float Contrast, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float mapped=1.0/(1.0+exp(-(l-Midpoint)*Contrast)); float reference=1.0/(1.0+exp(Midpoint*Contrast)); mapped=saturate((mapped-reference)/max(1.0-reference,0.0001)); float3 v=s.rgb*(mapped/max(l,0.0001)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Midpoint, Contrast, Strength);
}
