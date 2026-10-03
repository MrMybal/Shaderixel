//@name LUT
//@category Traitements
//@input Image
//@input LUT
//@halo full
//@param LUTSize float 16 2 64 "LUT Size"
//@param FlipVertical float 0 0 1 "Flip Vertical"
//@param Strength float 0 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Applies a standard horizontal 2D strip LUT (N*N by N) with adjustable strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
static const uint LUTTexture = 1;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LUTSize, float FlipVertical, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(LUTSize,2.0); float3 c=saturate(s.rgb); c.g=lerp(c.g,1.0-c.g,saturate(FlipVertical)); float z=c.b*(n-1.0); float z0=floor(z); float z1=min(z0+1.0,n-1.0); float2 uv0=float2((z0*n+c.r*(n-1.0)+0.5)/(n*n),(c.g*(n-1.0)+0.5)/n); float2 uv1=float2((z1*n+c.r*(n-1.0)+0.5)/(n*n),(c.g*(n-1.0)+0.5)/n); float3 graded=lerp(Texture2DSample(LUTTexture,LUTTextureSampler,uv0).rgb,Texture2DSample(LUTTexture,LUTTextureSampler,uv1).rgb,frac(z)); return float4(lerp(s.rgb,graded,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LUTSize, FlipVertical, Strength);
}
