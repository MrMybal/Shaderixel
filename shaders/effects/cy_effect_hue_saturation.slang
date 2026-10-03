//@name Hue Saturation
//@category Traitements
//@input Image
//@halo 0
//@param Hue float 0 -1 1 "Hue"
//@param Saturation float 1 0 4 "Saturation"
//@param Value float 1 0 8 "Value"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): YIQ hue rotation with saturation and value controls.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Hue, float Saturation, float Value, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float y=dot(s.rgb,float3(.299,.587,.114)); float i=dot(s.rgb,float3(.596,-.274,-.322)); float q=dot(s.rgb,float3(.211,-.523,.312)); float a=Hue*6.2831853; float ir=i*cos(a)-q*sin(a); float qr=i*sin(a)+q*cos(a); float3 h=float3(y+0.956*ir+0.621*qr,y-0.272*ir-0.647*qr,y-1.106*ir+1.703*qr); float l=dot(h,float3(.2126,.7152,.0722)); h=lerp(l.xxx,h,Saturation)*Value; return float4(lerp(s.rgb,h,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Hue, Saturation, Value, Strength);
}
