//@name Color Grading
//@category CyTexture Effets
//@input Image
//@halo 0
//@param LiftLuma float 0 -2 2 "Lift Luma"
//@param GammaLuma float 0 -0.95 4 "Gamma Luma"
//@param GainLuma float 0 -1 8 "Gain Luma"
//@param Temperature float 0 -1 1 "Temperature"
//@param Tint float 0 -1 1 "Tint"
//@param Saturation float 1 0 4 "Saturation"
//@param Contrast float 1 0 4 "Contrast"
//@param ContrastPivot float 0.5 0 2 "Contrast Pivot"
//@param Strength float 1 0 1 "Strength"
//@param LiftWheel color 0 0 1 1 "Lift Wheel"
//@param GammaWheel color 0 0 1 1 "Gamma Wheel"
//@param GainWheel color 0 0 1 1 "Gain Wheel"

// Imported from CyTexture (Effect / Color & Tone): Professional lift, gamma, and gain color wheels with temperature, tint, saturation, and contrast.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LiftLuma, float GammaLuma, float GainLuma, float Temperature, float Tint, float Saturation, float Contrast, float ContrastPivot, float Strength, float3 LiftWheel, float3 GammaWheel, float3 GainWheel)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 lp=abs(frac(LiftWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 gp=abs(frac(GammaWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 ap=abs(frac(GainWheel.r/360.0+float3(0,0.6666667,0.3333333))*6.0-3.0); float3 lc=lerp(1.0.xxx,saturate(lp-1.0),saturate(LiftWheel.g)); float3 gc=lerp(1.0.xxx,saturate(gp-1.0),saturate(GammaWheel.g)); float3 ac=lerp(1.0.xxx,saturate(ap-1.0),saturate(GainWheel.g)); lc-=dot(lc,float3(.333333,.333333,.333333)); gc-=dot(gc,float3(.333333,.333333,.333333)); ac-=dot(ac,float3(.333333,.333333,.333333)); float3 v=max(s.rgb+LiftLuma+lc*0.25,0.0); v=pow(v,1.0/max(1.0+GammaLuma+gc*0.35,0.05)); v*=max(1.0+GainLuma+ac*0.5,0.0); v*=max(float3(1.0+Temperature*.12-Tint*.04,1.0+Tint*.08,1.0-Temperature*.12-Tint*.04),.001); float l=dot(v,float3(.2126,.7152,.0722)); v=lerp(l.xxx,v,Saturation); v=(v-ContrastPivot)*Contrast+ContrastPivot; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LiftLuma, GammaLuma, GainLuma, Temperature, Tint, Saturation, Contrast, ContrastPivot, Strength, LiftWheel.rgb, GammaWheel.rgb, GainWheel.rgb);
}
