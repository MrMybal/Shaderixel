//@name Advanced Box Blur Variant
//@category Traitements
//@input Image
//@halo full
//@param Blur float 3.907962 0.0 7.815924 "Blur"
//@param BlurDitherIntensity float 0.7 0.0 1.4 "Blur Dither Intensity"
//@param Index float 14.0 1.0 128.0 "Index"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedBoxBlur.M_CyEffect_AdvancedBoxBlur: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n3(float2 UV, float Blur, float ID, float Filtered, uint CyInputTexture, float4 CyCanvasSize, float2 CyCanvasDimensions, float2 CyInvCanvasSize)
{
// box blur post process
// By CyberAlien 2023

// UV
// Blur
// ID
// Filtered

float2 ViewSize = CyInvCanvasSize;
float4 Result = Texture2DSample(CyInputTexture, CyInputTextureSampler, UV);
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( -1 , 0 ) * Blur * ViewSize));
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( 1 , 0 ) * Blur * ViewSize ));
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( 0 , -1 ) * Blur * ViewSize ));
Result += Texture2DSample(CyInputTexture, CyInputTextureSampler, UV + (float2( 0 , 1 ) * Blur * ViewSize ));
return SXRet( Result / 5 );
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
    float v_n8_o = (0.0 + 0.5);
    float v_n7_o = lerp(1.0, v_n8_o, BlurDitherIntensity);
    float v_n5_o = (Blur * v_n7_o);
    float4 b_n3 = (Custom_n3(UV, v_n5_o, Index, 1.0, 0, float4(ctx.size, ctx.texel), ctx.size, ctx.texel));
    float3 v_n2_o = (b_n3).xyz;
    float3 v_n0_o = lerp(v_n1_RGB, v_n2_o, 1.0);
    float4 b_n20 = CyTex(0, UV);
    float v_n19_o = (b_n20).w;
    float v_n23_o = (float4(v_n0_o, 1.0)).x;
    float v_n24_o = (float4(v_n0_o, 1.0)).y;
    float v_n22_o = max(v_n23_o, v_n24_o);
    float v_n25_o = (float4(v_n0_o, 1.0)).z;
    float v_n21_o = max(v_n22_o, v_n25_o);
    float v_n18_o = max(v_n19_o, v_n21_o);
    return float4(v_n0_o, v_n18_o);
}
