//@name Chroma Key
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Tolerance float 0.25 0 4 "Tolerance"
//@param Softness float 0.15 0.0001 2 "Softness"
//@param Despill float 0.5 0 1 "Despill"
//@param KeyColor color 0 1 0 1 "Key Color"

// Imported from CyTexture (Effect / Color & Tone): Removes a selected key color with tolerance, softness, edge restoration, and spill suppression.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tolerance, float Softness, float Despill, float3 KeyColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 n=s.rgb/max(dot(s.rgb,float3(.333333,.333333,.333333)),.001); float3 k=KeyColor.rgb/max(dot(KeyColor.rgb,float3(.333333,.333333,.333333)),.001); float d=distance(n,k); float keep=smoothstep(Tolerance,Tolerance+max(Softness,.0001),d); float spill=saturate(1.0-keep)*Despill; float3 v=lerp(s.rgb,dot(s.rgb,float3(.2126,.7152,.0722)).xxx,spill); return float4(v,s.a*keep);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tolerance, Softness, Despill, KeyColor.rgb);
}
