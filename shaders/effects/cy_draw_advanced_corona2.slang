//@name Advanced Corona2
//@category Dessin
//@generator
//@halo full
//@param Flare float 1.0 0.0 2.0 "Flare"
//@param CoronaNoise_Radius float 0.489 0.0 1.0 "Corona Noise Radius"
//@param Corona_Shift float 0.489 0.0 1.0 "Corona Shift"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedCorona2.M_CyDraw_AdvancedCorona2: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_f6_n12(float x, float y)
{
return SXRet( (atan2(y,x)));
}

float Noise_7_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_7_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_7_h(i), n100 = Noise_7_h(i + float3(1, 0, 0)), n010 = Noise_7_h(i + float3(0, 1, 0)), n110 = Noise_7_h(i + float3(1, 1, 0));
    float n001 = Noise_7_h(i + float3(0, 0, 1)), n101 = Noise_7_h(i + float3(1, 0, 1)), n011 = Noise_7_h(i + float3(0, 1, 1)), n111 = Noise_7_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_7(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_7_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 2.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_9_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_9_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_9_h(i), n100 = Noise_9_h(i + float3(1, 0, 0)), n010 = Noise_9_h(i + float3(0, 1, 0)), n110 = Noise_9_h(i + float3(1, 1, 0));
    float n001 = Noise_9_h(i + float3(0, 0, 1)), n101 = Noise_9_h(i + float3(1, 0, 1)), n011 = Noise_9_h(i + float3(0, 1, 1)), n111 = Noise_9_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_9(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_9_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 2.0; amp *= 0.5;
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
        p *= 2.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_14_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_14_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_14_h(i), n100 = Noise_14_h(i + float3(1, 0, 0)), n010 = Noise_14_h(i + float3(0, 1, 0)), n110 = Noise_14_h(i + float3(1, 1, 0));
    float n001 = Noise_14_h(i + float3(0, 0, 1)), n101 = Noise_14_h(i + float3(1, 0, 1)), n011 = Noise_14_h(i + float3(0, 1, 1)), n111 = Noise_14_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_14(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_14_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 2.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_16_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_16_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_16_h(i), n100 = Noise_16_h(i + float3(1, 0, 0)), n010 = Noise_16_h(i + float3(0, 1, 0)), n110 = Noise_16_h(i + float3(1, 1, 0));
    float n001 = Noise_16_h(i + float3(0, 0, 1)), n101 = Noise_16_h(i + float3(1, 0, 1)), n011 = Noise_16_h(i + float3(0, 1, 1)), n111 = Noise_16_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_16(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_16_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 2.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_18_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_18_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_18_h(i), n100 = Noise_18_h(i + float3(1, 0, 0)), n010 = Noise_18_h(i + float3(0, 1, 0)), n110 = Noise_18_h(i + float3(1, 1, 0));
    float n001 = Noise_18_h(i + float3(0, 0, 1)), n101 = Noise_18_h(i + float3(1, 0, 1)), n011 = Noise_18_h(i + float3(0, 1, 1)), n111 = Noise_18_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_18(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_18_one(p);
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
    float2 v_f6_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f6_n3_o = (v_f6_n4_o - ((float2)(1.0)));
    float v_f6_n13_o = (float4(v_f6_n3_o, 0.0, 1.0)).x;
    float v_f6_n14_o = (float4(v_f6_n3_o, 0.0, 1.0)).y;
    float b_f6_n12 = (Custom_f6_n12(v_f6_n13_o, v_f6_n14_o)).x;
    float v_f6_n11_o = (((float4)(b_f6_n12))).x;
    float v_f6_n10_o = (v_f6_n11_o / 6.283185);
    float v_f6_n9_o = frac(v_f6_n10_o);
    float v_n13_o = (v_f6_n9_o * Flare);
    float v_f5_n2_o = (((float4)(v_n13_o))).x;
    float2 v_f4_n5_o = float2(v_f5_n2_o, 0.0);
    float3 v_f4_n4_o = float3(v_f4_n5_o, 0.0);
    float v_n10_o = lerp(-1.0, 1.0, saturate(Noise_7(v_f4_n4_o * 50.0) * 0.5 + 0.5));
    float2 v_f8_n5_o = float2(v_f5_n2_o, v_f5_n2_o);
    float3 v_f8_n4_o = float3(v_f8_n5_o, 0.0);
    float v_n16_o = lerp(-1.0, 1.0, saturate(Noise_9(v_f8_n4_o * 0.9) * 0.5 + 0.5));
    float v_n9_o = (v_n10_o + v_n16_o);
    float v_f11_n4_o = (((float4)(v_n13_o))).y;
    float2 v_f10_n5_o = float2(0.0, v_f11_n4_o);
    float3 v_f10_n4_o = float3(v_f10_n5_o, 0.0);
    float v_n18_o = lerp(-1.0, 1.0, saturate(Noise_12(v_f10_n4_o * 20.0) * 0.5 + 0.5));
    float v_n8_o = (v_n9_o + v_n18_o);
    float v_n7_o = (v_n8_o / 3.0);
    float2 v_f13_n5_o = float2(v_f11_n4_o, v_f11_n4_o);
    float3 v_f13_n4_o = float3(v_f13_n5_o, 0.0);
    float v_n23_o = lerp(-1.0, 1.0, saturate(Noise_14(v_f13_n4_o * 20.0) * 0.5 + 0.5));
    float2 v_f15_n5_o = float2(0.0, v_f11_n4_o);
    float3 v_f15_n4_o = float3(v_f15_n5_o, v_f11_n4_o);
    float v_n25_o = lerp(-1.0, 1.0, saturate(Noise_16(v_f15_n4_o * 8.0) * 0.5 + 0.5));
    float v_n22_o = (v_n23_o + v_n25_o);
    float2 v_f17_n5_o = float2(v_f5_n2_o, 0.0);
    float3 v_f17_n4_o = float3(v_f17_n5_o, v_f5_n2_o);
    float v_n27_o = lerp(-1.0, 1.0, saturate(Noise_18(v_f17_n4_o * 80.0) * 0.5 + 0.5));
    float v_n21_o = (v_n22_o + v_n27_o);
    float v_n20_o = (v_n21_o / 3.0);
    float2 v_f3_n3_o = float2(v_n7_o, v_n20_o);
    float2 v_n5_o = (v_f3_n3_o * ((float2)(0.01)));
    float2 v_n4_o = (v_n5_o + UV);
    float2 v_f1_n0_o = float2(0.5, 0.5);
    float v_f1_n12_o = distance(v_n4_o, v_f1_n0_o);
    float v_f1_n11_o = (v_f1_n12_o * 1.0);
    float v_f1_n10_o = (v_f1_n11_o / 0.51);
    float v_f1_f2_n13_o = (v_f1_n10_o * 8.0);
    float v_f1_f2_n12_o = (v_f1_f2_n13_o * v_f1_f2_n13_o);
    float v_f1_f2_n11_o = ((1.0 > 0.5) ? v_f1_f2_n12_o : v_f1_f2_n13_o);
    float v_f1_f2_n9_o = pow(max(2.718282, 0.0), v_f1_f2_n11_o);
    float v_f1_f2_n7_o = (1.0 / v_f1_f2_n9_o);
    float v_f1_f2_n5_o = ((v_f1_n10_o == 0.0) ? 1.0 : ((v_f1_n10_o > 0.0) ? v_f1_f2_n7_o : 1.0));
    float v_f1_n15_o = (1.0 - v_f1_n10_o);
    float v_f1_f19_n13_o = (v_f1_n15_o * 8.0);
    float v_f1_f19_n12_o = (v_f1_f19_n13_o * v_f1_f19_n13_o);
    float v_f1_f19_n11_o = ((1.0 > 0.5) ? v_f1_f19_n12_o : v_f1_f19_n13_o);
    float v_f1_f19_n9_o = pow(max(2.718282, 0.0), v_f1_f19_n11_o);
    float v_f1_f19_n7_o = (1.0 / v_f1_f19_n9_o);
    float v_f1_f19_n5_o = ((v_f1_n15_o == 0.0) ? 1.0 : ((v_f1_n15_o > 0.0) ? v_f1_f19_n7_o : 1.0));
    float v_f1_n13_o = (1.0 - v_f1_f19_n5_o);
    float v_f1_n8_o = ((0.0 > 0.5) ? v_f1_f2_n5_o : v_f1_n13_o);
    float2 v_n34_o = (v_f3_n3_o * ((float2)(0.002)));
    float2 v_n33_o = (v_n34_o + UV);
    float2 v_f20_n0_o = float2(0.5, 0.5);
    float v_f20_n12_o = distance(v_n33_o, v_f20_n0_o);
    float v_f20_n11_o = (v_f20_n12_o * 1.0);
    float v_f20_n10_o = (v_f20_n11_o / CoronaNoise_Radius);
    float v_f20_f21_n13_o = (v_f20_n10_o * 16.0);
    float v_f20_f21_n12_o = (v_f20_f21_n13_o * v_f20_f21_n13_o);
    float v_f20_f21_n11_o = ((1.0 > 0.5) ? v_f20_f21_n12_o : v_f20_f21_n13_o);
    float v_f20_f21_n9_o = pow(max(2.718282, 0.0), v_f20_f21_n11_o);
    float v_f20_f21_n7_o = (1.0 / v_f20_f21_n9_o);
    float v_f20_f21_n5_o = ((v_f20_n10_o == 0.0) ? 1.0 : ((v_f20_n10_o > 0.0) ? v_f20_f21_n7_o : 1.0));
    float v_f20_n15_o = (1.0 - v_f20_n10_o);
    float v_f20_f22_n13_o = (v_f20_n15_o * 16.0);
    float v_f20_f22_n12_o = (v_f20_f22_n13_o * v_f20_f22_n13_o);
    float v_f20_f22_n11_o = ((1.0 > 0.5) ? v_f20_f22_n12_o : v_f20_f22_n13_o);
    float v_f20_f22_n9_o = pow(max(2.718282, 0.0), v_f20_f22_n11_o);
    float v_f20_f22_n7_o = (1.0 / v_f20_f22_n9_o);
    float v_f20_f22_n5_o = ((v_f20_n15_o == 0.0) ? 1.0 : ((v_f20_n15_o > 0.0) ? v_f20_f22_n7_o : 1.0));
    float v_f20_n13_o = (1.0 - v_f20_f22_n5_o);
    float v_f20_n8_o = ((0.0 > 0.5) ? v_f20_f21_n5_o : v_f20_n13_o);
    float v_n2_o = (v_f1_n8_o - v_f20_n8_o);
    float v_n1_o = clamp(v_n2_o, 0.0, 1.0);
    float2 v_n41_o = (v_f3_n3_o * ((float2)(0.002)));
    float2 v_n40_o = (v_n41_o + UV);
    float2 v_f23_n0_o = float2(0.5, 0.5);
    float v_f23_n12_o = distance(v_n40_o, v_f23_n0_o);
    float v_f23_n11_o = (v_f23_n12_o * 1.0);
    float v_f23_n10_o = (v_f23_n11_o / 0.49);
    float v_f23_f24_n13_o = (v_f23_n10_o * 100.0);
    float v_f23_f24_n12_o = (v_f23_f24_n13_o * v_f23_f24_n13_o);
    float v_f23_f24_n11_o = ((1.0 > 0.5) ? v_f23_f24_n12_o : v_f23_f24_n13_o);
    float v_f23_f24_n9_o = pow(max(2.718282, 0.0), v_f23_f24_n11_o);
    float v_f23_f24_n7_o = (1.0 / v_f23_f24_n9_o);
    float v_f23_f24_n5_o = ((v_f23_n10_o == 0.0) ? 1.0 : ((v_f23_n10_o > 0.0) ? v_f23_f24_n7_o : 1.0));
    float v_f23_n15_o = (1.0 - v_f23_n10_o);
    float v_f23_f25_n13_o = (v_f23_n15_o * 100.0);
    float v_f23_f25_n12_o = (v_f23_f25_n13_o * v_f23_f25_n13_o);
    float v_f23_f25_n11_o = ((1.0 > 0.5) ? v_f23_f25_n12_o : v_f23_f25_n13_o);
    float v_f23_f25_n9_o = pow(max(2.718282, 0.0), v_f23_f25_n11_o);
    float v_f23_f25_n7_o = (1.0 / v_f23_f25_n9_o);
    float v_f23_f25_n5_o = ((v_f23_n15_o == 0.0) ? 1.0 : ((v_f23_n15_o > 0.0) ? v_f23_f25_n7_o : 1.0));
    float v_f23_n13_o = (1.0 - v_f23_f25_n5_o);
    float v_f23_n8_o = ((0.0 > 0.5) ? v_f23_f24_n5_o : v_f23_n13_o);
    float2 v_n46_o = (v_f3_n3_o * ((float2)(0.002)));
    float2 v_n45_o = (v_n46_o + UV);
    float2 v_f26_n0_o = float2(0.5, 0.5);
    float v_f26_n12_o = distance(v_n45_o, v_f26_n0_o);
    float v_f26_n11_o = (v_f26_n12_o * 1.0);
    float v_f26_n10_o = (v_f26_n11_o / Corona_Shift);
    float v_f26_f27_n13_o = (v_f26_n10_o * 100.0);
    float v_f26_f27_n12_o = (v_f26_f27_n13_o * v_f26_f27_n13_o);
    float v_f26_f27_n11_o = ((1.0 > 0.5) ? v_f26_f27_n12_o : v_f26_f27_n13_o);
    float v_f26_f27_n9_o = pow(max(2.718282, 0.0), v_f26_f27_n11_o);
    float v_f26_f27_n7_o = (1.0 / v_f26_f27_n9_o);
    float v_f26_f27_n5_o = ((v_f26_n10_o == 0.0) ? 1.0 : ((v_f26_n10_o > 0.0) ? v_f26_f27_n7_o : 1.0));
    float v_f26_n15_o = (1.0 - v_f26_n10_o);
    float v_f26_f28_n13_o = (v_f26_n15_o * 100.0);
    float v_f26_f28_n12_o = (v_f26_f28_n13_o * v_f26_f28_n13_o);
    float v_f26_f28_n11_o = ((1.0 > 0.5) ? v_f26_f28_n12_o : v_f26_f28_n13_o);
    float v_f26_f28_n9_o = pow(max(2.718282, 0.0), v_f26_f28_n11_o);
    float v_f26_f28_n7_o = (1.0 / v_f26_f28_n9_o);
    float v_f26_f28_n5_o = ((v_f26_n15_o == 0.0) ? 1.0 : ((v_f26_n15_o > 0.0) ? v_f26_f28_n7_o : 1.0));
    float v_f26_n13_o = (1.0 - v_f26_f28_n5_o);
    float v_f26_n8_o = ((0.0 > 0.5) ? v_f26_f27_n5_o : v_f26_n13_o);
    float v_n38_o = (v_f23_n8_o - v_f26_n8_o);
    float v_n37_o = clamp(v_n38_o, 0.0, 1.0);
    float v_n0_o = (v_n1_o + v_n37_o);
    float v_n51_o = (((float4)(v_n0_o))).x;
    float v_n52_o = (((float4)(v_n0_o))).y;
    float v_n50_o = max(v_n51_o, v_n52_o);
    float v_n53_o = (((float4)(v_n0_o))).z;
    float v_n49_o = max(v_n50_o, v_n53_o);
    return float4(((float3)(v_n0_o)), v_n49_o);
}
