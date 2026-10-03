//@name Advanced Luma Contrast Utilities
//@category Traitements
//@input Image
//@halo 0
//@param Contrast color 0.0 0.0 0.0 0.0 "Contrast"
//@param Luminosity color 1.0 1.0 1.0 0.0 "Luminosity"
//@param Mix float 1.0 0.0 2.0 "Mix"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedLumaContrastUtilities.M_CyEffect_AdvancedLumaContrastUtilities: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float v_n8_R = (Contrast).x;
    float v_n7_o = (v_n8_R + v_n8_R);
    float v_f2_n8_o = (0.0 - v_n7_o);
    float v_f2_n9_o = (v_n7_o + 1.0);
    float v_f3_n2_o = (float4(v_n1_RGB, 1.0)).x;
    float v_f2_n7_o = lerp(v_f2_n8_o, v_f2_n9_o, v_f3_n2_o);
    float v_f2_n6_o = clamp(v_f2_n7_o, 0.0, 1.0);
    float v_f2_n5_o = (((float4)(v_f2_n6_o))).x;
    float v_n10_R = (Luminosity).x;
    float v_n9_o = (v_n10_R + v_n10_R);
    float v_n4_o = (v_f2_n5_o * v_n9_o);
    float v_n8_G = (Contrast).y;
    float v_n13_o = (v_n8_G + v_n8_G);
    float v_f4_n8_o = (0.0 - v_n13_o);
    float v_f4_n9_o = (v_n13_o + 1.0);
    float v_f5_n4_o = (float4(v_n1_RGB, 1.0)).y;
    float v_f4_n7_o = lerp(v_f4_n8_o, v_f4_n9_o, v_f5_n4_o);
    float v_f4_n6_o = clamp(v_f4_n7_o, 0.0, 1.0);
    float v_f4_n5_o = (((float4)(v_f4_n6_o))).x;
    float v_n10_G = (Luminosity).y;
    float v_n14_o = (v_n10_G + v_n10_G);
    float v_n11_o = (v_f4_n5_o * v_n14_o);
    float2 v_f1_n5_o = float2(v_n4_o, v_n11_o);
    float v_n8_B = (Contrast).z;
    float v_n17_o = (v_n8_B + v_n8_B);
    float v_f6_n8_o = (0.0 - v_n17_o);
    float v_f6_n9_o = (v_n17_o + 1.0);
    float v_f7_n6_o = (float4(v_n1_RGB, 1.0)).z;
    float v_f6_n7_o = lerp(v_f6_n8_o, v_f6_n9_o, v_f7_n6_o);
    float v_f6_n6_o = clamp(v_f6_n7_o, 0.0, 1.0);
    float v_f6_n5_o = (((float4)(v_f6_n6_o))).x;
    float v_n10_B = (Luminosity).z;
    float v_n18_o = (v_n10_B + v_n10_B);
    float v_n15_o = (v_f6_n5_o * v_n18_o);
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_n15_o);
    float3 v_n2_o = (float4(v_f1_n4_o, 1.0)).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, Mix);
    float4 b_n21 = CyTex(0, UV);
    float v_n20_o = (b_n21).w;
    return float4(v_n0_o, v_n20_o);
}
