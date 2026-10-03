//@name Advanced Radial HLSL Chroma
//@category Traitements
//@input Image
//@halo full
//@param Blur float 0.1 0.0 1.0 "Blur"
//@param BlurDitherIntensity float 1.0 0.0 2.0 "Blur Dither Intensity"
//@param Chroma_Offset float 0.45 0.0 1.0 "Chroma Offset"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRadialHLSLChroma.M_CyEffect_AdvancedRadialHLSLChroma: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n2(float BlurDist, float2 ScreenUV, uint Tex)
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

float4 Custom_n13(float BlurDist, float2 ScreenUV, uint Tex)
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

float4 Custom_n21(float BlurDist, float2 ScreenUV, uint Tex)
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
    float v_n6_o = (0.0 + 1.0);
    float v_n5_o = lerp(1.0, v_n6_o, BlurDitherIntensity);
    float v_n3_o = (Blur * v_n5_o);
    float2 v_n9_o = (UV * ((float2)(1.0)));
    float3 b_n2 = (Custom_n2(v_n3_o, v_n9_o, 0)).xyz;
    float v_f2_n2_o = (float4(b_n2, 1.0)).x;
    float v_n17_o = (Chroma_Offset / 100.0);
    float v_n16_o = (1.0 - v_n17_o);
    float2 v_n15_o = (UV * ((float2)(v_n16_o)));
    float v_n19_o = (v_n17_o / 2.0);
    float2 v_n14_o = (v_n15_o + ((float2)(v_n19_o)));
    float3 b_n13 = (Custom_n13(v_n3_o, v_n14_o, 0)).xyz;
    float v_f3_n4_o = (float4(b_n13, 1.0)).y;
    float2 v_f1_n5_o = float2(v_f2_n2_o, v_f3_n4_o);
    float v_n24_o = (v_n16_o - v_n17_o);
    float2 v_n23_o = (UV * ((float2)(v_n24_o)));
    float v_n26_o = (v_n17_o * 2.0);
    float v_n25_o = (v_n26_o / 2.0);
    float2 v_n22_o = (v_n23_o + ((float2)(v_n25_o)));
    float3 b_n21 = (Custom_n21(v_n3_o, v_n22_o, 0)).xyz;
    float v_f4_n6_o = (float4(b_n21, 1.0)).z;
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_f4_n6_o);
    float4 b_n28 = CyTex(0, UV);
    float v_n27_o = (b_n28).w;
    return float4(v_f1_n4_o, v_n27_o);
}
