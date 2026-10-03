//@name Advanced Tiling UV
//@category Traitements
//@input Image
//@halo full
//@param Add_X float 0.0 0.0 1.0 "Add X"
//@param Add_Y float 0.0 0.0 1.0 "Add Y"
//@param Tiiling float 1.0 0.0 2.0 "Tiiling"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedTilingUV.M_CyEffect_AdvancedTilingUV: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float v_f2_n2_o = (float4(UV, 0.0, 1.0)).x;
    float v_n3_o = (v_f2_n2_o + Add_X);
    float v_f3_n4_o = (float4(UV, 0.0, 1.0)).y;
    float v_n7_o = (v_f3_n4_o + Add_Y);
    float2 v_f1_n3_o = float2(v_n3_o, v_n7_o);
    float2 v_n1_o = (v_f1_n3_o * ((float2)(Tiiling)));
    float4 b_n0 = CyTex(0, v_n1_o);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n11 = CyTex(0, UV);
    float v_n10_o = (b_n11).w;
    return float4(v_n0_RGB, v_n10_o);
}
