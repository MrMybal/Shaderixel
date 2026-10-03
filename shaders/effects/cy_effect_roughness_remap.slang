//@name Roughness Remap
//@category CyTexture Effets
//@input Image
//@halo 0
//@param InputMinimum float 0 0 1 "Input Minimum"
//@param InputMaximum float 1 0 1 "Input Maximum"
//@param OutputMinimum float 0.04 0 1 "Output Minimum"
//@param OutputMaximum float 1 0 1 "Output Maximum"
//@param Gamma float 1 0.01 8 "Gamma"
//@param PhysicalMinimum float 0.02 0 1 "Physical Minimum"
//@param MicroScale float 256 1 4096 "Micro Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param MicroVariation float 0.03 0 0.5 "Micro Variation"
//@param SourceChannel color 1 0 0 0 "Source Channel"
//@param OutputChannels color 1 1 1 0 "Output Channels"

// Imported from CyTexture (Effect / PBR): PBR roughness remap with range compression, gamma, microvariation, and physical clamping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float InputMinimum, float InputMaximum, float OutputMinimum, float OutputMaximum, float Gamma, float PhysicalMinimum, float MicroScale, float Seed, float MicroVariation, float3 SourceChannel, float3 OutputChannels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float rough=dot(s.rgb,SourceChannel.rgb);rough=saturate((rough-InputMinimum)/max(InputMaximum-InputMinimum,.0001));rough=pow(rough,max(Gamma,.01));rough=lerp(OutputMinimum,OutputMaximum,rough);float n=frac(sin(dot(floor(UV*MicroScale)+Seed,float2(12.9898,78.233)))*43758.5453)-.5;rough=clamp(rough+n*MicroVariation,PhysicalMinimum,1.0);float3 outColor=lerp(s.rgb,rough.xxx,OutputChannels.rgb);return float4(outColor,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum, InputMaximum, OutputMinimum, OutputMaximum, Gamma, PhysicalMinimum, MicroScale, Seed, MicroVariation, SourceChannel.rgb, OutputChannels.rgb);
}
