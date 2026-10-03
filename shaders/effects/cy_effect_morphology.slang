//@name Morphology
//@category CyTexture Effets
//@input Image
//@halo full
//@param Radius float 3 0.25 128 "Radius"
//@param Mode float 1 0 1 "Mode"
//@param UseAlpha float 1 0 1 "Use Alpha"
//@param OutputMask float 0 0 1 "Output Mask"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Detail): GPU dilate/erode filter for alpha, luminance, masks, and cleanup workflows.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Mode, float UseAlpha, float OutputMask, float Strength)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float best=lerp(1.0,0.0,step(0.5,Mode));[unroll]for(int i=0;i<24;i++){float a=i*6.283185/24.0;float4 s=Texture2DSample(CyInput,CyInputSampler,UV+float2(cos(a),sin(a))*t);float v=lerp(dot(s.rgb,float3(.2126,.7152,.0722)),s.a,saturate(UseAlpha));best=lerp(min(best,v),max(best,v),step(.5,Mode));}float original=lerp(dot(c.rgb,float3(.2126,.7152,.0722)),c.a,saturate(UseAlpha));float result=lerp(original,best,saturate(Strength));float3 rgb=lerp(c.rgb,result.xxx,saturate(OutputMask));float alpha=lerp(c.a,result,saturate(UseAlpha));return float4(rgb,alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Mode, UseAlpha, OutputMask, Strength);
}
