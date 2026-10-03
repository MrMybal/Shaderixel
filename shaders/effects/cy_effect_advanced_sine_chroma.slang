//@name Advanced Sine Chroma
//@category CyTexture Effets
//@input Image
//@halo full
//@param Noise float 1.05 0.0 2.1 "Noise"
//@param Red float 1.05 0.0 2.1 "Red"
//@param Green float 1.0 0.0 2.0 "Green"
//@param Blue float 0.95 0.0 1.9 "Blue"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedSineChroma.M_CyEffect_AdvancedSineChroma: Native CyTexture advanced Effect preset with live GPU parameters.
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

float Noise_2_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_2_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_2_h(i), n100 = Noise_2_h(i + float3(1, 0, 0)), n010 = Noise_2_h(i + float3(0, 1, 0)), n110 = Noise_2_h(i + float3(1, 1, 0));
    float n001 = Noise_2_h(i + float3(0, 0, 1)), n101 = Noise_2_h(i + float3(1, 0, 1)), n011 = Noise_2_h(i + float3(0, 1, 1)), n111 = Noise_2_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_2(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_2_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 2.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float v_n9_o = lerp(-1.0, 1.0, saturate(Noise_2(float3(UV * ctx.size / 64.0, 0.0) * 0.01) * 0.5 + 0.5));
    float v_n7_o = (Noise * v_n9_o);
    float v_n6_o = sin((v_n7_o * 57.11987));
    float v_n5_o = (v_n6_o * Red);
    float2 v_n3_o = (UV * ((float2)(v_n5_o)));
    float v_n14_o = (v_n5_o - 1.0);
    float v_n13_o = (v_n14_o / 2.0);
    float v_n12_o = (v_n13_o * -1.0);
    float2 v_n2_o = (v_n3_o + ((float2)(v_n12_o)));
    float4 b_n1 = CyTex(0, v_n2_o);
    float3 v_n1_RGB = (b_n1).xyz;
    float v_n20_o = sin((v_n7_o * 62.83185));
    float v_n19_o = (v_n20_o * Green);
    float2 v_n17_o = (UV * ((float2)(v_n19_o)));
    float v_n24_o = (v_n19_o - 1.0);
    float v_n23_o = (v_n24_o / 2.0);
    float v_n22_o = (v_n23_o * -1.0);
    float2 v_n16_o = (v_n17_o + ((float2)(v_n22_o)));
    float4 b_n15 = CyTex(0, v_n16_o);
    float3 v_n15_RGB = (b_n15).xyz;
    float4 v_f1_n5_o = float4(v_n1_RGB, (v_n15_RGB).x);
    float v_n31_o = sin((v_n7_o * 69.81317));
    float v_n29_o = (Blue * v_n31_o);
    float2 v_n27_o = (UV * ((float2)(v_n29_o)));
    float v_n34_o = (v_n29_o - 1.0);
    float v_n33_o = (v_n34_o / 2.0);
    float v_n32_o = (v_n33_o * -1.0);
    float2 v_n26_o = (v_n27_o + ((float2)(v_n32_o)));
    float4 b_n25 = CyTex(0, v_n26_o);
    float3 v_n25_RGB = (b_n25).xyz;
    float4 b_n36 = CyTex(0, UV);
    float v_n35_o = (b_n36).w;
    return float4((v_f1_n5_o).xyz, v_n35_o);
}
