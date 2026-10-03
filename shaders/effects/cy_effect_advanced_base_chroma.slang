//@name Advanced Base Chroma
//@category CyTexture Effets
//@input Image
//@halo full
//@param Red float 1.05 0.0 2.1 "Red"
//@param Green float 1.0 0.0 2.0 "Green"
//@param Blue float 0.95 0.0 1.9 "Blue"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedBaseChroma.M_CyEffect_AdvancedBaseChroma: Native CyTexture advanced Effect preset with live GPU parameters.
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
    float2 v_n3_o = (UV * ((float2)(Red)));
    float v_n8_o = (Red - 1.0);
    float v_n7_o = (v_n8_o / 2.0);
    float v_n6_o = (v_n7_o * -1.0);
    float2 v_n2_o = (v_n3_o + ((float2)(v_n6_o)));
    float4 b_n1 = CyTex(0, v_n2_o);
    float3 v_n1_RGB = (b_n1).xyz;
    float2 v_n11_o = (UV * ((float2)(Green)));
    float v_n16_o = (Green - 1.0);
    float v_n15_o = (v_n16_o / 2.0);
    float v_n14_o = (v_n15_o * -1.0);
    float2 v_n10_o = (v_n11_o + ((float2)(v_n14_o)));
    float4 b_n9 = CyTex(0, v_n10_o);
    float3 v_n9_RGB = (b_n9).xyz;
    float4 v_f1_n5_o = float4(v_n1_RGB, (v_n9_RGB).x);
    float2 v_n19_o = (UV * ((float2)(Blue)));
    float v_n24_o = (Blue - 1.0);
    float v_n23_o = (v_n24_o / 2.0);
    float v_n22_o = (v_n23_o * -1.0);
    float2 v_n18_o = (v_n19_o + ((float2)(v_n22_o)));
    float4 b_n17 = CyTex(0, v_n18_o);
    float3 v_n17_RGB = (b_n17).xyz;
    float4 b_n26 = CyTex(0, UV);
    float v_n25_o = (b_n26).w;
    return float4((v_f1_n5_o).xyz, v_n25_o);
}
