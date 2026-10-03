//@name Dissolve
//@category CyTexture Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param NoiseScale float 12 1 512 "Noise Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Threshold float 0.5 0 1 "Threshold"
//@param Softness float 0.03 0.0001 0.5 "Softness"
//@param EdgeWidth float 0.08 0.0001 0.5 "Edge Width"
//@param EdgeIntensity float 0.8 0 8 "Edge Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param EdgeColor color 2 0.35 0.02 1 "Edge Color"

// Imported from CyTexture (Blend / Utility): Seeded procedural dissolve blend with threshold, feathered edge, and HDR edge color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float NoiseScale, float Seed, float Threshold, float Softness, float EdgeWidth, float EdgeIntensity, float Opacity, float3 EdgeColor)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float2 p=UV*max(NoiseScale,1.0)+Seed;float2 id=floor(p),f=frac(p);f=f*f*(3.0-2.0*f);float a0=frac(sin(dot(id,float2(127.1,311.7)))*43758.5453),a1=frac(sin(dot(id+float2(1,0),float2(127.1,311.7)))*43758.5453),a2=frac(sin(dot(id+float2(0,1),float2(127.1,311.7)))*43758.5453),a3=frac(sin(dot(id+1.0,float2(127.1,311.7)))*43758.5453);float n=lerp(lerp(a0,a1,f.x),lerp(a2,a3,f.x),f.y);float reveal=smoothstep(Threshold-Softness,Threshold+Softness,n);float edge=1.0-saturate(abs(n-Threshold)/max(EdgeWidth,.0001));float alpha=saturate(reveal*fg.a*mask*Opacity);float3 front=lerp(fg.rgb,EdgeColor.rgb,edge*EdgeIntensity);return float4(lerp(bg.rgb,front,alpha),alpha+bg.a*(1.0-alpha));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), NoiseScale, Seed, Threshold, Softness, EdgeWidth, EdgeIntensity, Opacity, EdgeColor.rgb);
}
