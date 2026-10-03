//@name Advanced Blur Poly3
//@category CyTexture Effets
//@input Image
//@halo full
//@param Scale float 1.0 0.0 2.0 "Scale"
//@param K1 float 0.0 0.0 1.0 "K1"
//@param K2 float 0.0 0.0 1.0 "K2"
//@param K3 float 0.0 0.0 1.0 "K3"
//@param Step float 16.0 1.0 128.0 "Step"
//@param Blur float 0.0 0.0 1.0 "Blur"
//@param BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param Type float 0.0 0.0 1.0 "Type"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedBlurPoly3.M_CyEffect_AdvancedBlurPoly3: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float K1, float K2, float K3, float Step, float Blur, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// Poly3 distorsion Blur , BC
// Brown-Conrady model , Cubic screen
// by CyberAlien 2023

// UV
// K1
// K2
// K3
// Step
// Blur
// ID
// Filtered
float Ratio = (View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y);
float2 CubicUV = float2( UV.x , ((UV.y/Ratio)+0.5) - (0.5/Ratio) );
float R = distance(CubicUV , float2(0.5,0.5) );
float Kcal = (float )0;
float4 Result = (float4 )0;
float2 Duv = (float2 )0;

for( int i = 0 ; i<Step ; ++i )
{
	Kcal = ((lerp( K1 , 0 , (i/Step)*Blur ))*pow(R,2)) + ((lerp( K2 , 0 , (i/Step)*Blur ))*pow(R,4)) + ((lerp( K3 , 0 , (i/Step)*Blur ))*pow(R,6));
	Duv = UV - ( (UV - 0.5 ) * Kcal );
	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, Duv);
}
return SXRet( Result / ( Step  ));
}

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
    float2 v_f1_n7_o = (UV / ((float2)(Scale)));
    float2 v_f1_n6_o = (v_f1_n7_o + ((float2)(0.5)));
    float v_f1_n9_o = (0.5 / Scale);
    float2 v_f1_n5_o = (v_f1_n6_o - ((float2)(v_f1_n9_o)));
    float v_n14_o = (0.0 + 0.5);
    float v_n13_o = lerp(1.0, v_n14_o, BlurDitherIntensity);
    float v_n11_o = (Blur * v_n13_o);
    float4 b_n3 = (Custom_n3(v_f1_n5_o, K1, K2, K3, Step, v_n11_o, 14.0, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, Type);
    float4 b_n26 = CyTex(0, UV);
    float v_n25_o = (b_n26).w;
    float v_n29_o = (float4(v_n0_o, 1.0)).x;
    float v_n30_o = (float4(v_n0_o, 1.0)).y;
    float v_n28_o = max(v_n29_o, v_n30_o);
    float v_n31_o = (float4(v_n0_o, 1.0)).z;
    float v_n27_o = max(v_n28_o, v_n31_o);
    float v_n24_o = max(v_n25_o, v_n27_o);
    return float4(v_n0_o, v_n24_o);
}
