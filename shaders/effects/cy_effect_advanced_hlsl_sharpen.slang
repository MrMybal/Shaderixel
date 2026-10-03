//@name Advanced HLSL Sharpen
//@category CyTexture Effets
//@input Image
//@halo full
//@param RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param Sharpen float 0.1 0.0 1.0 "Sharpen"
//@param TextureSize float 2048.0 0.0 4096.0 "Texture Size"
//@param p_lerp float 0.0 0.0 1.0 "lerp"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedHLSLSharpen.M_CyEffect_AdvancedHLSLSharpen: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n1(float2 UV, float RadialStep, float Sharpen, uint Tex, float TextureSize, float2 Radius)
{
// CyberAlien PostProcess HLSL
// Sharpen Radial
// v 1.0

// float2 , UV 
// float , RadialStep
// float , Intensity
// float , Shapen
// Tex , TextureInput
// TextureSize

// PI
const float PI = 3.14159265 ;
// RadialStep
float RadialStepAngle = ( 2 * PI ) / RadialStep ;
// Coord
float2 Coord = UV ;
// Store Result
float4 Result = Texture2DSample( Tex,TexSampler, Coord ) ;
float PX_Offset = 1 / TextureSize ;

// Exec

for ( int i=1 ; i<=RadialStep ; ++i )
{
	float Angle = i * RadialStepAngle ;
	float2 OffsetVector = float2(cos(Angle),sin(Angle)) ;
	float2 OffsetPX = ( PX_Offset * OffsetVector ) * Radius ;
	Coord = UV.xy + OffsetPX  ;
	Result += Texture2DSample( Tex,TexSampler, saturate(Coord) ) ;
}
Result = ( Result * ( -1.0 / RadialStep ) ) * Sharpen ;
Result += Texture2DSample( Tex,TexSampler, saturate(Coord) ) * ( Sharpen + 1.0 ); 
return SXRet( Result );
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_n7_o = float2(1.0, 1.0);
    float3 b_n1 = (Custom_n1(UV, RadialStep, Sharpen, 0, TextureSize, v_n7_o)).xyz;
    float4 b_n8 = CyTex(0, UV);
    float3 v_n8_RGB = (b_n8).xyz;
    float3 v_n0_o = lerp(b_n1, v_n8_RGB, p_lerp);
    float4 b_n11 = CyTex(0, UV);
    float v_n10_o = (b_n11).w;
    return float4(v_n0_o, v_n10_o);
}
