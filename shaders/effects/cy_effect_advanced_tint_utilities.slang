//@name Advanced Tint Utilities
//@category CyTexture Effets
//@input Image
//@halo 0
//@param R float 1.0 0.0 2.0 "R"
//@param G float 1.0 0.0 2.0 "G"
//@param B float 1.0 0.0 2.0 "B"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedTintUtilities.M_CyEffect_AdvancedTintUtilities: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float4 b_n1 = CyTex(0, UV);
    float3 v_n1_RGB = (b_n1).xyz;
    float2 v_f1_n5_o = float2(R, G);
    float3 v_f1_n4_o = float3(v_f1_n5_o, B);
    float3 v_n0_o = (v_n1_RGB * v_f1_n4_o);
    float4 b_n7 = CyTex(0, UV);
    float v_n6_o = (b_n7).w;
    return float4(v_n0_o, v_n6_o);
}
