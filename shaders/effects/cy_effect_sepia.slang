//@name Sepia
//@category Traitements
//@input Image
//@halo 0
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Classic sepia toning with adjustable intensity.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=float3(dot(s.rgb,float3(.393,.769,.189)),dot(s.rgb,float3(.349,.686,.168)),dot(s.rgb,float3(.272,.534,.131))); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength);
}
