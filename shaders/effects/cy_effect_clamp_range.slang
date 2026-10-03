//@name Clamp Range
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Minimum float 0 -32 32 "Minimum"
//@param Maximum float 1 -32 32 "Maximum"

// Imported from CyTexture (Effect / Color & Tone): Clamps HDR RGB to a configurable minimum and maximum range.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Minimum, float Maximum)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return float4(clamp(s.rgb,Minimum,Maximum),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Minimum, Maximum);
}
