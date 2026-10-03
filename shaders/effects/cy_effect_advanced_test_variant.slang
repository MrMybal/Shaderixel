//@name Advanced TEST Variant
//@category CyTexture Effets
//@input Param
//@input Image
//@halo full
//@param RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param OffsetStep float 16.0 1.0 128.0 "Offset Step"
//@param DistanceRadial float -0.072522 -0.072522 1.0 "Distance Radial"
//@param DistanceOffset float 0.067667 0.0 1.0 "Distance Offset"
//@param Chroma float -0.705092 -0.705092 1.0 "Chroma"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedTEST.M_CyEffect_AdvancedTEST: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n0(float2 UV, float RadialStep, float OffsetStep, float DistanceRadial, float DistanceOffset, float2 Center, uint Tex, float Chroma)
{
// Abberation Radial blur , Texture
// By CyberAlien , 2023

// UV
// RadialStep
// OffsetStep
// DistanceRadial
// DistanceOffset
// Center
// Chroma

const float PI = 6.28318530;
float2 Dir = Center - UV ;
float RadialStepAngle = PI / OffsetStep;
float3 Result;
float2 OffsetCoord;

for( int i = 0 ; i<OffsetStep ; ++i)
{
	OffsetCoord = UV + (float2(cos(i*RadialStepAngle),sin(i*RadialStepAngle)) * DistanceOffset );
	for( int u = 0 ; u<RadialStep ; ++u)
	{
	Dir = Center - OffsetCoord;
	Result = Texture2DSample(Tex , TexSampler, OffsetCoord + (Dir*DistanceRadial*(0.5*Chroma)) ).rgb * saturate(1.0-abs(3*(u/OffsetStep)-float3(0.0,1.0,2.0)));
//	Result.x += Texture2DSample(Tex , TexSampler, OffsetCoord + (Dir*DistanceRadial*(0.5*Chroma)) ).x;
//	Result.y += Texture2DSample(Tex , TexSampler, OffsetCoord + (Dir*DistanceRadial*(1.0*Chroma)) ).y;
//	Result.z += Texture2DSample(Tex , TexSampler, OffsetCoord + (Dir*DistanceRadial*(1.5*Chroma)) ).z;
	}
}
return SXRet( Result / ( RadialStep + OffsetStep ));
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_n6_o = float2(0.5, 0.5);
    float3 b_n0 = (Custom_n0(UV, RadialStep, OffsetStep, DistanceRadial, DistanceOffset, v_n6_o, 0, Chroma)).xyz;
    float4 b_n10 = CyTex(1, UV);
    float v_n9_o = (b_n10).w;
    return float4(b_n0, v_n9_o);
}
