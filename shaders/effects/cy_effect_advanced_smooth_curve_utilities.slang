//@name Advanced Smooth Curve Utilities
//@category Traitements
//@input Image
//@halo 0
//@param T0 float 1.0 0.0 2.0 "T0"
//@param T1 float 1.0 0.0 2.0 "T1"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSmoothCurveUtilities.M_CyEffect_AdvancedSmoothCurveUtilities: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float4 b_n3 = CyTex(0, UV);
    float3 v_n3_RGB = (b_n3).xyz;
    float v_f3_n2_o = (float4(v_n3_RGB, 1.0)).x;
    float v_f2_n7_o = (v_f3_n2_o * T0);
    float v_f2_n13_o = (T0 + T1);
    float v_f2_n12_o = (v_f2_n13_o - 2.0);
    float v_f2_n11_o = (v_f3_n2_o * v_f2_n12_o);
    float v_f2_n10_o = (v_f3_n2_o * v_f2_n11_o);
    float v_f2_n9_o = (v_f3_n2_o * v_f2_n10_o);
    float v_f2_n19_o = (3.0 - T1);
    float v_f2_n18_o = (v_f2_n19_o - 2.0);
    float v_f2_n17_o = (v_f2_n18_o * T0);
    float v_f2_n16_o = (v_f3_n2_o * v_f2_n17_o);
    float v_f2_n15_o = (v_f3_n2_o * v_f2_n16_o);
    float v_f2_n8_o = (v_f2_n9_o + v_f2_n15_o);
    float v_f2_n6_o = (v_f2_n7_o + v_f2_n8_o);
    float v_f5_n4_o = (float4(v_n3_RGB, 1.0)).y;
    float v_f4_n7_o = (v_f5_n4_o * T0);
    float v_f4_n13_o = (T0 + T1);
    float v_f4_n12_o = (v_f4_n13_o - 2.0);
    float v_f4_n11_o = (v_f5_n4_o * v_f4_n12_o);
    float v_f4_n10_o = (v_f5_n4_o * v_f4_n11_o);
    float v_f4_n9_o = (v_f5_n4_o * v_f4_n10_o);
    float v_f4_n19_o = (3.0 - T1);
    float v_f4_n18_o = (v_f4_n19_o - 2.0);
    float v_f4_n17_o = (v_f4_n18_o * T0);
    float v_f4_n16_o = (v_f5_n4_o * v_f4_n17_o);
    float v_f4_n15_o = (v_f5_n4_o * v_f4_n16_o);
    float v_f4_n8_o = (v_f4_n9_o + v_f4_n15_o);
    float v_f4_n6_o = (v_f4_n7_o + v_f4_n8_o);
    float2 v_f1_n5_o = float2(v_f2_n6_o, v_f4_n6_o);
    float v_f7_n6_o = (float4(v_n3_RGB, 1.0)).z;
    float v_f6_n7_o = (v_f7_n6_o * T0);
    float v_f6_n13_o = (T0 + T1);
    float v_f6_n12_o = (v_f6_n13_o - 2.0);
    float v_f6_n11_o = (v_f7_n6_o * v_f6_n12_o);
    float v_f6_n10_o = (v_f7_n6_o * v_f6_n11_o);
    float v_f6_n9_o = (v_f7_n6_o * v_f6_n10_o);
    float v_f6_n19_o = (3.0 - T1);
    float v_f6_n18_o = (v_f6_n19_o - 2.0);
    float v_f6_n17_o = (v_f6_n18_o * T0);
    float v_f6_n16_o = (v_f7_n6_o * v_f6_n17_o);
    float v_f6_n15_o = (v_f7_n6_o * v_f6_n16_o);
    float v_f6_n8_o = (v_f6_n9_o + v_f6_n15_o);
    float v_f6_n6_o = (v_f6_n7_o + v_f6_n8_o);
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_f6_n6_o);
    float4 b_n9 = CyTex(0, UV);
    float v_n8_o = (b_n9).w;
    return float4(v_f1_n4_o, v_n8_o);
}
