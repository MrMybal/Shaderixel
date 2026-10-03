//@name Advanced Dia Gra Mask
//@category Traitements
//@input Image
//@halo 0
//@param UV_X float 1.0 0.0 2.0 "UV X"
//@param UV_Y float 1.0 0.0 2.0 "UV Y"
//@param Falloff float 1.0 0.0 2.0 "Falloff"
//@param View_Mask float 0.0 0.0 1.0 "View Mask"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedDiaGraMask.M_CyEffect_AdvancedDiaGraMask: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float2 v_f2_n3_o = float2(UV_X, UV_Y);
    float2 v_n10_o = (UV * v_f2_n3_o);
    float v_f1_n3_o = (float4(v_n10_o, 0.0, 1.0)).x;
    float v_n15_o = -0.5;
    float v_n8_o = (v_f1_n3_o + v_n15_o);
    float v_n7_o = (v_n8_o * 2.0);
    float v_n6_o = abs(v_n7_o);
    float v_n5_o = (1.0 - v_n6_o);
    float v_f3_n5_o = (float4(v_n10_o, 0.0, 1.0)).y;
    float v_n20_o = (v_f3_n5_o + v_n15_o);
    float v_n19_o = (v_n20_o * 2.0);
    float v_n18_o = abs(v_n19_o);
    float v_n17_o = (1.0 - v_n18_o);
    float v_n4_o = (v_n5_o * v_n17_o);
    float v_n3_o = pow(max(v_n4_o, 0.0), Falloff);
    float3 v_n1_o = (v_n2_RGB * ((float3)(v_n3_o)));
    float3 v_n0_o = lerp(v_n1_o, ((float3)(v_n3_o)), View_Mask);
    float4 b_n24 = CyTex(0, UV);
    float v_n23_o = (b_n24).w;
    return float4(v_n0_o, v_n23_o);
}
