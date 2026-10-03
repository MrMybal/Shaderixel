//@name Levels
//@category CyTexture Effets
//@input Image
//@halo 0
//@param BlackPoint float 0 0 1 "Black Point"
//@param WhitePoint float 1 0 1 "White Point"
//@param Gamma float 1 0.05 8 "Gamma"

// Imported from CyTexture (Effect / Color & Tone): Black point, white point, and gamma correction.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float BlackPoint, float WhitePoint, float Gamma)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=saturate((s.rgb-BlackPoint)/max(WhitePoint-BlackPoint,0.0001)); v=pow(v,1.0/max(Gamma,0.0001)); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), BlackPoint, WhitePoint, Gamma);
}
