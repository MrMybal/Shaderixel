//@name Advanced Gradient Grad
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Radius float 0.5 0.0 1.0 "Radius"
//@param Density float 10.0 0.0 20.0 "Density"
//@param Power float 2.0 0.0 4.0 "Power"
//@param Scurve float 1.0 0.0 2.0 "Scurve"
//@param Mul float 1.0 0.0 2.0 "Mul"
//@param View_Mask float 0.0 0.0 1.0 "View Mask"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedGradientGrad.M_CyEffect_AdvancedGradientGrad: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float2 v_f2_n0_o = float2(0.5, 0.5);
    float v_f2_n12_o = distance(UV, v_f2_n0_o);
    float v_f2_n11_o = (v_f2_n12_o * 1.0);
    float v_f2_n10_o = (v_f2_n11_o / Radius);
    float v_f2_f3_n13_o = (v_f2_n10_o * Density);
    float v_f2_f3_n12_o = (v_f2_f3_n13_o * v_f2_f3_n13_o);
    float v_f2_f3_n11_o = ((1.0 > 0.5) ? v_f2_f3_n12_o : v_f2_f3_n13_o);
    float v_f2_f3_n9_o = pow(max(2.718282, 0.0), v_f2_f3_n11_o);
    float v_f2_f3_n7_o = (1.0 / v_f2_f3_n9_o);
    float v_f2_f3_n5_o = ((v_f2_n10_o == 0.0) ? 1.0 : ((v_f2_n10_o > 0.0) ? v_f2_f3_n7_o : 1.0));
    float v_f2_n15_o = (1.0 - v_f2_n10_o);
    float v_f2_f4_n13_o = (v_f2_n15_o * Density);
    float v_f2_f4_n12_o = (v_f2_f4_n13_o * v_f2_f4_n13_o);
    float v_f2_f4_n11_o = ((1.0 > 0.5) ? v_f2_f4_n12_o : v_f2_f4_n13_o);
    float v_f2_f4_n9_o = pow(max(2.718282, 0.0), v_f2_f4_n11_o);
    float v_f2_f4_n7_o = (1.0 / v_f2_f4_n9_o);
    float v_f2_f4_n5_o = ((v_f2_n15_o == 0.0) ? 1.0 : ((v_f2_n15_o > 0.0) ? v_f2_f4_n7_o : 1.0));
    float v_f2_n13_o = (1.0 - v_f2_f4_n5_o);
    float v_f2_n8_o = ((0.0 > 0.5) ? v_f2_f3_n5_o : v_f2_n13_o);
    float v_n6_o = pow(max(v_f2_n8_o, 0.0), Power);
    float v_f1_n9_o = (2.0 * v_n6_o);
    float v_f1_n8_o = clamp(v_f1_n9_o, 0.0, 1.0);
    float v_f1_n7_o = pow(max(v_f1_n8_o, 0.0), Scurve);
    float v_f1_n6_o = (v_f1_n7_o * 0.5);
    float v_f1_n21_o = -0.5;
    float v_f1_n20_o = (v_n6_o + v_f1_n21_o);
    float v_f1_n19_o = (v_f1_n20_o * 2.0);
    float v_f1_n18_o = clamp(v_f1_n19_o, 0.0, 1.0);
    float v_f1_n17_o = (1.0 - v_f1_n18_o);
    float v_f1_n16_o = pow(max(v_f1_n17_o, 0.0), Scurve);
    float v_f1_n15_o = (1.0 - v_f1_n16_o);
    float v_f1_n14_o = (v_f1_n15_o * 0.5);
    float v_f1_n12_o = (0.5 + v_f1_n14_o);
    float v_f1_n25_o = (v_f1_n15_o * 500.0);
    float v_f1_n24_o = clamp(v_f1_n25_o, 0.0, 1.0);
    float v_f1_n5_o = lerp(v_f1_n6_o, v_f1_n12_o, v_f1_n24_o);
    float v_n4_o = (v_f1_n5_o * Mul);
    float v_n3_o = saturate(v_n4_o);
    float3 v_n1_o = (v_n2_RGB * ((float3)(v_n3_o)));
    float3 v_n0_o = lerp(v_n1_o, ((float3)(v_f2_n8_o)), View_Mask);
    float4 b_n16 = CyTex(0, UV);
    float v_n15_o = (b_n16).w;
    return float4(v_n0_o, v_n15_o);
}
