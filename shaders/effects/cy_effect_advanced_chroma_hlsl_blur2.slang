//@name Advanced Chroma HLSL Blur2
//@category CyTexture Effets
//@input Image
//@halo full
//@param Step float 16.0 1.0 128.0 "Step"
//@param Distance float 0.05 0.0 1.0 "Distance"
//@param BlurDitherIntensity float 1.0 0.0 2.0 "Blur Dither Intensity"
//@param Red float 1.0 0.0 2.0 "Red"
//@param Green float 2.0 0.0 4.0 "Green"
//@param Blue float 3.0 0.0 6.0 "Blue"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedChromaHLSLBlur2.M_CyEffect_AdvancedChromaHLSLBlur2: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n0(float RadialStep, float2 UV, uint Tex, float ChromaOffset, float Red, float Green, float Blue)
{
// Chroma Spiral Blur

// RadialStep
// UV
// ChromaOffset

 const float PI = 3.14159265 ;
 float RadialStepAngle = ( 2 * PI ) / RadialStep ;
 float4 Result = 0 ;
 
 
 for ( int i= 1 ; i<RadialStep ; ++i )
 {
	 float Angle = i*RadialStepAngle ;
	 float2 OffsetVector = float2(cos(Angle),sin(Angle)) ;
	 float2 UVOffset = OffsetVector + UV ; 
	 Result.r += Texture2DSample(Tex , TexSampler , UV + ( ChromaOffset * 0.1 * Red * OffsetVector ) ).r;
	 Result.g += Texture2DSample(Tex , TexSampler , UV + ( ChromaOffset * 0.1 * Green * OffsetVector ) ).g;
	 Result.b += Texture2DSample(Tex , TexSampler , UV + ( ChromaOffset * 0.1 * Blue * OffsetVector ) ).b;
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
    float v_n7_o = (0.0 + 1.0);
    float v_n6_o = lerp(1.0, v_n7_o, BlurDitherIntensity);
    float v_n4_o = (Distance * v_n6_o);
    float3 b_n0 = (Custom_n0(Step, UV, 0, v_n4_o, Red, Green, Blue)).xyz;
    float4 b_n15 = CyTex(0, UV);
    float v_n14_o = (b_n15).w;
    float v_n18_o = (float4(b_n0, 1.0)).x;
    float v_n19_o = (float4(b_n0, 1.0)).y;
    float v_n17_o = max(v_n18_o, v_n19_o);
    float v_n20_o = (float4(b_n0, 1.0)).z;
    float v_n16_o = max(v_n17_o, v_n20_o);
    float v_n13_o = max(v_n14_o, v_n16_o);
    return float4(b_n0, v_n13_o);
}
