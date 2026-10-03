//@name Vibrance
//@category Traitements
//@input Image
//@halo 0
//@param Vibrance float 0.5 -2 2 "Vibrance"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Adaptive saturation that protects already saturated colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Vibrance, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float mx=max(max(s.r,s.g),s.b); float mn=min(min(s.r,s.g),s.b); float sat=mx-mn; float l=dot(s.rgb,float3(.2126,.7152,.0722)); float amount=Vibrance*(1.0-saturate(sat)); float3 v=lerp(l.xxx,s.rgb,1.0+amount); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Vibrance, Strength);
}
