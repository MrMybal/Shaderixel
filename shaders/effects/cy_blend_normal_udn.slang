//@name Normal UDN
//@category CyTexture Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param DetailStrength float 1 0 8 "Detail Strength"
//@param Opacity float 1 0 1 "Opacity"

// Imported from CyTexture (Blend / PBR): UDN-style tangent-space normal blend for strong detail without multiplying the base Z component.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float DetailStrength, float Opacity)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float3 n1=normalize(bg.xyz*2.0-1.0);float3 n2=normalize(fg.xyz*2.0-1.0);n2.xy*=DetailStrength;float3 combined=normalize(float3(n1.xy+n2.xy,n1.z));float3 encoded=combined*.5+.5;float a=saturate(fg.a*mask*Opacity);return float4(lerp(bg.rgb,encoded,a),bg.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), DetailStrength, Opacity);
}
