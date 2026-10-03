//@name Advanced Circular No Gradient
//@category Dessin
//@generator
//@halo full
//@param Radius float 0.5 0.0 1.0 "Radius"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedCircularNoGradient.M_CyDraw_AdvancedCircularNoGradient: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_n0(float2 UV, float Radius)
{
float Result = 0 ;
if ( distance(UV,float2(0.5,0.5)) < Radius )
{
	Result = 1;
}
return SXRet( Result);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float3 b_n0 = (Custom_n0(UV, Radius)).xyz;
    float v_n5_o = (float4(b_n0, 1.0)).x;
    float v_n6_o = (float4(b_n0, 1.0)).y;
    float v_n4_o = max(v_n5_o, v_n6_o);
    float v_n7_o = (float4(b_n0, 1.0)).z;
    float v_n3_o = max(v_n4_o, v_n7_o);
    return float4(b_n0, v_n3_o);
}
