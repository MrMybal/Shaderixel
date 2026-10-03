//@name Advanced Cir Flare
//@category CyTexture Dessin
//@generator
//@halo full
//@param Flare1 float 40.0 0.0 80.0 "Flare1"
//@param Flare2 float 600.0 0.0 1200.0 "Flare2"
//@param Power float 1.0 0.0 2.0 "Power"
//@param Tan0 float 1.0 0.0 2.0 "Tan0"
//@param Tan1 float 1.0 0.0 2.0 "Tan1"
//@param Luminance_from_Linear float 0.0 0.0 1.0 "Luminance from Linear"
//@param OutMul float 1.0 0.0 2.0 "Out Mul"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedCirFlare.M_CyDraw_AdvancedCirFlare: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_f7_n12(float x, float y)
{
return SXRet( (atan2(y,x)));
}

float Noise_10_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_10_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_10_h(i), n100 = Noise_10_h(i + float3(1, 0, 0)), n010 = Noise_10_h(i + float3(0, 1, 0)), n110 = Noise_10_h(i + float3(1, 1, 0));
    float n001 = Noise_10_h(i + float3(0, 0, 1)), n101 = Noise_10_h(i + float3(1, 0, 1)), n011 = Noise_10_h(i + float3(0, 1, 1)), n111 = Noise_10_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_10(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_10_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 3.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_11_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_11_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_11_h(i), n100 = Noise_11_h(i + float3(1, 0, 0)), n010 = Noise_11_h(i + float3(0, 1, 0)), n110 = Noise_11_h(i + float3(1, 1, 0));
    float n001 = Noise_11_h(i + float3(0, 0, 1)), n101 = Noise_11_h(i + float3(1, 0, 1)), n011 = Noise_11_h(i + float3(0, 1, 1)), n111 = Noise_11_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_11(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_11_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 3.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_12_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_12_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_12_h(i), n100 = Noise_12_h(i + float3(1, 0, 0)), n010 = Noise_12_h(i + float3(0, 1, 0)), n110 = Noise_12_h(i + float3(1, 1, 0));
    float n001 = Noise_12_h(i + float3(0, 0, 1)), n101 = Noise_12_h(i + float3(1, 0, 1)), n011 = Noise_12_h(i + float3(0, 1, 1)), n111 = Noise_12_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_12(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_12_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 3.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float4 Custom_f18_n12(float x, float y)
{
return SXRet( (atan2(y,x)));
}

float Noise_21_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_21_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_21_h(i), n100 = Noise_21_h(i + float3(1, 0, 0)), n010 = Noise_21_h(i + float3(0, 1, 0)), n110 = Noise_21_h(i + float3(1, 1, 0));
    float n001 = Noise_21_h(i + float3(0, 0, 1)), n101 = Noise_21_h(i + float3(1, 0, 1)), n011 = Noise_21_h(i + float3(0, 1, 1)), n111 = Noise_21_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_21(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_21_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 3.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_22_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_22_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_22_h(i), n100 = Noise_22_h(i + float3(1, 0, 0)), n010 = Noise_22_h(i + float3(0, 1, 0)), n110 = Noise_22_h(i + float3(1, 1, 0));
    float n001 = Noise_22_h(i + float3(0, 0, 1)), n101 = Noise_22_h(i + float3(1, 0, 1)), n011 = Noise_22_h(i + float3(0, 1, 1)), n111 = Noise_22_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_22(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_22_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 3.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_23_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_23_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_23_h(i), n100 = Noise_23_h(i + float3(1, 0, 0)), n010 = Noise_23_h(i + float3(0, 1, 0)), n110 = Noise_23_h(i + float3(1, 1, 0));
    float n001 = Noise_23_h(i + float3(0, 0, 1)), n101 = Noise_23_h(i + float3(1, 0, 1)), n011 = Noise_23_h(i + float3(0, 1, 1)), n111 = Noise_23_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_23(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_23_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 3.0; amp *= 0.5;
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
    float2 v_f3_n0_o = float2(0.5, 0.5);
    float v_f3_n12_o = distance(UV, v_f3_n0_o);
    float v_f3_n11_o = (v_f3_n12_o * 1.0);
    float2 v_f7_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f7_n3_o = (v_f7_n4_o - ((float2)(1.0)));
    float v_f7_n13_o = (float4(v_f7_n3_o, 0.0, 1.0)).x;
    float v_f7_n14_o = (float4(v_f7_n3_o, 0.0, 1.0)).y;
    float b_f7_n12 = (Custom_f7_n12(v_f7_n13_o, v_f7_n14_o)).x;
    float v_f7_n11_o = (((float4)(b_f7_n12))).x;
    float v_f7_n10_o = (v_f7_n11_o / 6.283185);
    float v_f7_n9_o = frac(v_f7_n10_o);
    float v_n10_o = (v_f7_n9_o * Flare1);
    float v_f6_n2_o = (((float4)(v_n10_o))).x;
    float v_f9_n4_o = (((float4)(v_n10_o))).y;
    float2 v_f8_n5_o = float2(0.0, v_f9_n4_o);
    float3 v_f8_n4_o = float3(v_f8_n5_o, 0.0);
    float v_n13_o = lerp(0.0, 1.0, saturate(Noise_10(v_f8_n4_o * 1.0) * 0.5 + 0.5));
    float2 v_f5_n5_o = float2(v_f6_n2_o, v_n13_o);
    float3 v_f5_n4_o = float3(v_f5_n5_o, 0.0);
    float v_n7_o = lerp(0.1, 0.8, saturate(Noise_11(v_f5_n4_o * 0.3) * 0.5 + 0.5));
    float v_f3_n10_o = (v_f3_n11_o / v_n7_o);
    float v_n15_o = lerp(0.5, 1.0, saturate(Noise_12(v_f5_n4_o * 1.0) * 0.5 + 0.5));
    float v_f3_f4_n13_o = (v_f3_n10_o * v_n15_o);
    float v_f3_f4_n12_o = (v_f3_f4_n13_o * v_f3_f4_n13_o);
    float v_f3_f4_n11_o = ((1.0 > 0.5) ? v_f3_f4_n12_o : v_f3_f4_n13_o);
    float v_f3_f4_n9_o = pow(max(2.718282, 0.0), v_f3_f4_n11_o);
    float v_f3_f4_n7_o = (1.0 / v_f3_f4_n9_o);
    float v_f3_f4_n5_o = ((v_f3_n10_o == 0.0) ? 1.0 : ((v_f3_n10_o > 0.0) ? v_f3_f4_n7_o : 1.0));
    float v_f3_n15_o = (1.0 - v_f3_n10_o);
    float v_f3_f13_n13_o = (v_f3_n15_o * v_n15_o);
    float v_f3_f13_n12_o = (v_f3_f13_n13_o * v_f3_f13_n13_o);
    float v_f3_f13_n11_o = ((1.0 > 0.5) ? v_f3_f13_n12_o : v_f3_f13_n13_o);
    float v_f3_f13_n9_o = pow(max(2.718282, 0.0), v_f3_f13_n11_o);
    float v_f3_f13_n7_o = (1.0 / v_f3_f13_n9_o);
    float v_f3_f13_n5_o = ((v_f3_n15_o == 0.0) ? 1.0 : ((v_f3_n15_o > 0.0) ? v_f3_f13_n7_o : 1.0));
    float v_f3_n13_o = (1.0 - v_f3_f13_n5_o);
    float v_f3_n8_o = ((0.0 > 0.5) ? v_f3_f4_n5_o : v_f3_n13_o);
    float v_n5_o = (v_f3_n8_o * v_n13_o);
    float2 v_f14_n0_o = float2(0.5, 0.5);
    float v_f14_n12_o = distance(UV, v_f14_n0_o);
    float v_f14_n11_o = (v_f14_n12_o * 1.0);
    float2 v_f18_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f18_n3_o = (v_f18_n4_o - ((float2)(1.0)));
    float v_f18_n13_o = (float4(v_f18_n3_o, 0.0, 1.0)).x;
    float v_f18_n14_o = (float4(v_f18_n3_o, 0.0, 1.0)).y;
    float b_f18_n12 = (Custom_f18_n12(v_f18_n13_o, v_f18_n14_o)).x;
    float v_f18_n11_o = (((float4)(b_f18_n12))).x;
    float v_f18_n10_o = (v_f18_n11_o / 6.283185);
    float v_f18_n9_o = frac(v_f18_n10_o);
    float v_n21_o = (v_f18_n9_o * Flare2);
    float v_f17_n2_o = (((float4)(v_n21_o))).x;
    float v_f20_n4_o = (((float4)(v_n21_o))).y;
    float2 v_f19_n5_o = float2(0.0, v_f20_n4_o);
    float3 v_f19_n4_o = float3(v_f19_n5_o, 0.0);
    float v_n24_o = lerp(0.0, 1.0, saturate(Noise_21(v_f19_n4_o * 1.0) * 0.5 + 0.5));
    float2 v_f16_n5_o = float2(v_f17_n2_o, v_n24_o);
    float3 v_f16_n4_o = float3(v_f16_n5_o, 0.0);
    float v_n18_o = lerp(0.1, 0.8, saturate(Noise_22(v_f16_n4_o * 0.3) * 0.5 + 0.5));
    float v_f14_n10_o = (v_f14_n11_o / v_n18_o);
    float v_n26_o = lerp(0.5, 1.0, saturate(Noise_23(v_f16_n4_o * 1.0) * 0.5 + 0.5));
    float v_f14_f15_n13_o = (v_f14_n10_o * v_n26_o);
    float v_f14_f15_n12_o = (v_f14_f15_n13_o * v_f14_f15_n13_o);
    float v_f14_f15_n11_o = ((1.0 > 0.5) ? v_f14_f15_n12_o : v_f14_f15_n13_o);
    float v_f14_f15_n9_o = pow(max(2.718282, 0.0), v_f14_f15_n11_o);
    float v_f14_f15_n7_o = (1.0 / v_f14_f15_n9_o);
    float v_f14_f15_n5_o = ((v_f14_n10_o == 0.0) ? 1.0 : ((v_f14_n10_o > 0.0) ? v_f14_f15_n7_o : 1.0));
    float v_f14_n15_o = (1.0 - v_f14_n10_o);
    float v_f14_f24_n13_o = (v_f14_n15_o * v_n26_o);
    float v_f14_f24_n12_o = (v_f14_f24_n13_o * v_f14_f24_n13_o);
    float v_f14_f24_n11_o = ((1.0 > 0.5) ? v_f14_f24_n12_o : v_f14_f24_n13_o);
    float v_f14_f24_n9_o = pow(max(2.718282, 0.0), v_f14_f24_n11_o);
    float v_f14_f24_n7_o = (1.0 / v_f14_f24_n9_o);
    float v_f14_f24_n5_o = ((v_f14_n15_o == 0.0) ? 1.0 : ((v_f14_n15_o > 0.0) ? v_f14_f24_n7_o : 1.0));
    float v_f14_n13_o = (1.0 - v_f14_f24_n5_o);
    float v_f14_n8_o = ((0.0 > 0.5) ? v_f14_f15_n5_o : v_f14_n13_o);
    float v_n16_o = (v_f14_n8_o * v_n24_o);
    float v_n4_o = (v_n5_o + v_n16_o);
    float v_n3_o = pow(max(v_n4_o, 0.0), Power);
    float v_f2_n7_o = (v_n3_o * Tan0);
    float v_f2_n13_o = (Tan0 + Tan1);
    float v_f2_n12_o = (v_f2_n13_o - 2.0);
    float v_f2_n11_o = (v_n3_o * v_f2_n12_o);
    float v_f2_n10_o = (v_n3_o * v_f2_n11_o);
    float v_f2_n9_o = (v_n3_o * v_f2_n10_o);
    float v_f2_n19_o = (3.0 - Tan1);
    float v_f2_n18_o = (v_f2_n19_o - 2.0);
    float v_f2_n17_o = (v_f2_n18_o * Tan0);
    float v_f2_n16_o = (v_n3_o * v_f2_n17_o);
    float v_f2_n15_o = (v_n3_o * v_f2_n16_o);
    float v_f2_n8_o = (v_f2_n9_o + v_f2_n15_o);
    float v_f2_n6_o = (v_f2_n7_o + v_f2_n8_o);
    float v_f1_n10_o = (v_f2_n6_o * 100.0);
    float v_f1_n18_o = pow(max(v_f2_n6_o, 0.0), 0.454545);
    float v_f1_n17_o = ((Luminance_from_Linear > 0.5) ? v_f1_n18_o : v_f2_n6_o);
    float3 v_f1_n16_o = lerp(((float3)(v_f1_n17_o)), (float3)dot(((float3)(v_f1_n17_o)), float3(0.3, 0.59, 0.11)), 1.0);
    float3 v_f1_n15_o = pow(max(v_f1_n16_o, 0.0), ((float3)(100.0)));
    float3 v_f1_n14_o = (((float3)(100.0)) * v_f1_n15_o);
    float3 v_f1_n13_o = (((float3)(1.0)) - v_f1_n14_o);
    float3 v_f1_n12_o = (((float3)(100.0)) + v_f1_n13_o);
    float3 v_f1_n6_o = float3(1.0, 1.0, 1.0);
    float3 v_f1_n20_o = lerp(((float3)(1.0)), v_f1_n6_o, v_f1_n15_o);
    float3 v_f1_n19_o = (((float3)(1.0)) * v_f1_n20_o);
    float3 v_f1_n11_o = (v_f1_n12_o / v_f1_n19_o);
    float3 v_f1_n9_o = (((float3)(v_f1_n10_o)) / v_f1_n11_o);
    float3 v_n0_o = (v_f1_n9_o * ((float3)(OutMul)));
    float v_n34_o = (float4(v_n0_o, 1.0)).x;
    float v_n35_o = (float4(v_n0_o, 1.0)).y;
    float v_n33_o = max(v_n34_o, v_n35_o);
    float v_n36_o = (float4(v_n0_o, 1.0)).z;
    float v_n32_o = max(v_n33_o, v_n36_o);
    return float4(v_n0_o, v_n32_o);
}
