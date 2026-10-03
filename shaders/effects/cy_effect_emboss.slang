//@name Emboss
//@category CyTexture Effets
//@input Image
//@halo full
//@param Angle float 0.785 -6.283 6.283 "Angle"
//@param Radius float 1 0.25 32 "Radius"
//@param Strength float 2 0 16 "Strength"
//@param Bias float 0.5 -2 2 "Bias"
//@param Blend float 1 0 1 "Blend"

// Imported from CyTexture (Effect / Detail): Directional emboss filter with adjustable bias.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Angle, float Radius, float Strength, float Bias, float Blend)
{
    float2 t=float2(cos(Angle)/max(CyCanvasSize.x,1.0),sin(Angle)/max(CyCanvasSize.y,1.0))*Radius; float4 a=Texture2DSample(CyInput,CyInputSampler,UV-t); float4 b=Texture2DSample(CyInput,CyInputSampler,UV+t); float3 e=(a.rgb-b.rgb)*Strength+Bias; float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return float4(lerp(s.rgb,e,saturate(Blend)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Angle, Radius, Strength, Bias, Blend);
}
