//@name Advanced Sharpen Depth Ppm2 Variant
//@category Traitements
//@input Image
//@halo full
//@param Sharpen float 3.0 0.0 6.0 "Sharpen"
//@param Index float 14.0 1.0 128.0 "Index"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSharpenDepthPpm2.M_CyEffect_AdvancedSharpenDepthPpm2: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float Sharpen, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// box Sharpen post process
// By CyberAlien 2023

// UV
// Sharpen
// ID
// Filtered

float2 ViewSize = CyInvCanvasSize;
float4 Result = Texture2DSample(CyInputTexture, CyInputTextureSampler, UV);
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( -1 , 0 ) * Sharpen * ViewSize));
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( 1 , 0 ) * Sharpen * ViewSize ));
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( 0 , -1 ) * Sharpen * ViewSize ));
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( 0 , 1 ) * Sharpen * ViewSize ));
Result = ( Result * ( -1.0 / 5 ) ) * Sharpen ;
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV) * ( Sharpen + 1.0 );
return SXRet( Result);
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
    float4 b_n3 = (Custom_n3(UV, Sharpen, Index, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float v_n12_o = (1.0 - 1.0);
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, v_n12_o);
    float4 b_n16 = CyTex(0, UV);
    float v_n15_o = (b_n16).w;
    return float4(v_n0_o, v_n15_o);
}
