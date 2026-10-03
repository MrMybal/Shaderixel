//@name Radial
//@category CyTexture Masques
//@generator
//@halo 0
//@param Radius float 0.35 0.001 2 "Radius"
//@param Softness float 0.1 0.0001 1 "Softness"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Invert float 0 0 1 "Invert"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported from CyTexture (Mask / Mask): Configurable radial mask independent of source color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Softness, float Aspect, float Invert, float3 Center)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float v=1.0-smoothstep(Radius-Softness,Radius+Softness,d); v=lerp(v,1.0-v,saturate(Invert)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Softness, Aspect, Invert, float3(Center, 0.0));
}
