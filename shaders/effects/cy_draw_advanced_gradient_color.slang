//@name Advanced Gradient Color
//@category CyTexture Dessin
//@generator
//@halo 0
//@param A color 1.0 0.0 0.0 0.0 "A"
//@param A_Mul float 1.0 0.0 2.0 "A Mul"
//@param B color 0.0 1.0 0.0 0.0 "B"
//@param B_Mul float 1.0 0.0 2.0 "B Mul"
//@param C color 0.0 0.0 1.0 0.0 "C"
//@param C_Mul float 1.0 0.0 2.0 "C Mul"
//@param Power float 1.0 0.0 2.0 "Power"
//@param Mul float 1.0 0.0 2.0 "Mul"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedGradientColor.M_CyDraw_AdvancedGradientColor: Native CyTexture advanced Draw preset with live GPU parameters.
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
    float3 v_n4_o = (A).xyz;
    float3 v_n3_o = (v_n4_o * ((float3)(A_Mul)));
    float3 v_n7_o = (B).xyz;
    float3 v_n6_o = (v_n7_o * ((float3)(B_Mul)));
    float v_f2_n2_o = (float4(UV, 0.0, 1.0)).x;
    float v_f1_n10_o = (v_f2_n2_o * 2.0);
    float v_f1_n9_o = clamp(v_f1_n10_o, 0.0, 1.0);
    float3 v_f1_n8_o = lerp(v_n3_o, v_n6_o, v_f1_n9_o);
    float3 v_n10_o = (C).xyz;
    float3 v_n9_o = (v_n10_o * ((float3)(C_Mul)));
    float v_f1_n12_o = (v_f1_n10_o - 1.0);
    float v_f1_n11_o = clamp(v_f1_n12_o, 0.0, 1.0);
    float3 v_f1_n7_o = lerp(v_f1_n8_o, v_n9_o, v_f1_n11_o);
    float3 v_n1_o = pow(max(v_f1_n7_o, 0.0), ((float3)(Power)));
    float3 v_n0_o = (v_n1_o * ((float3)(Mul)));
    float v_n18_o = (float4(v_n0_o, 1.0)).x;
    float v_n19_o = (float4(v_n0_o, 1.0)).y;
    float v_n17_o = max(v_n18_o, v_n19_o);
    float v_n20_o = (float4(v_n0_o, 1.0)).z;
    float v_n16_o = max(v_n17_o, v_n20_o);
    return float4(v_n0_o, v_n16_o);
}
