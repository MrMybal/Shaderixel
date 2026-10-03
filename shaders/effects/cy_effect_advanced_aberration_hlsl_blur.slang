//@name Advanced Aberration HLSL Blur
//@category Traitements
//@input Image
//@halo full
//@param X float 1.0 0.0 2.0 "X"
//@param Y float 1.0 0.0 2.0 "Y"
//@param Distance float 0.001 0.0 1.0 "Distance"
//@param Step float 16.0 1.0 128.0 "Step"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedAberrationHLSLBlur.M_CyEffect_AdvancedAberrationHLSLBlur: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_f1_n7(uint Tex, float2 UV, float2 Vector, float Distance, float Steps)
{
float3 CurColor = (float3 )0;
float3 newSample = (float3 )0;

int i = (int )0;
Steps = max(Steps, 1);
float StepSize = Distance / (int) Steps;
float2 NewUV = UV - Vector*0.5*Distance;
while ( i < (int) Steps)
{
float3 newSample=Texture2DSample(Tex,TexSampler,NewUV).rgb;
float3 CurrentAbberationValue=saturate(1.0-abs(3*(i/Steps)-float4(0.0,1.0,2.0,3.0))).xyz;
CurColor += newSample*CurrentAbberationValue;
NewUV.x += (StepSize * Vector.x);
NewUV.y += (StepSize * Vector.y);
i++;
}
CurColor = CurColor / i;
return SXRet( CurColor);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_f2_n3_o = float2(X, Y);
    float3 b_f1_n7 = (Custom_f1_n7(0, UV, v_f2_n3_o, Distance, Step)).xyz;
    float3 v_n0_o = (b_f1_n7 * ((float3)(4.0)));
    float4 b_n11 = CyTex(0, UV);
    float v_n10_o = (b_n11).w;
    float v_n14_o = (float4(v_n0_o, 1.0)).x;
    float v_n15_o = (float4(v_n0_o, 1.0)).y;
    float v_n13_o = max(v_n14_o, v_n15_o);
    float v_n16_o = (float4(v_n0_o, 1.0)).z;
    float v_n12_o = max(v_n13_o, v_n16_o);
    float v_n9_o = max(v_n10_o, v_n12_o);
    return float4(v_n0_o, v_n9_o);
}
