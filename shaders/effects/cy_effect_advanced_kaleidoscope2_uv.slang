//@name Advanced Kaleidoscope2 UV
//@category Traitements
//@input Image
//@halo full
//@param Power_X float 1.0 0.0 2.0 "Power X"
//@param Add_X float 0.0 0.0 1.0 "Add X"
//@param Power_Y float 1.0 0.0 2.0 "Power Y"
//@param Add_Y float 0.0 0.0 1.0 "Add Y"
//@param Offset float 0.0 0.0 1.0 "Offset"
//@param Compare float 0.5 0.0 1.0 "Compare"
//@param Width float 1.0 0.0 2.0 "Width"
//@param Sharpness float 0.0 0.0 1.0 "Sharpness"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedKaleidoscope2UV.M_CyEffect_AdvancedKaleidoscope2UV: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float v_f4_n2_o = (float4(UV, 0.0, 1.0)).x;
    float v_n9_o = pow(max(v_f4_n2_o, 0.0), Power_X);
    float v_n8_o = (v_n9_o + Add_X);
    float v_f5_n4_o = (float4(UV, 0.0, 1.0)).y;
    float v_n15_o = pow(max(v_f5_n4_o, 0.0), Power_Y);
    float v_n14_o = (v_n15_o + Add_Y);
    float2 v_f3_n3_o = float2(v_n8_o, v_n14_o);
    float2 v_f2_n14_o = (v_f3_n3_o + ((float2)(Offset)));
    float2 v_f2_n13_o = frac(v_f2_n14_o);
    float v_f2_n12_o = (float4(v_f2_n13_o, 0.0, 1.0)).x;
    float v_f2_n15_o = (float4(v_f2_n13_o, 0.0, 1.0)).y;
    float v_f2_n11_o = ((0.0 > 0.5) ? v_f2_n12_o : v_f2_n15_o);
    float v_f2_n10_o = saturate((1.0 - distance(v_f2_n11_o, Compare) / max(Width, 1e-5)) / max(1.0 - Sharpness, 1e-5));
    float2 v_f6_n14_o = (v_f3_n3_o + ((float2)(Offset)));
    float2 v_f6_n13_o = frac(v_f6_n14_o);
    float v_f6_n12_o = (float4(v_f6_n13_o, 0.0, 1.0)).x;
    float v_f6_n15_o = (float4(v_f6_n13_o, 0.0, 1.0)).y;
    float v_f6_n11_o = ((1.0 > 0.5) ? v_f6_n12_o : v_f6_n15_o);
    float v_f6_n10_o = saturate((1.0 - distance(v_f6_n11_o, Compare) / max(Width, 1e-5)) / max(1.0 - Sharpness, 1e-5));
    float2 v_f1_n3_o = float2(v_f2_n10_o, v_f6_n10_o);
    float4 b_n0 = CyTex(0, v_f1_n3_o);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n21 = CyTex(0, UV);
    float v_n20_o = (b_n21).w;
    return float4(v_n0_RGB, v_n20_o);
}
