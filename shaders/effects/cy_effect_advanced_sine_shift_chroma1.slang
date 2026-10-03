//@name Advanced Sine Shift Chroma1
//@category CyTexture Effets
//@input Image
//@halo full
//@param Sin float 1.0 0.0 2.0 "Sin"
//@param SinPower float 1.0 0.0 2.0 "Sin Power"
//@param Radius float 0.5 0.0 1.0 "Radius"
//@param Density float 1.0 0.0 2.0 "Density"
//@param Power float 1.0 0.0 2.0 "Power"
//@param Lerp float 0.5 0.0 1.0 "Lerp"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSineShiftChroma1.M_CyEffect_AdvancedSineShiftChroma1: Native CyTexture advanced Effect preset with live GPU parameters.
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

float3 RotateAboutAxis_4(float3 axis, float angle, float3 pivot, float3 pos)
{
    float3 c = pivot + axis * dot(axis, pos - pivot);
    float3 u = pos - c, v = cross(axis, u);
    float s, co;
    sincos(angle * 6.283185, s, co);
    return c + u * co + v * s - pos;
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float3 v_f1_n7_o = float3(1.0, 1.0, 1.0);
    float3 v_f1_n6_o = normalize(v_f1_n7_o);
    float2 v_f3_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f3_n3_o = (v_f3_n4_o - ((float2)(1.0)));
    float2 v_f3_n19_o = (v_f3_n3_o * v_f3_n3_o);
    float v_f3_n18_o = (float4(v_f3_n19_o, 0.0, 1.0)).x;
    float v_f3_n20_o = (float4(v_f3_n19_o, 0.0, 1.0)).y;
    float v_f3_n17_o = (v_f3_n18_o + v_f3_n20_o);
    float v_f3_n16_o = sqrt(v_f3_n17_o);
    float v_f2_n2_o = (((float4)(v_f3_n16_o))).x;
    float v_n7_o = (v_f2_n2_o * Sin);
    float v_n6_o = (1.0 - v_n7_o);
    float v_n5_o = pow(max(v_n6_o, 0.0), SinPower);
    float3 v_f1_n1_o = float3(0.0, 1.0, 0.0);
    float3 v_f1_n5_o = RotateAboutAxis_4(v_f1_n6_o, v_n5_o, ((float3)(0.0)), v_f1_n1_o);
    float3 v_f1_n4_o = (v_f1_n5_o + v_f1_n1_o);
    float2 v_f5_n0_o = float2(0.5, 0.5);
    float v_f5_n12_o = distance(UV, v_f5_n0_o);
    float v_f5_n11_o = (v_f5_n12_o * 1.0);
    float v_f5_n10_o = (v_f5_n11_o / Radius);
    float v_f5_f6_n13_o = (v_f5_n10_o * Density);
    float v_f5_f6_n12_o = (v_f5_f6_n13_o * v_f5_f6_n13_o);
    float v_f5_f6_n11_o = ((1.0 > 0.5) ? v_f5_f6_n12_o : v_f5_f6_n13_o);
    float v_f5_f6_n9_o = pow(max(2.718282, 0.0), v_f5_f6_n11_o);
    float v_f5_f6_n7_o = (1.0 / v_f5_f6_n9_o);
    float v_f5_f6_n5_o = ((v_f5_n10_o == 0.0) ? 1.0 : ((v_f5_n10_o > 0.0) ? v_f5_f6_n7_o : 1.0));
    float v_f5_n15_o = (1.0 - v_f5_n10_o);
    float v_f5_f7_n13_o = (v_f5_n15_o * Density);
    float v_f5_f7_n12_o = (v_f5_f7_n13_o * v_f5_f7_n13_o);
    float v_f5_f7_n11_o = ((1.0 > 0.5) ? v_f5_f7_n12_o : v_f5_f7_n13_o);
    float v_f5_f7_n9_o = pow(max(2.718282, 0.0), v_f5_f7_n11_o);
    float v_f5_f7_n7_o = (1.0 / v_f5_f7_n9_o);
    float v_f5_f7_n5_o = ((v_f5_n15_o == 0.0) ? 1.0 : ((v_f5_n15_o > 0.0) ? v_f5_f7_n7_o : 1.0));
    float v_f5_n13_o = (1.0 - v_f5_f7_n5_o);
    float v_f5_n8_o = ((0.0 > 0.5) ? v_f5_f6_n5_o : v_f5_n13_o);
    float v_n12_o = pow(max(v_f5_n8_o, 0.0), Power);
    float3 v_n3_o = lerp(((float3)(1.0)), v_f1_n4_o, v_n12_o);
    float3 v_n1_o = lerp(((float3)(1.0)), v_n3_o, Lerp);
    float4 b_n18 = CyTex(0, UV);
    float3 v_n18_RGB = (b_n18).xyz;
    float3 v_n0_o = (v_n1_o * v_n18_RGB);
    float4 b_n21 = CyTex(0, UV);
    float v_n20_o = (b_n21).w;
    return float4(v_n0_o, v_n20_o);
}
