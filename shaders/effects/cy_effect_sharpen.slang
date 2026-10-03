//@name Sharpen
//@category Traitements
//@input Image
//@halo full
//@param Strength float 0.5 0 1 "Strength"

// Imported preset (Effect / Detail): Five-tap sharpen filter with live strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength)
{
    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0)); float4 c=Texture2DSample(CyInput,CyInputSampler,UV); float4 n=Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0))+Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0))+Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y))+Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)); return lerp(c,c*5.0-n,saturate(Strength));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength);
}
