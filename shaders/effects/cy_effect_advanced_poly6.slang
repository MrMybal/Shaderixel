//@name Advanced Poly6
//@category Traitements
//@input Image
//@halo full
//@param K1 float 0.0 0.0 1.0 "K1"
//@param K2 float 0.0 0.0 1.0 "K2"
//@param K3 float 0.0 0.0 1.0 "K3"
//@param K4 float 0.0 0.0 1.0 "K4"
//@param K5 float 0.0 0.0 1.0 "K5"
//@param K6 float 0.0 0.0 1.0 "K6"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedPoly6.M_CyEffect_AdvancedPoly6: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n1(float2 UV, float K1, float K2, float K3, float K4, float K5, float K6)
{
// Poly6 distorsion UV , BC
// Brown-Conrady model , Cubic screen
// by CyberAlien 2023

// UV
// K1
// K2
// K3
// K4
// K5
// K6
float Ratio = (View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y);
float2 CubicUV = float2( UV.x , ((UV.y/Ratio)+0.5) - (0.5/Ratio) );
float R = distance(CubicUV , float2(0.5,0.5) );
float Kcal = (K1*pow(R,2)) + (K2*pow(R,4)) + (K3*pow(R,6)) + (K4*pow(R,8)) + (K5*pow(R,10)) + (K6*pow(R,12));

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
    float2 b_n1 = (Custom_n1(UV, K1, K2, K3, K4, K5, K6)).xy;
    float4 b_n0 = CyTex(0, b_n1);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n10 = CyTex(0, UV);
    float v_n9_o = (b_n10).w;
    return float4(v_n0_RGB, v_n9_o);
}
