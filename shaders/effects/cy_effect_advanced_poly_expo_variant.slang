//@name Advanced Poly Expo Variant
//@category Traitements
//@input Image
//@halo full
//@param K1 float -48.84918 -48.84918 1.0 "K1"
//@param Scale float 3.051865 0.0 6.10373 "Scale"
//@param p_min float 0.0 0.0 1.0 "min"
//@param p_max float 1.0 0.0 1.0 "max"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedPolyExpo.M_CyEffect_AdvancedPolyExpo: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float K)
{
// PolyExpo distorsion UV , BC
// Brown-Conrady model , Cubic screen
// by CyberAlien 2023

// UV
// K
// 2,71828

float Exp = 2.71828 ;
float K1 = K ;
float K2 = K ;
float K3 = K ;

float Ratio = (View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y);
float2 CubicUV = float2( UV.x , ((UV.y/Ratio)+0.5) - (0.5/Ratio) );
float R = pow( Exp , distance(CubicUV , float2(0.5,0.5) ) );
float Kcal = (K1*pow(R,2)) + (K2*pow(R,4)) + (K3*pow(R,6)) ;

return SXRet( UV - ( (UV - 0.5 ) * Kcal ) );
}

float4 Custom_n8(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float v_n5_o = (K1 / 1000.0);
    float2 b_n3 = (Custom_n3(UV, v_n5_o)).xy;
    float2 v_f1_n7_o = (b_n3 / ((float2)(Scale)));
    float2 v_f1_n6_o = (v_f1_n7_o + ((float2)(0.5)));
    float v_f1_n9_o = (0.5 / Scale);
    float2 v_f1_n5_o = (v_f1_n6_o - ((float2)(v_f1_n9_o)));
    float4 b_n1 = CyTex(0, v_f1_n5_o);
    float3 v_n1_RGB = (b_n1).xyz;
    float b_n8 = (Custom_n8(v_f1_n5_o, p_min, p_max)).x;
    float3 v_n0_o = (v_n1_RGB * ((float3)(b_n8)));
    float4 b_n12 = CyTex(0, UV);
    float v_n11_o = (b_n12).w;
    return float4(v_n0_o, v_n11_o);
}
