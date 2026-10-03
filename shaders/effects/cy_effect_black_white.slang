//@name Black White
//@category Traitements
//@input Image
//@halo 0
//@param TintStrength float 0 0 1 "Tint Strength"
//@param Strength float 1 0 1 "Strength"
//@param ChannelWeights color 0.3 0.59 0.11 0 "Channel Weights"
//@param Tint color 0.75 0.62 0.42 1 "Tint"

// Imported preset (Effect / Color & Tone): Photoshop-style black-and-white conversion with independent source-channel weights and tint.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float TintStrength, float Strength, float3 ChannelWeights, float3 Tint)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,ChannelWeights.rgb)/max(dot(ChannelWeights.rgb,1.0),0.0001); float3 bw=lerp(l.xxx,l*Tint.rgb,saturate(TintStrength)); return float4(lerp(s.rgb,bw,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), TintStrength, Strength, ChannelWeights.rgb, Tint.rgb);
}
