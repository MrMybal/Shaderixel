//@name Color Lookup
//@category CyTexture Effets
//@input Image
//@input Lookup
//@halo full
//@param LUTSize float 16 2 64 "LUT Size"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): Applies a standard square 2D color lookup table with configurable grid size.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
static const uint LookupTexture = 1;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LUTSize, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(LUTSize,2.0); float blue=saturate(s.b)*(n-1.0); float slice0=floor(blue); float slice1=min(slice0+1.0,n-1.0); float grid=ceil(sqrt(n)); float2 cell0=float2(fmod(slice0,grid),floor(slice0/grid)); float2 cell1=float2(fmod(slice1,grid),floor(slice1/grid)); float2 local=(s.rg*(n-1.0)+0.5)/n; float2 uv0=(cell0+local)/grid; float2 uv1=(cell1+local)/grid; float3 lut=lerp(Texture2DSample(LookupTexture,LookupTextureSampler,uv0).rgb,Texture2DSample(LookupTexture,LookupTextureSampler,uv1).rgb,frac(blue)); return float4(lerp(s.rgb,lut,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LUTSize, Strength);
}
