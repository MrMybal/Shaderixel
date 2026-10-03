//@name Advanced Rotate Blur Chroma P Pm1
//@category Traitements
//@input Image
//@halo full
//@param Step float 16.0 1.0 128.0 "Step"
//@param Blur float 0.0 0.0 1.0 "Blur"
//@param BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param chroma float 0.2 0.0 1.0 "chroma"
//@param chromaChannel color 0.8 1.0 1.2 0.0 "chroma Channel"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRotateBlurChromaPPm1.M_CyEffect_AdvancedRotateBlurChromaPPm1: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float Step, float Blur, float2 Center, float ID, float Filtered, float ChromaOffset, float3 ChromaChannel, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// Rotate blur Chroma
// By CyberAlien

// UV
// Step
// Blur
// Center
// ID
// Filtered
// ChromaOffset
// ChromaChannel

float PI = 6.28318548;
float2 BaseUV = ( Center * -1 ) + UV;
float2 Rot = (float2 )0;
float Rotate = (float )0;
float4 Result = (float4 )0;

for( int i = 0 ; i<Step ; ++i )
{
	Rotate = (Blur/100000) * ( ((Step/2)*-1)+i ) * ( (ChromaOffset*ChromaChannel.r) + 1);
	Rot = float2( dot( BaseUV , float2(cos((Rotate*PI)),sin((Rotate*PI))*-1) ) , dot( BaseUV , float2( sin((Rotate*PI)),cos((Rotate*PI))) ) );
	Result.r += Texture2DSample(CyInputTexture, CyInputTextureSampler, Center + Rot).r;
	
	Rotate = (Blur/100000) * ( ((Step/2)*-1)+i ) * ( (ChromaOffset*ChromaChannel.g) + 1);
	Rot = float2( dot( BaseUV , float2(cos((Rotate*PI)),sin((Rotate*PI))*-1) ) , dot( BaseUV , float2( sin((Rotate*PI)),cos((Rotate*PI))) ) );
	Result.g += Texture2DSample(CyInputTexture, CyInputTextureSampler, Center + Rot).g;
	
	Rotate = (Blur/100000) * ( ((Step/2)*-1)+i ) * ( (ChromaOffset*ChromaChannel.b) + 1);
	Rot = float2( dot( BaseUV , float2(cos((Rotate*PI)),sin((Rotate*PI))*-1) ) , dot( BaseUV , float2( sin((Rotate*PI)),cos((Rotate*PI))) ) );
	Result.b += Texture2DSample(CyInputTexture, CyInputTextureSampler, Center + Rot).b;
}
return SXRet( Result / Step);
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
    float v_n9_o = (0.0 + 0.5);
    float v_n8_o = lerp(1.0, v_n9_o, BlurDitherIntensity);
    float v_n6_o = (Blur * v_n8_o);
    float2 v_n12_o = float2(0.5, 0.5);
    float3 v_n16_o = (chromaChannel).xyz;
    float4 b_n3 = (Custom_n3(UV, Step, v_n6_o, v_n12_o, 14.0, 1.0, chroma, v_n16_o, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, 1.0);
    float4 b_n32 = CyTex(0, UV);
    float v_n31_o = (b_n32).w;
    float v_n35_o = (float4(v_n0_o, 1.0)).x;
    float v_n36_o = (float4(v_n0_o, 1.0)).y;
    float v_n34_o = max(v_n35_o, v_n36_o);
    float v_n37_o = (float4(v_n0_o, 1.0)).z;
    float v_n33_o = max(v_n34_o, v_n37_o);
    float v_n30_o = max(v_n31_o, v_n33_o);
    return float4(v_n0_o, v_n30_o);
}
