//@name Dehaze
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Amount float 0.5 -2 2 "Amount"
//@param MinimumTransmission float 0.15 0.01 1 "Minimum Transmission"
//@param Strength float 1 0 1 "Strength"
//@param HazeColor color 1 1 1 1 "Haze Color"

// Imported from CyTexture (Effect / Color & Tone): Restores contrast and color density in hazy or washed-out images.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float MinimumTransmission, float Strength, float3 HazeColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float dark=min(s.r,min(s.g,s.b)); float transmission=max(1.0-Amount*dark,MinimumTransmission); float3 v=(s.rgb-HazeColor.rgb)/transmission+HazeColor.rgb; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, MinimumTransmission, Strength, HazeColor.rgb);
}
