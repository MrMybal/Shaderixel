//@name Alpha Remap
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Minimum float 0 0 1 "Minimum"
//@param Maximum float 1 0 1 "Maximum"
//@param Feather float 0 0 0.5 "Feather"
//@param Gamma float 1 0.01 8 "Gamma"
//@param Invert float 0 0 1 "Invert"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): Remaps, feathers, inverts, and gamma-corrects alpha without changing RGB.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Minimum, float Maximum, float Feather, float Gamma, float Invert, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float a=smoothstep(Minimum-Feather,Maximum+Feather,s.a); a=pow(saturate(a),max(Gamma,0.001)); a=lerp(a,1.0-a,saturate(Invert)); return float4(s.rgb,lerp(s.a,a,saturate(Strength)));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Minimum, Maximum, Feather, Gamma, Invert, Strength);
}
