//@name Advanced Split RGBA Utilities
//@category Traitements
//@input Image
//@halo 0
//@param R float 1.0 0.0 2.0 "R"
//@param G float 1.0 0.0 2.0 "G"
//@param B float 1.0 0.0 2.0 "B"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSplitRGBAUtilities.M_CyEffect_AdvancedSplitRGBAUtilities: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float4 b_n2 = CyTex(0, UV);
    float3 v_n2_RGB = (b_n2).xyz;
    float3 v_n1_o = (v_n2_RGB * ((float3)(R)));
    float3 v_n4_o = (v_n2_RGB * ((float3)(G)));
    float4 v_f1_n5_o = float4(v_n1_o, (v_n4_o).x);
    float3 v_n6_o = (v_n2_RGB * ((float3)(B)));
    float4 b_n9 = CyTex(0, UV);
    float v_n8_o = (b_n9).w;
    return float4((v_f1_n5_o).xyz, v_n8_o);
}
