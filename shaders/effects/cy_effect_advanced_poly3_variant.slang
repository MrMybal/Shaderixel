//@name Advanced Poly3 Variant
//@category Traitements
//@input Image
//@halo full
//@param Scale float 1.370685 0.0 2.74137 "Scale"
//@param K1 float 0.204132 0.0 1.0 "K1"
//@param K2 float -0.974737 -0.974737 1.0 "K2"
//@param K3 float -4.582661 -4.582661 1.0 "K3"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedPoly3.M_CyEffect_AdvancedPoly3: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n1(float2 UV, float K1, float K2, float K3)
{
// Poly3 distorsion UV 
// Cubic screen
// by CyberAlien 2023

// UV
// K1
// K2
// K3
float Ratio = (View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y);
float2 CubicUV = float2( UV.x , ((UV.y/Ratio)+0.5) - (0.5/Ratio) );
float R = distance(CubicUV , float2(0.5,0.5) );
float Kcal = (K1*R) + (K2*pow(R,2)) + (K3*pow(R,3));

return SXRet( UV - ( (UV - 0.5 ) * Kcal ) );
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_f1_n7_o = (UV / ((float2)(Scale)));
    float2 v_f1_n6_o = (v_f1_n7_o + ((float2)(0.5)));
    float v_f1_n9_o = (0.5 / Scale);
    float2 v_f1_n5_o = (v_f1_n6_o - ((float2)(v_f1_n9_o)));
    float2 b_n1 = (Custom_n1(v_f1_n5_o, K1, K2, K3)).xy;
    float4 b_n0 = CyTex(0, b_n1);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n9 = CyTex(0, UV);
    float v_n8_o = (b_n9).w;
    return float4(v_n0_RGB, v_n8_o);
}
