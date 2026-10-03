//@name Height Erosion
//@category CyTexture Effets
//@input Image
//@halo full
//@param Radius float 2 0.25 64 "Radius"
//@param Talus float 0.015 0 1 "Talus"
//@param Strength float 12 0 128 "Strength"
//@param Erosion float 0.65 0 1 "Erosion"
//@param Deposition float 0.18 0 1 "Deposition"
//@param NoiseScale float 128 1 4096 "Noise Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param NoiseAmount float 0.35 0 1 "Noise Amount"
//@param HeightChannel color 1 0 0 0 "Height Channel"
//@param ChannelMask color 1 1 1 0 "Channel Mask"

// Imported from CyTexture (Effect / PBR): Iterative-looking height erosion approximation with talus, deposition, and noise-driven weathering.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Talus, float Strength, float Erosion, float Deposition, float NoiseScale, float Seed, float NoiseAmount, float3 HeightChannel, float3 ChannelMask)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 src=Texture2DSample(CyInput,CyInputSampler,UV);float h=dot(src.rgb,HeightChannel.rgb);float lower=h,average=0;[unroll]for(int i=0;i<16;i++){float a=i*6.283185/16.0;float sampleH=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(cos(a),sin(a))*t).rgb,HeightChannel.rgb);lower=min(lower,sampleH);average+=sampleH;}average/=16;float slope=saturate((h-lower-Talus)*Strength);float noise=frac(sin(dot(floor(UV*NoiseScale)+Seed,float2(12.9898,78.233)))*43758.5453);float eroded=lerp(h,lower,slope*Erosion*lerp(1.0-NoiseAmount,1.0,noise));eroded=lerp(eroded,average,Deposition*(1.0-slope));float3 outColor=lerp(src.rgb,eroded.xxx,ChannelMask.rgb);return float4(outColor,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Talus, Strength, Erosion, Deposition, NoiseScale, Seed, NoiseAmount, HeightChannel.rgb, ChannelMask.rgb);
}
