//@name Advanced Spiral HLSL Blur
//@category Traitements
//@input Image
//@halo full
//@param Distance float 0.05 0.0 1.0 "Distance"
//@param DistanceStep float 16.0 1.0 128.0 "Distance Step"
//@param RadialSteps float 8.0 1.0 128.0 "Radial Steps"
//@param RadialOffset float 0.62 0.0 1.24 "Radial Offset"
//@param KernelPower float 1.0 0.0 2.0 "Kernel Power"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSpiralHLSLBlur.M_CyEffect_AdvancedSpiralHLSLBlur: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_f1_n10(uint Tex, float2 UV, float Distance, float DistanceSteps, float RadialSteps, float RadialOffset, float KernelPower)
{
float3 CurColor=0;
float2 NewUV = UV;
int i=0;
float StepSize = Distance / (int) DistanceSteps;
float CurDistance=0;
float2 CurOffset=0;
float SubOffset = 0;
float TwoPi = 6.283185;
float accumdist=0;

if (DistanceSteps < 1)

{
return SXRet( Texture2DSample(Tex,TexSampler,UV));		
}
else
{
while ( i < (int) DistanceSteps)
{
	CurDistance += StepSize;
	for (int j = 0; j < (int) RadialSteps; j++)
	{
SubOffset +=1;
CurOffset.x = cos(TwoPi*(SubOffset / RadialSteps));
CurOffset.y = sin(TwoPi*(SubOffset / RadialSteps));
		NewUV.x = UV.x + CurOffset.x * CurDistance;
		NewUV.y = UV.y + CurOffset.y * CurDistance;
		float distpow = pow(CurDistance, KernelPower);
		CurColor += Texture2DSample(Tex,TexSampler,NewUV).rgb*distpow;		
		accumdist += distpow;
	}
SubOffset +=RadialOffset;
i++;
}
CurColor = CurColor;
CurColor /=accumdist;
return SXRet( CurColor);
}
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float3 b_f1_n10 = (Custom_f1_n10(0, UV, Distance, DistanceStep, RadialSteps, RadialOffset, KernelPower)).xyz;
    float4 b_n10 = CyTex(0, UV);
    float v_n9_o = (b_n10).w;
    float v_n13_o = (float4(b_f1_n10, 1.0)).x;
    float v_n14_o = (float4(b_f1_n10, 1.0)).y;
    float v_n12_o = max(v_n13_o, v_n14_o);
    float v_n15_o = (float4(b_f1_n10, 1.0)).z;
    float v_n11_o = max(v_n12_o, v_n15_o);
    float v_n8_o = max(v_n9_o, v_n11_o);
    return float4(b_f1_n10, v_n8_o);
}
