//@name Selective Color
//@category Traitements
//@input Image
//@halo 0
//@param Tolerance float 0.2 0 1 "Tolerance"
//@param Softness float 0.15 0.0001 1 "Softness"
//@param Strength float 1 0 1 "Strength"
//@param TargetColor color 1 0 0 1 "Target Color"
//@param Correction color 0 0 0 0 "Correction"

// Imported preset (Effect / Color & Tone): Selects a color range and applies an RGB correction with soft tolerance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tolerance, float Softness, float Strength, float3 TargetColor, float3 Correction)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float d=length(saturate(s.rgb)-TargetColor.rgb); float m=1.0-smoothstep(Tolerance,Tolerance+max(Softness,0.0001),d); float3 v=s.rgb+Correction.rgb*m; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tolerance, Softness, Strength, TargetColor.rgb, Correction.rgb);
}
