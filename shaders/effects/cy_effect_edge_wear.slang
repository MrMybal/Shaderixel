//@name Edge Wear
//@category CyTexture Effets
//@input Image
//@halo full
//@param Radius float 2 0.25 32 "Radius"
//@param EdgeStrength float 8 0 128 "Edge Strength"
//@param EdgePower float 0.8 0.05 8 "Edge Power"
//@param NoiseScale float 180 1 4096 "Noise Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param NoiseThreshold float 0.18 0 1 "Noise Threshold"
//@param NoiseContrast float 2.5 0 32 "Noise Contrast"
//@param HeightMinimum float 0 0 1 "Height Minimum"
//@param HeightMaximum float 1 0 1 "Height Maximum"
//@param Invert float 0 0 1 "Invert"
//@param HeightChannel color 1 0 0 0 "Height Channel"

// Imported from CyTexture (Effect / PBR): Builds a controllable worn-edge mask from source height or luminance with procedural breakup.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float EdgeStrength, float EdgePower, float NoiseScale, float Seed, float NoiseThreshold, float NoiseContrast, float HeightMinimum, float HeightMaximum, float Invert, float3 HeightChannel)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float h0=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,HeightChannel.rgb);float hx=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,HeightChannel.rgb)-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,HeightChannel.rgb);float hy=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,HeightChannel.rgb)-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,HeightChannel.rgb);float edge=pow(saturate(length(float2(hx,hy))*EdgeStrength),EdgePower);float n=frac(sin(dot(floor(UV*NoiseScale)+Seed,float2(12.9898,78.233)))*43758.5453);float wear=edge*saturate((n-NoiseThreshold)*NoiseContrast)*smoothstep(HeightMinimum,HeightMaximum,h0);wear=lerp(wear,1.0-wear,Invert);return float4(wear.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, EdgeStrength, EdgePower, NoiseScale, Seed, NoiseThreshold, NoiseContrast, HeightMinimum, HeightMaximum, Invert, HeightChannel.rgb);
}
