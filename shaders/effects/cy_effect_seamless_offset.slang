//@name Seamless Offset
//@category CyTexture Effets
//@input Image
//@halo full
//@param Feather float 0.08 0.001 0.5 "Feather"
//@param Strength float 1 0 1 "Strength"
//@param Offset float2 0.5 0.5 0 1 "Offset"

// Imported from CyTexture (Effect / PBR): Wraps and crossfades an offset texture to reduce visible borders during seamless texture authoring.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Feather, float Strength, float3 Offset)
{
    float2 uv=frac(UV+Offset.xy);float4 base=Texture2DSample(CyInput,CyInputSampler,uv);float2 edge=min(uv,1.0-uv);float wx=1.0-smoothstep(0.0,Feather,edge.x);float wy=1.0-smoothstep(0.0,Feather,edge.y);float4 x=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(.5,0)));float4 y=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(0,.5)));float4 xy=Texture2DSample(CyInput,CyInputSampler,frac(uv+.5));float4 blended=lerp(lerp(base,x,wx),lerp(y,xy,wx),wy);return lerp(base,blended,Strength);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Feather, Strength, float3(Offset, 0.0));
}
