//@name HDR Toning
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Exposure float 0 -16 16 "Exposure"
//@param Detail float 1 0 8 "Detail"
//@param Pivot float 0.5 0 4 "Pivot"
//@param Shoulder float 1 0.001 16 "Shoulder"
//@param Saturation float 1 0 4 "Saturation"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): HDR exposure, detail contrast, shoulder compression, and saturation toning.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Exposure, float Detail, float Pivot, float Shoulder, float Saturation, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 x=max(s.rgb*exp2(Exposure),0.0); float l=dot(x,float3(.2126,.7152,.0722)); x=(x-Pivot)*Detail+Pivot; x=x/(1.0+x/max(Shoulder,0.001)); float nl=dot(x,float3(.2126,.7152,.0722)); x=lerp(nl.xxx,x,Saturation); return float4(lerp(s.rgb,x,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Exposure, Detail, Pivot, Shoulder, Saturation, Strength);
}
