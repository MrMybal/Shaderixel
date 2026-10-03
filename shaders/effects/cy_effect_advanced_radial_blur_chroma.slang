//@name Advanced Radial Blur Chroma
//@category CyTexture Effets
//@input Image
//@halo full
//@param RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param Center color 0.0 0.0 0.0 0.0 "Center"
//@param Blur float 0.1 0.0 1.0 "Blur"
//@param BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param Index float 14.0 1.0 128.0 "Index"
//@param ChromaOffset float 16.0 0.0 32.0 "Chroma Offset"
//@param ChromaChannel color 0.0 0.0 0.0 0.0 "Chroma Channel"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRadialBlurChroma.M_CyEffect_AdvancedRadialBlurChroma: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float RadialStep, float2 Center, float Blur, float ID, float Filtered, float ChromaOffset, float3 ChromaChannel, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
//Radial blur chroma , Post process
//By CyberAlien , 2023


//RadialStep , number of Step
//UV , input UV coord
//Blur , Blur
//Center
//ID , SceneTextureLookup Index
//Filtered , SceneTextureLookupFiltered
// ChromaOffset
// ChromaChannel

float RadialStepAngle = 6.28318530 / RadialStep ;
float4 Result = 0 ;
float2 Dir = Center - UV;
float2 pos = (float2 )0;


for(int i = 0; i<RadialStep; i++)
{
    pos = ViewportUVToBufferUV( UV + Dir * ( i * RadialStepAngle ) * Blur * ( 1 + ChromaOffset * ChromaChannel.r ) );
	Result.r += Texture2DSample(CyInputTexture, CyInputTextureSampler, pos).r;
	pos = ViewportUVToBufferUV( UV + Dir * ( i * RadialStepAngle ) * Blur * ( 1 + ChromaOffset * ChromaChannel.g ) );
	Result.g += Texture2DSample(CyInputTexture, CyInputTextureSampler, pos).g;
	pos = ViewportUVToBufferUV( UV + Dir * ( i * RadialStepAngle ) * Blur * ( 1 + ChromaOffset * ChromaChannel.b ) );
	Result.b += Texture2DSample(CyInputTexture, CyInputTextureSampler, pos).b;
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
    float4 b_n1 = CyTex(0, UV);
    float3 v_n1_RGB = (b_n1).xyz;
    float v_n7_R = (Center).x;
    float2 v_f1_n3_o = float2(v_n7_R, v_n7_R);
    float v_n11_o = (0.0 + 0.5);
    float v_n10_o = lerp(1.0, v_n11_o, BlurDitherIntensity);
    float v_n8_o = (Blur * v_n10_o);
    float v_n18_R = (ChromaChannel).x;
    float2 v_f2_n5_o = float2(v_n18_R, v_n18_R);
    float3 v_f2_n4_o = float3(v_f2_n5_o, v_n18_R);
    float4 b_n3 = (Custom_n3(UV, RadialStep, v_f1_n3_o, v_n8_o, Index, 1.0, ChromaOffset, v_f2_n4_o, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, 1.0);
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
