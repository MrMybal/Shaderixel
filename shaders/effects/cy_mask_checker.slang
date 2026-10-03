//@name Checker
//@category CyTexture Masques
//@generator
//@halo 0
//@param Tiling float 8 1 512 "Tiling"
//@param Invert float 0 0 1 "Invert"

// Imported from CyTexture (Mask / Mask): Procedural checker mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tiling, float Invert)
{
    float2 c=floor(UV*max(Tiling,1.0)); float v=fmod(c.x+c.y,2.0); v=lerp(v,1.0-v,saturate(Invert)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tiling, Invert);
}
