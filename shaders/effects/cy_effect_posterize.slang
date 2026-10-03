//@name Posterize
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Levels float 8 2 256 "Levels"

// Imported from CyTexture (Effect / Stylize): Quantizes RGB to a configurable number of levels.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Levels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(Levels,2.0); return float4(floor(s.rgb*n)/max(n-1.0,1.0),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Levels);
}
