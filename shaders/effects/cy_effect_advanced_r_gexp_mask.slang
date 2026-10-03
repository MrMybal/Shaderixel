//@name Advanced R Gexp Mask
//@category CyTexture Effets
//@input Image
//@halo 0
//@param UV_X float 1.0 0.0 2.0 "UV X"
//@param UV_Y float 1.0 0.0 2.0 "UV Y"
//@param Radius float 0.5 0.0 1.0 "Radius"
//@param Density float 10.0 0.0 20.0 "Density"
//@param View_Mask float 0.0 0.0 1.0 "View Mask"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedRGexpMask.M_CyEffect_AdvancedRGexpMask: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float4 b_n2 = CyTex(0, UV);
    float3 v_n2_RGB = (b_n2).xyz;
    float2 v_f3_n3_o = float2(UV_X, UV_Y);
    float2 v_n4_o = (UV * v_f3_n3_o);
    float2 v_n9_o = (v_f3_n3_o / ((float2)(2.0)));
    float v_f1_n12_o = distance(v_n4_o, v_n9_o);
    float v_f1_n11_o = (v_f1_n12_o * 1.0);
    float v_f1_n10_o = (v_f1_n11_o / Radius);
    float v_f1_f2_n13_o = (v_f1_n10_o * Density);
    float v_f1_f2_n12_o = (v_f1_f2_n13_o * v_f1_f2_n13_o);
    float v_f1_f2_n11_o = ((1.0 > 0.5) ? v_f1_f2_n12_o : v_f1_f2_n13_o);
    float v_f1_f2_n9_o = pow(max(2.718282, 0.0), v_f1_f2_n11_o);
    float v_f1_f2_n7_o = (1.0 / v_f1_f2_n9_o);
    float v_f1_f2_n5_o = ((v_f1_n10_o == 0.0) ? 1.0 : ((v_f1_n10_o > 0.0) ? v_f1_f2_n7_o : 1.0));
    float v_f1_n15_o = (1.0 - v_f1_n10_o);
    float v_f1_f4_n13_o = (v_f1_n15_o * Density);
    float v_f1_f4_n12_o = (v_f1_f4_n13_o * v_f1_f4_n13_o);
    float v_f1_f4_n11_o = ((1.0 > 0.5) ? v_f1_f4_n12_o : v_f1_f4_n13_o);
    float v_f1_f4_n9_o = pow(max(2.718282, 0.0), v_f1_f4_n11_o);
    float v_f1_f4_n7_o = (1.0 / v_f1_f4_n9_o);
    float v_f1_f4_n5_o = ((v_f1_n15_o == 0.0) ? 1.0 : ((v_f1_n15_o > 0.0) ? v_f1_f4_n7_o : 1.0));
    float v_f1_n13_o = (1.0 - v_f1_f4_n5_o);
    float v_f1_n8_o = ((0.0 > 0.5) ? v_f1_f2_n5_o : v_f1_n13_o);
    float3 v_n1_o = (v_n2_RGB * ((float3)(v_f1_n8_o)));
    float3 v_n0_o = lerp(v_n1_o, ((float3)(v_f1_n8_o)), View_Mask);
    float4 b_n14 = CyTex(0, UV);
    float v_n13_o = (b_n14).w;
    return float4(v_n0_o, v_n13_o);
}
