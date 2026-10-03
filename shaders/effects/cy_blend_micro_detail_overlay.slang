//@name Micro Detail Overlay
//@category CyTexture Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Neutral float 0.5 -8 8 "Neutral"
//@param Strength float 1 0 16 "Strength"
//@param Opacity float 1 0 1 "Opacity"
//@param DetailChannel color 1 0 0 0 "Detail Channel"
//@param TargetChannels color 1 1 1 0 "Target Channels"

// Imported from CyTexture (Blend / PBR): Adds signed micro-detail to selected PBR channels with range-safe attenuation near black and white.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Neutral, float Strength, float Opacity, float4 DetailChannel, float3 TargetChannels)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float detail=dot(fg,DetailChannel)-Neutral;float a=saturate(fg.a*mask*Opacity);float3 headroom=min(bg.rgb,1.0-bg.rgb)*2.0;float3 modified=bg.rgb+detail*Strength*headroom*TargetChannels.rgb;float3 rgb=lerp(bg.rgb,modified,a);return float4(rgb,bg.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Neutral, Strength, Opacity, DetailChannel, TargetChannels.rgb);
}
