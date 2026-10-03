//@name Advanced Radial HLSL Blur2
//@category CyTexture Effets
//@input Image
//@halo full
//@param RadialStep float 16.0 1.0 128.0 "Radial Step"
//@param Distance float 0.05 0.0 1.0 "Distance"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRadialHLSLBlur2.M_CyEffect_AdvancedRadialHLSLBlur2: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n0(float RadialStep, float2 UV, float BlurDist, uint Tex)
{
//Radial blur , TExture
//By CyberAlien , 2022


//RadialStep , number of Step
//UV , input UV coord
//BlurDist , Blur
//Tex , TextureInput

const float PI = 3.14159265 ;
float RadialStepAngle = ( 2 * PI ) / RadialStep ;
float4 Result = 0 ;
float2 dir = float2(0.5,0.5) - UV;


for(int i = 0; i<RadialStep; i++)
{
    float2 pos = UV + dir * ( i * RadialStepAngle ) * BlurDist;
    pos = clamp(pos, float2(0.0,0.0), float2(1.0, 1.0));
    Result +=Texture2DSample(Tex,TexSampler, pos);
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
    float3 b_n0 = (Custom_n0(RadialStep, UV, Distance, 0)).xyz;
    float4 b_n7 = CyTex(0, UV);
    float v_n6_o = (b_n7).w;
    float v_n10_o = (float4(b_n0, 1.0)).x;
    float v_n11_o = (float4(b_n0, 1.0)).y;
    float v_n9_o = max(v_n10_o, v_n11_o);
    float v_n12_o = (float4(b_n0, 1.0)).z;
    float v_n8_o = max(v_n9_o, v_n12_o);
    float v_n5_o = max(v_n6_o, v_n8_o);
    return float4(b_n0, v_n5_o);
}
