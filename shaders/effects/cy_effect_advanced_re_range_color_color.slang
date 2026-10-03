//@name Advanced Re Range Color Color
//@category CyTexture Effets
//@input Image
//@halo 0
//@param R_Low float 0.0 0.0 1.0 "R Low"
//@param R_High float 1.0 0.0 2.0 "R High"
//@param R_Target_Height float 1.0 0.0 2.0 "R Target Height"
//@param R_Target_Low float 0.0 0.0 1.0 "R Target Low"
//@param G_Low float 0.0 0.0 1.0 "G Low"
//@param G_High float 1.0 0.0 2.0 "G High"
//@param G_Target_Height float 1.0 0.0 2.0 "G Target Height"
//@param G_Target_Low float 0.0 0.0 1.0 "G Target Low"
//@param B_Low float 0.0 0.0 1.0 "B Low"
//@param B_High float 1.0 0.0 2.0 "B High"
//@param B_Target_Height float 1.0 0.0 2.0 "B Target Height"
//@param B_Target_Low float 0.0 0.0 1.0 "B Target Low"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedReRangeColorColor.M_CyEffect_AdvancedReRangeColorColor: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float v_f2_n9_o = (v_f3_n2_o - R_Low);
    float v_f2_n10_o = (R_High - R_Low);
    float v_f2_n8_o = (v_f2_n9_o / v_f2_n10_o);
    float v_f2_n11_o = (R_Target_Height - R_Target_Low);
    float v_f2_n7_o = (v_f2_n8_o * v_f2_n11_o);
    float v_f2_n6_o = (v_f2_n7_o + R_Target_Low);
    float v_f5_n4_o = (float4(v_n3_RGB, 1.0)).y;
    float v_f4_n9_o = (v_f5_n4_o - G_Low);
    float v_f4_n10_o = (G_High - G_Low);
    float v_f4_n8_o = (v_f4_n9_o / v_f4_n10_o);
    float v_f4_n11_o = (G_Target_Height - G_Target_Low);
    float v_f4_n7_o = (v_f4_n8_o * v_f4_n11_o);
    float v_f4_n6_o = (v_f4_n7_o + G_Target_Low);
    float2 v_f1_n5_o = float2(v_f2_n6_o, v_f4_n6_o);
    float v_f7_n6_o = (float4(v_n3_RGB, 1.0)).z;
    float v_f6_n9_o = (v_f7_n6_o - B_Low);
    float v_f6_n10_o = (B_High - B_Low);
    float v_f6_n8_o = (v_f6_n9_o / v_f6_n10_o);
    float v_f6_n11_o = (B_Target_Height - B_Target_Low);
    float v_f6_n7_o = (v_f6_n8_o * v_f6_n11_o);
    float v_f6_n6_o = (v_f6_n7_o + B_Target_Low);
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_f6_n6_o);
    float4 b_n19 = CyTex(0, UV);
    float v_n18_o = (b_n19).w;
    return float4(v_f1_n4_o, v_n18_o);
}
