//@name Photo Filter
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Density float 0.25 0 1 "Density"
//@param PreserveLuminosity float 1 0 1 "Preserve Luminosity"
//@param FilterColor color 1 0.55 0.2 1 "Filter Color"

// Imported from CyTexture (Effect / Color & Tone): Photographic warming/cooling filter with density and optional luminance preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Density, float PreserveLuminosity, float3 FilterColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float3 v=lerp(s.rgb,s.rgb*FilterColor.rgb,saturate(Density)); float vl=dot(v,float3(.2126,.7152,.0722)); v+=PreserveLuminosity*(l-vl); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Density, PreserveLuminosity, FilterColor.rgb);
}
