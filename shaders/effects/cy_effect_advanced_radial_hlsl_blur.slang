//@name Advanced Radial HLSL Blur
//@category Traitements
//@input Image
//@halo full
//@param Distance float 0.05 0.0 1.0 "Distance"
//@param BlurDitherIntensity float 1.0 0.0 2.0 "Blur Dither Intensity"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRadialHLSLBlur.M_CyEffect_AdvancedRadialHLSLBlur: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n0(float BlurDist, float2 ScreenUV, uint Tex)
{
const float Samples[11] = {-0.08,-0.05,-0.03,-0.02,-0.01,0,0.01,0.02,0.03,0.05,0.08};

float2 dir = float2(0.5,0.5) - ScreenUV;
float4 sum = float4(0.0, 0.0, 0.0, 0.0);
for(int i = 0; i<11; i++)
{
    float2 pos = ScreenUV + dir * Samples[i] * BlurDist;
    pos = clamp(pos, float2(0.0,0.0), float2(1.0, 1.0));
    sum +=Texture2DSample(Tex,TexSampler, pos);
}

return SXRet( sum / 11.0);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float v_n4_o = (0.0 + 1.0);
    float v_n3_o = lerp(1.0, v_n4_o, BlurDitherIntensity);
    float v_n1_o = (Distance * v_n3_o);
    float3 b_n0 = (Custom_n0(v_n1_o, UV, 0)).xyz;
    float4 b_n11 = CyTex(0, UV);
    float v_n10_o = (b_n11).w;
    float v_n14_o = (float4(b_n0, 1.0)).x;
    float v_n15_o = (float4(b_n0, 1.0)).y;
    float v_n13_o = max(v_n14_o, v_n15_o);
    float v_n16_o = (float4(b_n0, 1.0)).z;
    float v_n12_o = max(v_n13_o, v_n16_o);
    float v_n9_o = max(v_n10_o, v_n12_o);
    return float4(b_n0, v_n9_o);
}
