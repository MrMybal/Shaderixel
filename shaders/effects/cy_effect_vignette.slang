//@name Vignette
//@category Traitements
//@input Image
//@halo 0
//@param Aspect float 1 0.01 8 "Aspect"
//@param Radius float 0.45 0.01 2 "Radius"
//@param Softness float 0.25 0.001 1 "Softness"
//@param Intensity float 0.8 0 1 "Intensity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param VignetteColor color 0 0 0 1 "Vignette Color"

// Imported preset (Effect / Color & Tone): Elliptical soft vignette with tint and HDR intensity.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float Softness, float Intensity, float3 Center, float3 VignetteColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float v=smoothstep(Radius-Softness,Radius+Softness,d); float3 c=lerp(s.rgb,s.rgb*VignetteColor.rgb,v*Intensity); return float4(c,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, Softness, Intensity, float3(Center, 0.0), VignetteColor.rgb);
}
