//@name Exposure
//@category CyTexture Effets
//@input Image
//@halo 0
//@param ExposureEV float 0 -16 16 "Exposure EV"

// Imported from CyTexture (Effect / Color & Tone): Linear HDR exposure in photographic EV stops.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float ExposureEV)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return float4(s.rgb*exp2(ExposureEV),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ExposureEV);
}
