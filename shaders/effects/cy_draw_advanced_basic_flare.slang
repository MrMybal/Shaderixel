//@name Advanced Basic Flare
//@category Dessin
//@generator
//@halo full
//@param Divisor float 4.0 0.0 8.0 "Divisor"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedBasicFlare.M_CyDraw_AdvancedBasicFlare: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_f4_n12(float x, float y)
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
    float2 v_f1_n0_o = float2(0.5, 0.5);
    float v_f1_n12_o = distance(UV, v_f1_n0_o);
    float v_f1_n11_o = (v_f1_n12_o * 1.0);
    float v_f1_n10_o = (v_f1_n11_o / 0.5);
    float2 v_f4_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f4_n3_o = (v_f4_n4_o - ((float2)(1.0)));
    float v_f4_n13_o = (float4(v_f4_n3_o, 0.0, 1.0)).x;
    float v_f4_n14_o = (float4(v_f4_n3_o, 0.0, 1.0)).y;
    float b_f4_n12 = (Custom_f4_n12(v_f4_n13_o, v_f4_n14_o)).x;
    float v_f4_n11_o = (((float4)(b_f4_n12))).x;
    float v_f4_n10_o = (v_f4_n11_o / 6.283185);
    float v_f4_n9_o = frac(v_f4_n10_o);
    float2 v_f4_n19_o = (v_f4_n3_o * v_f4_n3_o);
    float v_f4_n18_o = (float4(v_f4_n19_o, 0.0, 1.0)).x;
    float v_f4_n20_o = (float4(v_f4_n19_o, 0.0, 1.0)).y;
    float v_f4_n17_o = (v_f4_n18_o + v_f4_n20_o);
    float v_f4_n16_o = sqrt(v_f4_n17_o);
    float2 v_f4_n8_o = float2(v_f4_n9_o, v_f4_n16_o);
    float2 v_f4_n21_o = float2(v_f4_n16_o, v_f4_n9_o);
    float2 v_f4_n7_o = ((1.0 > 0.5) ? v_f4_n8_o : v_f4_n21_o);
    float v_n6_o = (1.0 / Divisor);
    float2 v_n4_o = fmod(v_f4_n7_o, ((float2)(v_n6_o)));
    float2 v_n3_o = (v_n4_o / ((float2)(v_n6_o)));
    float2 v_n2_o = sin((v_n3_o * 6.283185));
    float v_f3_n2_o = (float4(v_n2_o, 0.0, 1.0)).x;
    float v_f1_f2_n13_o = (v_f1_n10_o * v_f3_n2_o);
    float v_f1_f2_n12_o = (v_f1_f2_n13_o * v_f1_f2_n13_o);
    float v_f1_f2_n11_o = ((1.0 > 0.5) ? v_f1_f2_n12_o : v_f1_f2_n13_o);
    float v_f1_f2_n9_o = pow(max(2.718282, 0.0), v_f1_f2_n11_o);
    float v_f1_f2_n7_o = (1.0 / v_f1_f2_n9_o);
    float v_f1_f2_n5_o = ((v_f1_n10_o == 0.0) ? 1.0 : ((v_f1_n10_o > 0.0) ? v_f1_f2_n7_o : 1.0));
    float v_f1_n15_o = (1.0 - v_f1_n10_o);
    float v_f1_f5_n13_o = (v_f1_n15_o * v_f3_n2_o);
    float v_f1_f5_n12_o = (v_f1_f5_n13_o * v_f1_f5_n13_o);
    float v_f1_f5_n11_o = ((1.0 > 0.5) ? v_f1_f5_n12_o : v_f1_f5_n13_o);
    float v_f1_f5_n9_o = pow(max(2.718282, 0.0), v_f1_f5_n11_o);
    float v_f1_f5_n7_o = (1.0 / v_f1_f5_n9_o);
    float v_f1_f5_n5_o = ((v_f1_n15_o == 0.0) ? 1.0 : ((v_f1_n15_o > 0.0) ? v_f1_f5_n7_o : 1.0));
    float v_f1_n13_o = (1.0 - v_f1_f5_n5_o);
    float v_f1_n8_o = ((0.0 > 0.5) ? v_f1_f2_n5_o : v_f1_n13_o);
    float v_n10_o = (((float4)(v_f1_n8_o))).x;
    float v_n11_o = (((float4)(v_f1_n8_o))).y;
    float v_n9_o = max(v_n10_o, v_n11_o);
    float v_n12_o = (((float4)(v_f1_n8_o))).z;
    float v_n8_o = max(v_n9_o, v_n12_o);
    return float4(((float3)(v_f1_n8_o)), v_n8_o);
}
