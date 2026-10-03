//@name Advanced Flare2
//@category CyTexture Dessin
//@generator
//@halo full
//@param Fmod float 0.2 0.0 1.0 "Fmod"
//@param F1 float 10.0 0.0 20.0 "F1"
//@param F2 float 2.0 0.0 4.0 "F2"
//@param Radius float 0.5 0.0 1.0 "Radius"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Draw/M_CyDraw_AdvancedFlare2.M_CyDraw_AdvancedFlare2: Native CyTexture advanced Draw preset with live GPU parameters.
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

float4 Custom_f1_n12(float x, float y)
{
return SXRet( (atan2(y,x)));
}

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
        p *= 3.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_3_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_3_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_3_h(i), n100 = Noise_3_h(i + float3(1, 0, 0)), n010 = Noise_3_h(i + float3(0, 1, 0)), n110 = Noise_3_h(i + float3(1, 1, 0));
    float n001 = Noise_3_h(i + float3(0, 0, 1)), n101 = Noise_3_h(i + float3(1, 0, 1)), n011 = Noise_3_h(i + float3(0, 1, 1)), n111 = Noise_3_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_3(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 10; ++l)
    {
        float v = Noise_3_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 4.0; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}

float Noise_6_h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
float Noise_6_one(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);
    float n000 = Noise_6_h(i), n100 = Noise_6_h(i + float3(1, 0, 0)), n010 = Noise_6_h(i + float3(0, 1, 0)), n110 = Noise_6_h(i + float3(1, 1, 0));
    float n001 = Noise_6_h(i + float3(0, 0, 1)), n101 = Noise_6_h(i + float3(1, 0, 1)), n011 = Noise_6_h(i + float3(0, 1, 1)), n111 = Noise_6_h(i + 1.0);
    float v = lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return v * 2.0 - 1.0;
}
float Noise_6(float3 p)
{
    float sum = 0.0, amp = 1.0, norm = 0.0;
    [loop] for (int l = 0; l < 6; ++l)
    {
        float v = Noise_6_one(p);
        v = true ? abs(v) : v;
        sum += v * amp; norm += amp;
        p *= 3.714286; amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
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
        p *= 4.0; amp *= 0.5;
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
    float2 v_f1_n4_o = (UV * float2(2.0, 2.0));
    float2 v_f1_n3_o = (v_f1_n4_o - ((float2)(1.0)));
    float v_f1_n13_o = (float4(v_f1_n3_o, 0.0, 1.0)).x;
    float v_f1_n14_o = (float4(v_f1_n3_o, 0.0, 1.0)).y;
    float b_f1_n12 = (Custom_f1_n12(v_f1_n13_o, v_f1_n14_o)).x;
    float v_f1_n11_o = (((float4)(b_f1_n12))).x;
    float v_f1_n10_o = (v_f1_n11_o / 6.283185);
    float v_f1_n9_o = frac(v_f1_n10_o);
    float v_n5_o = fmod(v_f1_n9_o, Fmod);
    float v_n4_o = (v_n5_o * F1);
    float v_n3_o = lerp(0.1, 0.8, saturate(Noise_2(((float3)(v_n4_o)) * 0.5) * 0.5 + 0.5));
    float v_n2_o = pow(max(v_n3_o, 0.0), 2.0);
    float v_n11_o = (v_n4_o * F2);
    float v_n10_o = lerp(0.1, 2.0, saturate(Noise_3(((float3)(v_n11_o)) * 1.0) * 0.5 + 0.5));
    float v_n9_o = pow(max(v_n10_o, 0.0), 6.0);
    float v_n1_o = (v_n2_o + v_n9_o);
    float2 v_f4_n0_o = float2(0.5, 0.5);
    float v_f4_n12_o = distance(UV, v_f4_n0_o);
    float v_f4_n11_o = (v_f4_n12_o * 1.0);
    float v_n16_o = lerp(0.7, 0.8, saturate(Noise_6(((float3)(v_f1_n9_o)) * 4.0) * 0.5 + 0.5));
    float v_n15_o = (v_n16_o * Radius);
    float v_f4_n10_o = (v_f4_n11_o / v_n15_o);
    float v_n18_o = lerp(1.9, 2.0, saturate(Noise_7(((float3)(v_f1_n9_o)) * 10.0) * 0.5 + 0.5));
    float v_f4_f5_n13_o = (v_f4_n10_o * v_n18_o);
    float v_f4_f5_n12_o = (v_f4_f5_n13_o * v_f4_f5_n13_o);
    float v_f4_f5_n11_o = ((1.0 > 0.5) ? v_f4_f5_n12_o : v_f4_f5_n13_o);
    float v_f4_f5_n9_o = pow(max(2.718282, 0.0), v_f4_f5_n11_o);
    float v_f4_f5_n7_o = (1.0 / v_f4_f5_n9_o);
    float v_f4_f5_n5_o = ((v_f4_n10_o == 0.0) ? 1.0 : ((v_f4_n10_o > 0.0) ? v_f4_f5_n7_o : 1.0));
    float v_f4_n15_o = (1.0 - v_f4_n10_o);
    float v_f4_f8_n13_o = (v_f4_n15_o * v_n18_o);
    float v_f4_f8_n12_o = (v_f4_f8_n13_o * v_f4_f8_n13_o);
    float v_f4_f8_n11_o = ((1.0 > 0.5) ? v_f4_f8_n12_o : v_f4_f8_n13_o);
    float v_f4_f8_n9_o = pow(max(2.718282, 0.0), v_f4_f8_n11_o);
    float v_f4_f8_n7_o = (1.0 / v_f4_f8_n9_o);
    float v_f4_f8_n5_o = ((v_f4_n15_o == 0.0) ? 1.0 : ((v_f4_n15_o > 0.0) ? v_f4_f8_n7_o : 1.0));
    float v_f4_n13_o = (1.0 - v_f4_f8_n5_o);
    float v_f4_n8_o = ((0.0 > 0.5) ? v_f4_f5_n5_o : v_f4_n13_o);
    float v_n0_o = (v_n1_o * v_f4_n8_o);
    float v_n21_o = (((float4)(v_n0_o))).x;
    float v_n22_o = (((float4)(v_n0_o))).y;
    float v_n20_o = max(v_n21_o, v_n22_o);
    float v_n23_o = (((float4)(v_n0_o))).z;
    float v_n19_o = max(v_n20_o, v_n23_o);
    return float4(((float3)(v_n0_o)), v_n19_o);
}
