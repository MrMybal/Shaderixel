//@name VHS Glitch
//@category CyTexture Effets
//@input Image
//@halo full
//@param LineDensity float 240 8 4096 "Line Density"
//@param Phase float 0 -100 100 "Phase"
//@param GlitchThreshold float 0.87 0 0.999 "Glitch Threshold"
//@param GlitchSharpness float 2 0.1 16 "Glitch Sharpness"
//@param HorizontalJitter float 0.08 0 0.5 "Horizontal Jitter"
//@param TrackingFrequency float 85 0 2048 "Tracking Frequency"
//@param TrackingAmount float 0.003 0 0.1 "Tracking Amount"
//@param ColorDelay float 3 0 64 "Color Delay"
//@param Scanlines float 0.25 0 1 "Scanlines"
//@param Dropout float 0.35 0 1 "Dropout"

// Imported from CyTexture (Effect / Stylize): Analog VHS displacement with tracking noise, color delay, dropout, and scanline controls.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LineDensity, float Phase, float GlitchThreshold, float GlitchSharpness, float HorizontalJitter, float TrackingFrequency, float TrackingAmount, float ColorDelay, float Scanlines, float Dropout)
{
    float scanLine=floor(UV.y*max(LineDensity,1.0));float n=frac(sin(scanLine*12.9898+Phase*7.13)*43758.5453);float burst=pow(saturate((n-GlitchThreshold)/max(1.0-GlitchThreshold,.0001)),GlitchSharpness);float jitter=(n-.5)*HorizontalJitter+sin(UV.y*TrackingFrequency+Phase)*TrackingAmount;float2 uv=UV+float2(jitter*burst,0);float delay=ColorDelay/max(CyCanvasSize.x,1.0);float r=Texture2DSample(CyInput,CyInputSampler,uv+float2(delay,0)).r;float g=Texture2DSample(CyInput,CyInputSampler,uv).g;float b=Texture2DSample(CyInput,CyInputSampler,uv-float2(delay,0)).b;float scan=lerp(1.0,0.75+0.25*sin(UV.y*CyCanvasSize.y*3.14159),Scanlines);float dropout=1.0-burst*Dropout;float4 src=Texture2DSample(CyInput,CyInputSampler,uv);return float4(float3(r,g,b)*scan*dropout,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LineDensity, Phase, GlitchThreshold, GlitchSharpness, HorizontalJitter, TrackingFrequency, TrackingAmount, ColorDelay, Scanlines, Dropout);
}
