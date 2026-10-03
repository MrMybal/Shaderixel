//@name Channel Normalize
//@category Traitements
//@input Image
//@halo 0
//@param InputMinimum color 0 0 0 0 "Input Minimum"
//@param InputMaximum color 1 1 1 0 "Input Maximum"
//@param Gamma color 1 1 1 0 "Gamma"
//@param OutputMinimum color 0 0 0 0 "Output Minimum"
//@param OutputMaximum color 1 1 1 0 "Output Maximum"
//@param Channels color 1 1 1 0 "Channels"

// Imported preset (Effect / PBR): Normalizes and remaps independently selected packed PBR channels with per-channel gamma.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float3 InputMinimum, float3 InputMaximum, float3 Gamma, float3 OutputMinimum, float3 OutputMaximum, float3 Channels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 v=saturate((s.rgb-InputMinimum.rgb)/max(InputMaximum.rgb-InputMinimum.rgb,.0001));v=pow(v,max(Gamma.rgb,.01));v=lerp(OutputMinimum.rgb,OutputMaximum.rgb,v);return float4(lerp(s.rgb,v,Channels.rgb),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum.rgb, InputMaximum.rgb, Gamma.rgb, OutputMinimum.rgb, OutputMaximum.rgb, Channels.rgb);
}
