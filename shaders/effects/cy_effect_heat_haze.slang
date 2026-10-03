//@name Heat Haze
//@category CyTexture Effets
//@input Image
//@halo full
//@param Scale float 4 0.01 128 "Scale"
//@param Phase float 0 -100 100 "Phase"
//@param Distortion float 0.7 0 8 "Distortion"
//@param ReferencePixels float 24 0 512 "Reference Pixels"
//@param UseAlphaMask float 0 0 1 "Use Alpha Mask"
//@param MaskPower float 1 0.05 8 "Mask Power"

// Imported from CyTexture (Effect / Distort): Layered procedural refraction for heat shimmer, underwater distortion, and energy fields.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Phase, float Distortion, float ReferencePixels, float UseAlphaMask, float MaskPower)
{
    float2 p=UV*max(Scale,0.01);float nx=sin(p.y*7.13+Phase)+sin((p.x+p.y)*13.17-Phase*.7)+sin(p.y*29.7+p.x*3.1);float ny=cos(p.x*8.31-Phase)+cos((p.x-p.y)*17.71+Phase*.4)+cos(p.x*31.3-p.y*2.7);float2 warp=float2(nx,ny)/3.0*Distortion/max(CyCanvasSize.xy,1.0)*ReferencePixels;float mask=lerp(1.0,pow(saturate(Texture2DSample(CyInput,CyInputSampler,UV).a),MaskPower),UseAlphaMask);float4 s=Texture2DSample(CyInput,CyInputSampler,UV+warp*mask);return s;
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Phase, Distortion, ReferencePixels, UseAlphaMask, MaskPower);
}
