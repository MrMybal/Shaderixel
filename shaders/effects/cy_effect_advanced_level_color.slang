//@name Advanced Level Color
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Black float 0.0 0.0 1.0 "Black"
//@param Middle float 0.5 0.0 1.0 "Middle"
//@param Middle_Value float 0.5 0.0 1.0 "Middle Value"
//@param InvertPower float 0.0 0.0 1.0 "Invert Power"
//@param InterpolationPower float 0.5 0.0 1.0 "Interpolation Power"
//@param Interpolation float 0.0 0.0 1.0 "Interpolation"
//@param White float 1.0 0.0 2.0 "White"
//@param Out_Min float 0.0 0.0 1.0 "Out Min"
//@param Out_Max float 1.0 0.0 2.0 "Out Max"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedLevelColor.M_CyEffect_AdvancedLevelColor: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float4 b_n3 = CyTex(0, UV);
    float v_n3_R = (b_n3).x;
    float v_f2_n24_o = (v_n3_R / Middle_Value);
    float v_f2_n23_o = clamp(v_f2_n24_o, 0.0, 1.0);
    float v_f2_n22_o = (1.0 - v_f2_n23_o);
    float v_f2_n21_o = ((InvertPower > 0.5) ? v_f2_n22_o : v_f2_n23_o);
    float v_f2_n20_o = pow(max(v_f2_n21_o, 0.0), InterpolationPower);
    float v_f2_n19_o = (1.0 - v_f2_n20_o);
    float v_f2_n18_o = ((InvertPower > 0.5) ? v_f2_n19_o : v_f2_n20_o);
    float v_f2_n17_o = ((Interpolation > 0.5) ? v_f2_n18_o : v_f2_n23_o);
    float v_f2_n16_o = lerp(Black, Middle, v_f2_n17_o);
    float v_f2_n34_o = (1.0 - v_n3_R);
    float v_f2_n35_o = (1.0 - Middle_Value);
    float v_f2_n33_o = (v_f2_n34_o / v_f2_n35_o);
    float v_f2_n32_o = clamp(v_f2_n33_o, 0.0, 1.0);
    float v_f2_n31_o = (1.0 - v_f2_n32_o);
    float v_f2_n30_o = ((InvertPower > 0.5) ? v_f2_n31_o : v_f2_n32_o);
    float v_f2_n29_o = pow(max(v_f2_n30_o, 0.0), InterpolationPower);
    float v_f2_n28_o = (1.0 - v_f2_n29_o);
    float v_f2_n27_o = ((InvertPower > 0.5) ? v_f2_n28_o : v_f2_n29_o);
    float v_f2_n26_o = ((Interpolation > 0.5) ? v_f2_n27_o : v_f2_n32_o);
    float v_f2_n25_o = lerp(White, Middle, v_f2_n26_o);
    float v_f2_n36_o = floor(v_f2_n17_o);
    float v_f2_n15_o = lerp(v_f2_n16_o, v_f2_n25_o, v_f2_n36_o);
    float v_n3_G = (b_n3).y;
    float v_f3_n24_o = (v_n3_G / Middle_Value);
    float v_f3_n23_o = clamp(v_f3_n24_o, 0.0, 1.0);
    float v_f3_n22_o = (1.0 - v_f3_n23_o);
    float v_f3_n21_o = ((InvertPower > 0.5) ? v_f3_n22_o : v_f3_n23_o);
    float v_f3_n20_o = pow(max(v_f3_n21_o, 0.0), InterpolationPower);
    float v_f3_n19_o = (1.0 - v_f3_n20_o);
    float v_f3_n18_o = ((InvertPower > 0.5) ? v_f3_n19_o : v_f3_n20_o);
    float v_f3_n17_o = ((Interpolation > 0.5) ? v_f3_n18_o : v_f3_n23_o);
    float v_f3_n16_o = lerp(Black, Middle, v_f3_n17_o);
    float v_f3_n34_o = (1.0 - v_n3_G);
    float v_f3_n35_o = (1.0 - Middle_Value);
    float v_f3_n33_o = (v_f3_n34_o / v_f3_n35_o);
    float v_f3_n32_o = clamp(v_f3_n33_o, 0.0, 1.0);
    float v_f3_n31_o = (1.0 - v_f3_n32_o);
    float v_f3_n30_o = ((InvertPower > 0.5) ? v_f3_n31_o : v_f3_n32_o);
    float v_f3_n29_o = pow(max(v_f3_n30_o, 0.0), InterpolationPower);
    float v_f3_n28_o = (1.0 - v_f3_n29_o);
    float v_f3_n27_o = ((InvertPower > 0.5) ? v_f3_n28_o : v_f3_n29_o);
    float v_f3_n26_o = ((Interpolation > 0.5) ? v_f3_n27_o : v_f3_n32_o);
    float v_f3_n25_o = lerp(White, Middle, v_f3_n26_o);
    float v_f3_n36_o = floor(v_f3_n17_o);
    float v_f3_n15_o = lerp(v_f3_n16_o, v_f3_n25_o, v_f3_n36_o);
    float2 v_f1_n5_o = float2(v_f2_n15_o, v_f3_n15_o);
    float v_n3_B = (b_n3).z;
    float v_f4_n24_o = (v_n3_B / Middle_Value);
    float v_f4_n23_o = clamp(v_f4_n24_o, 0.0, 1.0);
    float v_f4_n22_o = (1.0 - v_f4_n23_o);
    float v_f4_n21_o = ((InvertPower > 0.5) ? v_f4_n22_o : v_f4_n23_o);
    float v_f4_n20_o = pow(max(v_f4_n21_o, 0.0), InterpolationPower);
    float v_f4_n19_o = (1.0 - v_f4_n20_o);
    float v_f4_n18_o = ((InvertPower > 0.5) ? v_f4_n19_o : v_f4_n20_o);
    float v_f4_n17_o = ((Interpolation > 0.5) ? v_f4_n18_o : v_f4_n23_o);
    float v_f4_n16_o = lerp(Black, Middle, v_f4_n17_o);
    float v_f4_n34_o = (1.0 - v_n3_B);
    float v_f4_n35_o = (1.0 - Middle_Value);
    float v_f4_n33_o = (v_f4_n34_o / v_f4_n35_o);
    float v_f4_n32_o = clamp(v_f4_n33_o, 0.0, 1.0);
    float v_f4_n31_o = (1.0 - v_f4_n32_o);
    float v_f4_n30_o = ((InvertPower > 0.5) ? v_f4_n31_o : v_f4_n32_o);
    float v_f4_n29_o = pow(max(v_f4_n30_o, 0.0), InterpolationPower);
    float v_f4_n28_o = (1.0 - v_f4_n29_o);
    float v_f4_n27_o = ((InvertPower > 0.5) ? v_f4_n28_o : v_f4_n29_o);
    float v_f4_n26_o = ((Interpolation > 0.5) ? v_f4_n27_o : v_f4_n32_o);
    float v_f4_n25_o = lerp(White, Middle, v_f4_n26_o);
    float v_f4_n36_o = floor(v_f4_n17_o);
    float v_f4_n15_o = lerp(v_f4_n16_o, v_f4_n25_o, v_f4_n36_o);
    float3 v_f1_n4_o = float3(v_f1_n5_o, v_f4_n15_o);
    float3 v_n0_o = clamp(v_f1_n4_o, ((float3)(Out_Min)), ((float3)(Out_Max)));
    float4 b_n17 = CyTex(0, UV);
    float v_n16_o = (b_n17).w;
    return float4(v_n0_o, v_n16_o);
}
