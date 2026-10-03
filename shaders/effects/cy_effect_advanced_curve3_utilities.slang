//@name Advanced Curve3 Utilities
//@category Traitements
//@input Image
//@halo 0
//@param Low float 0.0 0.0 1.0 "Low"
//@param Mid float 0.5 0.0 1.0 "Mid"
//@param High float 1.0 0.0 2.0 "High"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedCurve3Utilities.M_CyEffect_AdvancedCurve3Utilities: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float4 b_n7 = CyTex(0, UV);
    float3 v_n7_RGB = (b_n7).xyz;
    float v_f2_n2_o = (float4(v_n7_RGB, 1.0)).x;
    float v_n5_o = (v_f2_n2_o * 1.0);
    float v_n2_o = lerp(Low, Mid, v_n5_o);
    float v_n8_o = lerp(Mid, High, v_n5_o);
    float v_n1_o = lerp(v_n2_o, v_n8_o, v_n5_o);
    float v_f3_n4_o = (float4(v_n7_RGB, 1.0)).y;
    float v_n14_o = (v_f3_n4_o * 1.0);
    float v_n11_o = lerp(Low, Mid, v_n14_o);
    float v_n15_o = lerp(Mid, High, v_n14_o);
    float v_n10_o = lerp(v_n11_o, v_n15_o, v_n14_o);
    float2 v_f1_n5_o = float2(v_n1_o, v_n10_o);
    float v_f4_n6_o = (float4(v_n7_RGB, 1.0)).z;
    float v_n21_o = (v_f4_n6_o * 1.0);
    float v_n18_o = lerp(Low, Mid, v_n21_o);
    float v_n22_o = lerp(Mid, High, v_n21_o);
    float v_n17_o = lerp(v_n18_o, v_n22_o, v_n21_o);
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_n17_o);
    float4 b_n25 = CyTex(0, UV);
    float v_n24_o = (b_n25).w;
    return float4(v_f1_n4_o, v_n24_o);
}
