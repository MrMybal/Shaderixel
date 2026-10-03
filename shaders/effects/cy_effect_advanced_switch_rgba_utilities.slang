//@name Advanced Switch RGBA Utilities
//@category Traitements
//@input Image
//@halo 0
//@param R___R float 1.0 0.0 2.0 "R - R"
//@param R___G float 0.0 0.0 1.0 "R - G"
//@param R___B float 0.0 0.0 1.0 "R - B"
//@param R___A float 0.0 0.0 1.0 "R - A"
//@param R___Mul float 1.0 0.0 2.0 "R - Mul"
//@param G___R float 0.0 0.0 1.0 "G - R"
//@param G___G float 1.0 0.0 2.0 "G - G"
//@param G___B float 0.0 0.0 1.0 "G - B"
//@param G___A float 0.0 0.0 1.0 "G - A"
//@param G___Mul float 1.0 0.0 2.0 "G - Mul"
//@param B___R float 0.0 0.0 1.0 "B - R"
//@param B___G float 0.0 0.0 1.0 "B - G"
//@param B___B float 1.0 0.0 2.0 "B - B"
//@param B___A float 0.0 0.0 1.0 "B - A"
//@param B___Mul float 1.0 0.0 2.0 "B - Mul"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSwitchRGBAUtilities.M_CyEffect_AdvancedSwitchRGBAUtilities: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float4 b_n6 = CyTex(0, UV);
    float v_f2_n2_o = (b_n6).x;
    float v_n4_o = (v_f2_n2_o * R___R);
    float v_f3_n4_o = (b_n6).y;
    float v_n8_o = (v_f3_n4_o * R___G);
    float v_n3_o = (v_n4_o + v_n8_o);
    float v_f4_n6_o = (b_n6).z;
    float v_n11_o = (v_f4_n6_o * R___B);
    float v_f5_n8_o = (b_n6).w;
    float v_n13_o = (v_f5_n8_o * R___A);
    float v_n10_o = (v_n11_o + v_n13_o);
    float v_n2_o = (v_n3_o + v_n10_o);
    float v_n1_o = (v_n2_o * R___Mul);
    float v_f6_n2_o = (b_n6).x;
    float v_n19_o = (v_f6_n2_o * G___R);
    float v_f7_n4_o = (b_n6).y;
    float v_n22_o = (v_f7_n4_o * G___G);
    float v_n18_o = (v_n19_o + v_n22_o);
    float v_f8_n6_o = (b_n6).z;
    float v_n25_o = (v_f8_n6_o * G___B);
    float v_f9_n8_o = (b_n6).w;
    float v_n27_o = (v_f9_n8_o * G___A);
    float v_n24_o = (v_n25_o + v_n27_o);
    float v_n17_o = (v_n18_o + v_n24_o);
    float v_n16_o = (v_n17_o * G___Mul);
    float2 v_f1_n5_o = float2(v_n1_o, v_n16_o);
    float v_f10_n2_o = (b_n6).x;
    float v_n33_o = (v_f10_n2_o * B___R);
    float v_f11_n4_o = (b_n6).y;
    float v_n36_o = (v_f11_n4_o * B___G);
    float v_n32_o = (v_n33_o + v_n36_o);
    float v_f12_n6_o = (b_n6).z;
    float v_n39_o = (v_f12_n6_o * B___B);
    float v_f13_n8_o = (b_n6).w;
    float v_n41_o = (v_f13_n8_o * B___A);
    float v_n38_o = (v_n39_o + v_n41_o);
    float v_n31_o = (v_n32_o + v_n38_o);
    float v_n30_o = (v_n31_o * B___Mul);
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_n30_o);
    float4 b_n45 = CyTex(0, UV);
    float v_n44_o = (b_n45).w;
    return float4(v_f1_n4_o, v_n44_o);
}
