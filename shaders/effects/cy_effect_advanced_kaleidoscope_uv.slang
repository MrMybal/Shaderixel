//@name Advanced Kaleidoscope UV
//@category CyTexture Effets
//@input Image
//@halo full
//@param Power_X float 8.0 0.0 16.0 "Power X"
//@param Power_Y float 8.0 0.0 16.0 "Power Y"
//@param Spiral float 0.0 0.0 1.0 "Spiral"
//@param Add_X float 0.0 0.0 1.0 "Add X"
//@param Add_Y float 0.0 0.0 1.0 "Add Y"
//@param Divisor float 8.0 0.0 16.0 "Divisor"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedKaleidoscopeUV.M_CyEffect_AdvancedKaleidoscopeUV: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_f5_n12(float x, float y)
{
return SXRet( (atan2(y,x)));
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_f5_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f5_n3_o = (v_f5_n4_o - ((float2)(1.0)));
    float v_f5_n13_o = (float4(v_f5_n3_o, 0.0, 1.0)).x;
    float v_f5_n14_o = (float4(v_f5_n3_o, 0.0, 1.0)).y;
    float b_f5_n12 = (Custom_f5_n12(v_f5_n13_o, v_f5_n14_o)).x;
    float v_f5_n11_o = (((float4)(b_f5_n12))).x;
    float v_f5_n10_o = (v_f5_n11_o / 6.283185);
    float v_f5_n9_o = frac(v_f5_n10_o);
    float2 v_f5_n19_o = (v_f5_n3_o * v_f5_n3_o);
    float v_f5_n18_o = (float4(v_f5_n19_o, 0.0, 1.0)).x;
    float v_f5_n20_o = (float4(v_f5_n19_o, 0.0, 1.0)).y;
    float v_f5_n17_o = (v_f5_n18_o + v_f5_n20_o);
    float v_f5_n16_o = sqrt(v_f5_n17_o);
    float2 v_f5_n8_o = float2(v_f5_n9_o, v_f5_n16_o);
    float2 v_f5_n21_o = float2(v_f5_n16_o, v_f5_n9_o);
    float2 v_f5_n7_o = ((1.0 > 0.5) ? v_f5_n8_o : v_f5_n21_o);
    float v_f7_n2_o = (float4(v_f5_n7_o, 0.0, 1.0)).x;
    float v_n14_o = pow(max(v_f7_n2_o, 0.0), Power_X);
    float2 v_n13_o = (((float2)(v_n14_o)) + v_f5_n7_o);
    float v_f8_n4_o = (float4(v_f5_n7_o, 0.0, 1.0)).y;
    float v_n18_o = pow(max(v_f8_n4_o, 0.0), Power_Y);
    float2 v_n17_o = (v_f5_n7_o + ((float2)(v_n18_o)));
    float4 v_f6_n3_o = float4(v_n13_o, v_n17_o);
    float4 v_n10_o = lerp(float4(v_f5_n7_o, 0.0, 1.0), v_f6_n3_o, Spiral);
    float v_f4_n2_o = (v_n10_o).x;
    float v_n8_o = (v_f4_n2_o + Add_X);
    float v_f9_n4_o = (v_n10_o).y;
    float v_n22_o = (v_f9_n4_o + Add_Y);
    float2 v_f3_n3_o = float2(v_n8_o, v_n22_o);
    float v_n24_o = (1.0 / Divisor);
    float2 v_n6_o = fmod(v_f3_n3_o, ((float2)(v_n24_o)));
    float v_f2_n2_o = (float4(v_n6_o, 0.0, 1.0)).x;
    float v_n4_o = (v_f2_n2_o / v_n24_o);
    float v_n26_o = (1.0 - v_n4_o);
    float v_n3_o = (v_n4_o - v_n26_o);
    float v_n2_o = abs(v_n3_o);
    float v_f10_n4_o = (float4(v_n6_o, 0.0, 1.0)).y;
    float v_n27_o = (v_f10_n4_o / v_n24_o);
    float2 v_f1_n3_o = float2(v_n2_o, v_n27_o);
    float4 b_n0 = CyTex(0, v_f1_n3_o);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n29 = CyTex(0, UV);
    float v_n28_o = (b_n29).w;
    return float4(v_n0_RGB, v_n28_o);
}
