//@name Advanced Cubic UV
//@category CyTexture Effets
//@input Param
//@input Image
//@halo full

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedCubicUV.M_CyEffect_AdvancedCubicUV: Native CyTexture advanced Effect preset with live GPU parameters.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }  // unconnected = white, as Unreal
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
#define Texture2DSampleLevel(T, S, UVS, LOD) CyTex(T, UVS)
static EffectContext ctx;  // for helper functions (noise) reading the document size
// Unreal view values and helpers used by Custom nodes (a document pixel = a screen pixel).
struct SXView { float4 ViewSizeAndInvSize; float4 BufferSizeAndInvSize; float4 TemporalAAParams; };
static SXView View;
static SXView ResolvedView;
float2 ViewportUVToBufferUV(float2 uv) { return uv; }
float2 BufferUVToViewportUV(float2 uv) { return uv; }
float4 SXRet(float v) { return float4(v, v, v, 1.0); }
float4 SXRet(float2 v) { return float4(v, 0.0, 1.0); }
float4 SXRet(float3 v) { return float4(v, 1.0); }
float4 SXRet(float4 v) { return v; }

float4 Custom_n1(float2 UV)
{
// Cubic view for UV coord , Post process
// By CyberAlien 2023

// UV

return SXRet( float2( UV.x , (View.ViewSizeAndInvSize.y/View.ViewSizeAndInvSize.x) * UV.y ));
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 b_n1 = (Custom_n1(UV)).xy;
    float4 b_n0 = CyTex(0, b_n1);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n4 = CyTex(1, UV);
    float v_n3_o = (b_n4).w;
    return float4(v_n0_RGB, v_n3_o);
}
