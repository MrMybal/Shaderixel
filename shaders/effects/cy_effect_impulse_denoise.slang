//@name Impulse Denoise
//@category Traitements
//@input Image
//@halo full
//@param Radius float 2 0.25 32 "Radius"
//@param ColorTolerance float 0.12 0.001 4 "Color Tolerance"
//@param Threshold float 0.08 0 2 "Threshold"
//@param Softness float 0.05 0.0001 1 "Softness"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Blur & Denoise): Edge-preserving impulse and salt-and-pepper denoiser with adaptive color rejection.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float ColorTolerance, float Threshold, float Softness, float Strength)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float3 sum=0;float wsum=0;[unroll]for(int i=0;i<16;i++){float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*t*sqrt((i+.5)/16.0);float3 s=Texture2DSample(CyInput,CyInputSampler,UV+o).rgb;float delta=length(s-c.rgb);float w=exp(-delta*delta/max(ColorTolerance*ColorTolerance,.000001));sum+=s*w;wsum+=w;}float3 filtered=sum/max(wsum,.0001);float centerError=length(c.rgb-filtered);float reject=smoothstep(Threshold,Threshold+Softness,centerError);return float4(lerp(c.rgb,filtered,reject*saturate(Strength)),c.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, ColorTolerance, Threshold, Softness, Strength);
}
