//@name Advanced Spiral Blurchroma P Pm2
//@category Traitements
//@input Image
//@input n15
//@halo full
//@param RadialStep float 1.0 1.0 128.0 "Radial Step"
//@param Spiral float 1.0 0.0 2.0 "Spiral"
//@param Blur float 0.1 0.0 1.0 "Blur"
//@param BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param Index float 14.0 1.0 128.0 "Index"
//@param chroma float 14.0 0.0 28.0 "chroma"
//@param chromaChannel color 0.8 1.0 1.2 0.0 "chroma Channel"
//@param type float 1.0 0.0 2.0 "type"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSpiralBlurchromaPPm2.M_CyEffect_AdvancedSpiralBlurchromaPPm2: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n4(float2 UV, float RadialStep, float Spiral, float Blur, uint Tex, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// SpiralBlur post process
// By CyberAlien 2023

// UV
// RadialStep
// Spiral
// Blur
// Tex
// ID
// Filtered

// TemporalAA
float2 ScenePixels=View.BufferSizeAndInvSize.xy*ViewportUVToBufferUV(UV);
ScenePixels +=View.TemporalAAParams.r;
float2 RandomSamp = ((uint)(ScenePixels.x) + 2 * (uint)(ScenePixels.y)) % 5;
RandomSamp += Texture2DSample(Tex,TexSampler,ScenePixels).xy;
RandomSamp /= 5;
RandomSamp -= 0.5;
float TempAARotation = RandomSamp.x;
float TempAADistance = Spiral*RandomSamp.x;

float PI = 6.28318530;
float RadialStepAngle = PI / RadialStep * Spiral;
float Angle = (float )0;
float2 StepVector = (float2 )0;
float2 Coord = (float2 )0;
float4 Result = Texture2DSample(CyInputTexture, CyInputTextureSampler, UV);

for( int i = 0 ; i<RadialStep ; ++i )
{
	Angle = i * RadialStepAngle;
	StepVector = float2(cos( Angle * TempAARotation ) , sin( Angle * TempAARotation ) );
	Coord = ViewportUVToBufferUV( UV + ( StepVector * Blur ) );
	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, Coord);
}
return SXRet( Result / ( RadialStep + 2 ));
}

float4 Custom_n24(float2 UV, float RadialStep, float Spiral, float Blur, float ID, float Filtered, float ChromaOffset, float3 ChromaChannel, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// SpiralBlur chroma post process
// By CyberAlien 2023

// UV
// RadialStep
// Spiral
// Blur
// ID
// Filtered
// ChromaOffset
// ChromaChannel

float PI = 6.28318530;
float RadialStepAngle = PI / RadialStep * Spiral;
float Angle = (float )0;
float2 StepVector = (float2 )0;
float2 Coord = (float2 )0;
float4 Result = Texture2DSample(CyInputTexture, CyInputTextureSampler, UV);

for( int i = 0 ; i<RadialStep ; ++i )
{
	Angle = i * RadialStepAngle;
	StepVector = float2(cos( Angle ) , sin( Angle ) );
//	Coord = ViewportUVToBufferUV( UV + ( StepVector * Blur ) );
//	Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, Coord);
	Coord = ViewportUVToBufferUV( (UV + (StepVector * Blur * ((ChromaChannel.r*ChromaOffset) + 1) ) ));
	Result.r += Texture2DSample(CyInputTexture, CyInputTextureSampler, Coord).r;
	Coord = ViewportUVToBufferUV( (UV + ( StepVector * Blur * ((ChromaChannel.g * ChromaOffset) + 1) ) ));
	Result.g += Texture2DSample(CyInputTexture, CyInputTextureSampler, Coord).g;
	Coord = ViewportUVToBufferUV( (UV + ( StepVector * Blur * ((ChromaChannel.b * ChromaOffset) + 1) ) ));
	Result.b += Texture2DSample(CyInputTexture, CyInputTextureSampler, Coord).b;
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
    float4 b_n2 = CyTex(0, UV);
    float3 v_n2_RGB = (b_n2).xyz;
    float v_n12_o = (0.0 + 0.5);
    float v_n11_o = lerp(1.0, v_n12_o, BlurDitherIntensity);
    float v_n9_o = (Blur * v_n11_o);
    float v_n8_o = (v_n9_o / 100.0);
    float4 b_n4 = (Custom_n4(UV, RadialStep, Spiral, v_n8_o, 1, Index, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n3_o = (b_n4).xyz;
    float3 v_n1_o = lerp(v_n2_RGB, v_n3_o, 1.0);
    float3 v_n26_o = (chromaChannel).xyz;
    float4 b_n24 = (Custom_n24(UV, RadialStep, Spiral, v_n8_o, Index, 1.0, chroma, v_n26_o, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n23_o = (b_n24).xyz;
    float3 v_n0_o = lerp(v_n1_o, v_n23_o, type);
    float4 b_n30 = CyTex(0, UV);
    float v_n29_o = (b_n30).w;
    float v_n33_o = (float4(v_n0_o, 1.0)).x;
    float v_n34_o = (float4(v_n0_o, 1.0)).y;
    float v_n32_o = max(v_n33_o, v_n34_o);
    float v_n35_o = (float4(v_n0_o, 1.0)).z;
    float v_n31_o = max(v_n32_o, v_n35_o);
    float v_n28_o = max(v_n29_o, v_n31_o);
    return float4(v_n0_o, v_n28_o);
}
