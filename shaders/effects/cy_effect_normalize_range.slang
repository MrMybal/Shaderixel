//@name Normalize Range
//@category CyTexture Effets
//@input Image
//@halo 0
//@param InputMinimum float 0 -32 32 "Input Minimum"
//@param InputMaximum float 1 -32 32 "Input Maximum"
//@param OutputMinimum float 0 -32 32 "Output Minimum"
//@param OutputMaximum float 1 -32 32 "Output Maximum"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): Remaps a chosen input range to a configurable output range.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float InputMinimum, float InputMaximum, float OutputMinimum, float OutputMaximum, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=(s.rgb-InputMinimum)/max(InputMaximum-InputMinimum,.0001); v=lerp(OutputMinimum,OutputMaximum,v); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum, InputMaximum, OutputMinimum, OutputMaximum, Strength);
}
