//@name Tint
//@category Traitements
//@input Image
//@halo 0
//@param Strength float 1 0 1 "Strength"
//@param Tint color 1 1 1 1 "Tint"

// Imported preset (Effect / Color & Tone): Multiplies the source by an HDR tint with a blendable strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength, float3 Tint)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return lerp(s,float4(s.rgb*Tint,s.a),saturate(Strength));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength, Tint.rgb);
}
