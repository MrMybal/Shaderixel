//@name RNM Normal
//@category CyTexture Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Opacity float 1 0 1 "Opacity"

// Imported from CyTexture (Blend / PBR): Reoriented Normal Mapping blend for combining two tangent-space normal maps without flattening detail.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float m=Texture2DSample(CyMask,CyMaskSampler,UV).r;float3 n1=normalize(bg.xyz*2.0-1.0);float3 n2=normalize(fg.xyz*2.0-1.0);n1.z+=1.0;n2.xy*=-1.0;float3 r=normalize(n1*dot(n1,n2)/max(n1.z,.0001)-n2);float3 blend=r*.5+.5;float a=saturate(fg.a*m*Opacity);return float4(lerp(bg.rgb,blend,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity);
}
