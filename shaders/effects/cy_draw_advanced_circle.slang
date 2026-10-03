//@name Advanced Circle
//@category Dessin
//@generator
//@halo 0
//@param Radius float 0.25 0.0 1.0 "Radius"
//@param Distance float 0.25 0.0 1.0 "Distance"
//@param Range float 10.0 0.0 20.0 "Range"
//@param Color color 1.0 0.60322 0.0 0.0 "Color"
//@param Luminance_from_Linear float 0.0 0.0 1.0 "Luminance from Linear"
//@param OutMul float 1.0 0.0 2.0 "Out Mul"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedCircle.M_CyDraw_AdvancedCircle: Native CyTexture advanced Draw preset with live GPU parameters.
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
    float2 v_f2_n0_o = float2(0.5, 0.5);
    float v_f2_n12_o = distance(UV, v_f2_n0_o);
    float v_f2_n11_o = (v_f2_n12_o * 1.0);
    float v_f2_n10_o = (v_f2_n11_o / Radius);
    float v_f2_f3_n13_o = (v_f2_n10_o * 2.333);
    float v_f2_f3_n12_o = (v_f2_f3_n13_o * v_f2_f3_n13_o);
    float v_f2_f3_n11_o = ((1.0 > 0.5) ? v_f2_f3_n12_o : v_f2_f3_n13_o);
    float v_f2_f3_n9_o = pow(max(2.718282, 0.0), v_f2_f3_n11_o);
    float v_f2_f3_n7_o = (1.0 / v_f2_f3_n9_o);
    float v_f2_f3_n5_o = ((v_f2_n10_o == 0.0) ? 1.0 : ((v_f2_n10_o > 0.0) ? v_f2_f3_n7_o : 1.0));
    float v_f2_n15_o = (1.0 - v_f2_n10_o);
    float v_f2_f4_n13_o = (v_f2_n15_o * 2.333);
    float v_f2_f4_n12_o = (v_f2_f4_n13_o * v_f2_f4_n13_o);
    float v_f2_f4_n11_o = ((1.0 > 0.5) ? v_f2_f4_n12_o : v_f2_f4_n13_o);
    float v_f2_f4_n9_o = pow(max(2.718282, 0.0), v_f2_f4_n11_o);
    float v_f2_f4_n7_o = (1.0 / v_f2_f4_n9_o);
    float v_f2_f4_n5_o = ((v_f2_n15_o == 0.0) ? 1.0 : ((v_f2_n15_o > 0.0) ? v_f2_f4_n7_o : 1.0));
    float v_f2_n13_o = (1.0 - v_f2_f4_n5_o);
    float v_f2_n8_o = ((0.0 > 0.5) ? v_f2_f3_n5_o : v_f2_n13_o);
    float v_n7_o = distance(v_f2_n8_o, Distance);
    float v_n6_o = (v_n7_o * Range);
    float v_n5_o = (1.0 - v_n6_o);
    float v_n4_o = saturate(v_n5_o);
    float v_n3_o = saturate(v_n4_o);
    float3 v_n13_o = (Color).xyz;
    float3 v_n2_o = (((float3)(v_n3_o)) * v_n13_o);
    float3 v_f1_n10_o = (v_n2_o * ((float3)(100.0)));
    float3 v_f1_n18_o = pow(max(v_n2_o, 0.0), ((float3)(0.454545)));
    float3 v_f1_n17_o = ((Luminance_from_Linear > 0.5) ? v_f1_n18_o : v_n2_o);
    float3 v_f1_n16_o = lerp(v_f1_n17_o, (float3)dot(v_f1_n17_o, float3(0.3, 0.59, 0.11)), 1.0);
    float3 v_f1_n15_o = pow(max(v_f1_n16_o, 0.0), ((float3)(100.0)));
    float3 v_f1_n14_o = (((float3)(100.0)) * v_f1_n15_o);
    float3 v_f1_n13_o = (((float3)(1.0)) - v_f1_n14_o);
    float3 v_f1_n12_o = (((float3)(100.0)) + v_f1_n13_o);
    float3 v_f1_n6_o = float3(1.0, 1.0, 1.0);
    float3 v_f1_n20_o = lerp(((float3)(1.0)), v_f1_n6_o, v_f1_n15_o);
    float3 v_f1_n19_o = (((float3)(1.0)) * v_f1_n20_o);
    float3 v_f1_n11_o = (v_f1_n12_o / v_f1_n19_o);
    float3 v_f1_n9_o = (v_f1_n10_o / v_f1_n11_o);
    float3 v_n0_o = (v_f1_n9_o * ((float3)(OutMul)));
    float v_n18_o = (float4(v_n0_o, 1.0)).x;
    float v_n19_o = (float4(v_n0_o, 1.0)).y;
    float v_n17_o = max(v_n18_o, v_n19_o);
    float v_n20_o = (float4(v_n0_o, 1.0)).z;
    float v_n16_o = max(v_n17_o, v_n20_o);
    return float4(v_n0_o, v_n16_o);
}
