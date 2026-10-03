//@name Wave Distortion
//@category Traitements
//@input Image
//@halo full
//@param Amount float 0.01 0 0.25 "Amount"
//@param Frequency float 24 0 256 "Frequency"
//@param Phase float 0 -12.566 12.566 "Phase"

// Imported preset (Effect / Distort): Animated-ready sine distortion controlled by phase and frequency.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float Frequency, float Phase)
{
    float2 o=float2(sin(UV.y*Frequency+Phase),cos(UV.x*Frequency+Phase))*Amount; return Texture2DSample(CyInput,CyInputSampler,UV+o);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, Frequency, Phase);
}
