//@name Film Grain
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Amount float 0.04 0 1 "Amount"
//@param Seed float 0 -10000 10000 "Seed"
//@param Colored float 0 0 1 "Colored"

// Imported from CyTexture (Effect / Stylize): Resolution-aware monochrome or colored film grain.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float Seed, float Colored)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=floor(UV*CyCanvasSize.xy)+Seed; float r=frac(sin(dot(p,float2(12.9898,78.233)))*43758.5453)-0.5; float g=frac(sin(dot(p,float2(39.346,11.135)))*24634.6345)-0.5; float b=frac(sin(dot(p,float2(73.156,52.235)))*56445.2345)-0.5; float3 n=lerp(r.xxx,float3(r,g,b),saturate(Colored)); return float4(s.rgb+n*Amount,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, Seed, Colored);
}
