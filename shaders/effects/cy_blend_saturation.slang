//@name Saturation
//@category CyTexture Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Opacity float 1 0 1 "Opacity"

// Imported from CyTexture (Blend / Compositing): Transfers foreground saturation while preserving background hue and luminance in YIQ space.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float m=Texture2DSample(CyMask,CyMaskSampler,UV).r;float y=dot(bg.rgb,float3(.299,.587,.114));float bi=dot(bg.rgb,float3(.596,-.275,-.321));float bq=dot(bg.rgb,float3(.212,-.523,.311));float fi=dot(fg.rgb,float3(.596,-.275,-.321));float fq=dot(fg.rgb,float3(.212,-.523,.311));float chroma=length(float2(fi,fq));float2 hue=normalize(float2(bi,bq)+.000001);float i=hue.x*chroma,q=hue.y*chroma;float3 blend=float3(y+.956*i+.621*q,y-.272*i-.647*q,y-1.106*i+1.703*q);float a=saturate(fg.a*m*Opacity);return float4(lerp(bg.rgb,blend,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity);
}
