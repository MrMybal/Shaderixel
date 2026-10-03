//@name Advanced Circular Blur Variant
//@category CyTexture Effets
//@input Image
//@halo full
//@param RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param Blur float 0.960307 0.0 1.920614 "Blur"
//@param BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param Index float 14.0 1.0 128.0 "Index"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedCircularBlur.M_CyEffect_AdvancedCircularBlur: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float RadialStep, float Blur, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// Circular post process
// By CyberAlien 2023

// UV
// RadialStep
// Blur
// Tex
// ID
// Filtered

float PI = 6.28318530;
float RadialStepAngle = PI / RadialStep;
float Angle;
float2 StepVector;
float2 Coord;
float4 Result = Texture2DSample(CyInputTexture, CyInputTextureSampler, UV);

for( int i = 0 ; i<RadialStep ; ++i )
{
	Angle = i * RadialStepAngle;
	StepVector = float2(cos(Angle) , sin(Angle) );
	Coord = ViewportUVToBufferUV( UV + ( StepVector * Blur ) );
	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, Coord);
}
return SXRet( Result / ( RadialStep + 2 ));
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
    float v_n10_o = (0.0 + 0.5);
    float v_n9_o = lerp(1.0, v_n10_o, BlurDitherIntensity);
    float v_n7_o = (Blur * v_n9_o);
    float v_n6_o = (v_n7_o / 100.0);
    float4 b_n3 = (Custom_n3(UV, RadialStep, v_n6_o, Index, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, 1.0);
    float4 b_n22 = CyTex(0, UV);
    float v_n21_o = (b_n22).w;
    float v_n25_o = (float4(v_n0_o, 1.0)).x;
    float v_n26_o = (float4(v_n0_o, 1.0)).y;
    float v_n24_o = max(v_n25_o, v_n26_o);
    float v_n27_o = (float4(v_n0_o, 1.0)).z;
    float v_n23_o = max(v_n24_o, v_n27_o);
    float v_n20_o = max(v_n21_o, v_n23_o);
    return float4(v_n0_o, v_n20_o);
}
