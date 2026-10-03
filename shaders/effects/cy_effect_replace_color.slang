//@name Replace Color
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Tolerance float 0.18 0 1 "Tolerance"
//@param Softness float 0.12 0.0001 1 "Softness"
//@param Strength float 1 0 1 "Strength"
//@param SourceColor color 1 0 0 1 "Source Color"
//@param TargetColor color 0 0.4 1 1 "Target Color"

// Imported from CyTexture (Effect / Color & Tone): Replaces a selected color while preserving source shading.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tolerance, float Softness, float Strength, float3 SourceColor, float3 TargetColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float d=length(saturate(s.rgb)-SourceColor.rgb); float m=1.0-smoothstep(Tolerance,Tolerance+max(Softness,0.0001),d); float sourceLum=max(dot(SourceColor.rgb,float3(.2126,.7152,.0722)),0.0001); float pixelLum=dot(s.rgb,float3(.2126,.7152,.0722)); float3 replacement=TargetColor.rgb*(pixelLum/sourceLum); return float4(lerp(s.rgb,replacement,m*saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tolerance, Softness, Strength, SourceColor.rgb, TargetColor.rgb);
}
