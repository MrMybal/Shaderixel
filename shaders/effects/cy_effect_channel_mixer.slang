//@name Channel Mixer
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Strength float 1 0 1 "Strength"
//@param RedMix color 1 0 0 0 "Red Mix"
//@param GreenMix color 0 1 0 0 "Green Mix"
//@param BlueMix color 0 0 1 0 "Blue Mix"
//@param Constant color 0 0 0 0 "Constant"

// Imported from CyTexture (Effect / Color & Tone): Full RGB channel mixer using three editable matrix rows.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength, float3 RedMix, float3 GreenMix, float3 BlueMix, float3 Constant)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=float3(dot(s.rgb,RedMix.rgb),dot(s.rgb,GreenMix.rgb),dot(s.rgb,BlueMix.rgb))+Constant.rgb; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength, RedMix.rgb, GreenMix.rgb, BlueMix.rgb, Constant.rgb);
}
