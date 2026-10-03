//@name Brightness Contrast
//@category Traitements
//@input Image
//@halo 0
//@param Brightness float 0 -4 4 "Brightness"
//@param Contrast float 1 0 8 "Contrast"
//@param Pivot float 0.5 0 2 "Pivot"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): HDR-safe brightness and pivoted contrast adjustment.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Brightness, float Contrast, float Pivot, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=(s.rgb-Pivot)*Contrast+Pivot+Brightness; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Brightness, Contrast, Pivot, Strength);
}
