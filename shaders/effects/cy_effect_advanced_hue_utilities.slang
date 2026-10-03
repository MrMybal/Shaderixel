//@name Advanced HUE Utilities
//@category CyTexture Effets
//@input Image
//@halo 0
//@param HUE_shift float 0.0 0.0 1.0 "HUE shift"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedHUEUtilities.M_CyEffect_AdvancedHUEUtilities: Native CyTexture advanced Effect preset with live GPU parameters.
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

float3 RotateAboutAxis_2(float3 axis, float angle, float3 pivot, float3 pos)
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
    float4 b_n2 = CyTex(0, UV);
    float3 v_n2_RGB = (b_n2).xyz;
    float3 v_f1_n5_o = RotateAboutAxis_2(v_f1_n6_o, HUE_shift, ((float3)(0.0)), v_n2_RGB);
    float3 v_f1_n4_o = (v_f1_n5_o + v_n2_RGB);
    float4 b_n4 = CyTex(0, UV);
    float v_n3_o = (b_n4).w;
    return float4(v_f1_n4_o, v_n3_o);
}
