//@name Advanced Poly3lensflare
//@category Traitements
//@input Image
//@halo full
//@param K1 float 0.0 0.0 1.0 "K1"
//@param K2 float 0.0 0.0 1.0 "K2"
//@param K3 float 0.0 0.0 1.0 "K3"
//@param size float 0.0 0.0 1.0 "size"
//@param RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param Center color 0.5 0.5 0.0 0.0 "Center"
//@param Blur float 0.1 0.0 1.0 "Blur"
//@param BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param Index float 14.0 1.0 128.0 "Index"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedPoly3lensflare.M_CyEffect_AdvancedPoly3lensflare: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n2(float2 UV, float K1, float K2, float K3)
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

float4 Custom_n8(float2 UV, float RadialStep, float2 Center, float Blur, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
//Radial blur , Post process
//By CyberAlien , 2023


//RadialStep , number of Step
//UV , input UV coord
//Blur , Blur
//Center
//ID , SceneTextureLookup Index
//Filtered , SceneTextureLookupFiltered

float RadialStepAngle = 6.28318530 / RadialStep ;
float4 Result = 0 ;
float2 Dir = Center - UV;


for(int i = 0; i<RadialStep; i++)
{
    float2 pos = ViewportUVToBufferUV( UV + Dir * ( i * RadialStepAngle ) * Blur );
	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, pos);
}
return SXRet( Result / RadialStep );
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 b_n2 = (Custom_n2(UV, K1, K2, K3)).xy;
    float4 b_n1 = CyTex(0, b_n2);
    float3 v_n1_RGB = (b_n1).xyz;
    float2 v_f1_n7_o = (b_n2 / ((float2)(size)));
    float2 v_f1_n6_o = (v_f1_n7_o + ((float2)(0.5)));
    float v_f1_n9_o = (0.5 / size);
    float2 v_f1_n5_o = (v_f1_n6_o - ((float2)(v_f1_n9_o)));
    float v_n13_R = (Center).x;
    float2 v_f2_n3_o = float2(v_n13_R, v_n13_R);
    float v_n17_o = (0.0 + 0.5);
    float v_n16_o = lerp(1.0, v_n17_o, BlurDitherIntensity);
    float v_n14_o = (Blur * v_n16_o);
    float4 b_n8 = (Custom_n8(v_f1_n5_o, RadialStep, v_f2_n3_o, v_n14_o, Index, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n7_o = (b_n8).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n7_o, 1.0);
    float4 b_n29 = CyTex(0, UV);
    float v_n28_o = (b_n29).w;
    float v_n32_o = (float4(v_n0_o, 1.0)).x;
    float v_n33_o = (float4(v_n0_o, 1.0)).y;
    float v_n31_o = max(v_n32_o, v_n33_o);
    float v_n34_o = (float4(v_n0_o, 1.0)).z;
    float v_n30_o = max(v_n31_o, v_n34_o);
    float v_n27_o = max(v_n28_o, v_n30_o);
    return float4(v_n0_o, v_n27_o);
}
