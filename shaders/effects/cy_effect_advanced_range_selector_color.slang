//@name Advanced Range Selector Color
//@category CyTexture Effets
//@input Image
//@halo 0
//@param ColorSelect color 0.0 0.0 0.0 0.0 "Color Select"
//@param Range float 0.2 0.0 1.0 "Range"
//@param Power float 2.0 0.0 4.0 "Power"
//@param MaskMul float 2.0 0.0 4.0 "Mask Mul"
//@param HueShift float 0.0 0.0 1.0 "Hue Shift"
//@param Tint color 1.0 1.0 1.0 0.0 "Tint"
//@param Scurve float 1.0 0.0 2.0 "Scurve"
//@param Luminosity float 1.0 0.0 2.0 "Luminosity"
//@param Contrast float 1.0 0.0 2.0 "Contrast"
//@param Saturation float 0.0 0.0 1.0 "Saturation"
//@param ViewMask float 1.0 0.0 2.0 "View Mask"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRangeSelectorColor.M_CyEffect_AdvancedRangeSelectorColor: Native CyTexture advanced Effect preset with live GPU parameters.
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

float3 RotateAboutAxis_5(float3 axis, float angle, float3 pivot, float3 pos)
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
    float4 b_n7 = CyTex(0, UV);
    float3 v_n7_RGB = (b_n7).xyz;
    float3 v_n8_o = (ColorSelect).xyz;
    float v_n6_o = distance(v_n7_RGB, v_n8_o);
    float v_n5_o = (v_n6_o / Range);
    float v_n4_o = (1.0 - v_n5_o);
    float v_n3_o = pow(max(v_n4_o, 0.0), Power);
    float v_n2_o = (v_n3_o * MaskMul);
    float v_n1_o = clamp(v_n2_o, 0.0, 1.0);
    float3 v_f1_n7_o = float3(1.0, 1.0, 1.0);
    float3 v_f1_n6_o = normalize(v_f1_n7_o);
    float3 v_n21_o = (Tint).xyz;
    float3 v_n20_o = (v_n7_RGB * v_n21_o);
    float3 v_f4_n9_o = (((float3)(2.0)) * v_n20_o);
    float3 v_f4_n8_o = clamp(v_f4_n9_o, ((float3)(0.0)), ((float3)(1.0)));
    float3 v_f4_n7_o = pow(max(v_f4_n8_o, 0.0), ((float3)(Scurve)));
    float3 v_f4_n6_o = (v_f4_n7_o * ((float3)(0.5)));
    float v_f4_n21_o = -0.5;
    float3 v_f4_n20_o = (v_n20_o + ((float3)(v_f4_n21_o)));
    float3 v_f4_n19_o = (v_f4_n20_o * ((float3)(2.0)));
    float3 v_f4_n18_o = clamp(v_f4_n19_o, ((float3)(0.0)), ((float3)(1.0)));
    float3 v_f4_n17_o = (1.0 - v_f4_n18_o);
    float3 v_f4_n16_o = pow(max(v_f4_n17_o, 0.0), ((float3)(Scurve)));
    float3 v_f4_n15_o = (1.0 - v_f4_n16_o);
    float3 v_f4_n14_o = (v_f4_n15_o * ((float3)(0.5)));
    float3 v_f4_n12_o = (((float3)(0.5)) + v_f4_n14_o);
    float3 v_f4_n25_o = (v_f4_n15_o * ((float3)(500.0)));
    float3 v_f4_n24_o = clamp(v_f4_n25_o, ((float3)(0.0)), ((float3)(1.0)));
    float3 v_f4_n5_o = lerp(v_f4_n6_o, v_f4_n12_o, v_f4_n24_o);
    float3 v_f3_n4_o = normalize(v_f4_n5_o);
    float3 v_f3_n3_o = (v_f3_n4_o * ((float3)(Luminosity)));
    float3 v_f2_n4_o = normalize(v_f3_n3_o);
    float3 v_f2_n6_o = lerp(v_f3_n3_o, (float3)dot(v_f3_n3_o, float3(0.3, 0.59, 0.11)), 1.0);
    float3 v_f2_n5_o = pow(max(v_f2_n6_o, 0.0), ((float3)(Contrast)));
    float3 v_f2_n3_o = (v_f2_n4_o * v_f2_n5_o);
    float3 v_n16_o = lerp(v_f2_n3_o, (float3)dot(v_f2_n3_o, float3(0.3, 0.59, 0.11)), Saturation);
    float3 v_f1_n5_o = RotateAboutAxis_5(v_f1_n6_o, HueShift, ((float3)(0.0)), v_n16_o);
    float3 v_f1_n4_o = (v_f1_n5_o + v_n16_o);
    float3 v_n13_o = (float4(v_f1_n4_o, 1.0)).xyz;
    float3 v_n12_o = lerp(v_n7_RGB, v_n13_o, v_n1_o);
    float3 v_n0_o = lerp(((float3)(v_n1_o)), v_n12_o, ViewMask);
    float4 b_n28 = CyTex(0, UV);
    float v_n27_o = (b_n28).w;
    return float4(v_n0_o, v_n27_o);
}
