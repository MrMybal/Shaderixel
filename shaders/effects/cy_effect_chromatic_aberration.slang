//@name Chromatic Aberration
//@category Traitements
//@input Image
//@halo full
//@param Amount float 0.015 -0.25 0.25 "Amount"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Color & Tone): Radial RGB channel separation for lens effects.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float3 Center)
{
    float2 dir=UV-Center.xy; float2 o=dir*Amount; float r=Texture2DSample(CyInput,CyInputSampler,saturate(UV+o)).r; float4 g=Texture2DSample(CyInput,CyInputSampler,UV); float b=Texture2DSample(CyInput,CyInputSampler,saturate(UV-o)).b; return float4(r,g.g,b,g.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, float3(Center, 0.0));
}
