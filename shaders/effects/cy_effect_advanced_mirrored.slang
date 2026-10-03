//@name Advanced Mirrored
//@category Traitements
//@input Image
//@halo full
//@param Scale float 0.6 0.0 1.2 "Scale"
//@param K1 float -0.4 -0.4 1.0 "K1"

// Imported from Unreal material /CyTexture/Editor/LayerPresets/Effect/M_CyEffect_AdvancedMirrored.M_CyEffect_AdvancedMirrored: Native CyTexture advanced Effect preset with live GPU parameters.
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

float4 Custom_n12(float2 UV, float K1)
{
// LensFun poly3 Distortion
// Cubic screen
// by CyberAlien 2023

// "Rd = Ru * (1 - k1 + k1 * Ru^2)"

// UV
// K1

float Ratio = (View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y);
float2 CubicUV = float2( UV.x , ((UV.y/Ratio)+0.5) - (0.5/Ratio) );
float R = distance(CubicUV , float2(0.5,0.5) );
//float Kcal = R * ( 1 - K1 + (K1 * pow(R,2)) );
float Kcal = R * ( K1 + (K1 * pow(R,2)) );

return SXRet( UV - ( (UV - 0.5 ) * Kcal ) );
}

float4 Custom_n19(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n26(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n38(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n45(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n58(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n65(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n76(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n83(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Custom_n89(float2 UV, float Min, float Max)
{
return SXRet( (step(Min,UV.x)*step(UV.x,Max))*(step(Min,UV.y)*step(UV.y,Max)));
}

float4 Effect(EffectContext context)
{
    ctx = context;
    View.ViewSizeAndInvSize = float4(ctx.size, ctx.texel);
    View.BufferSizeAndInvSize = View.ViewSizeAndInvSize;
    View.TemporalAAParams = 0;  // no temporal jitter in a document
    ResolvedView = View;
    float2 UV = ctx.uv;
    float2 v_f3_n7_o = (UV / ((float2)(Scale)));
    float2 v_f3_n6_o = (v_f3_n7_o + ((float2)(0.5)));
    float v_f3_n9_o = (0.5 / Scale);
    float2 v_f3_n5_o = (v_f3_n6_o - ((float2)(v_f3_n9_o)));
    float2 b_n12 = (Custom_n12(v_f3_n5_o, K1)).xy;
    float2 v_n11_o = (b_n12 * ((float2)(1.0)));
    float2 v_n17_o = float2(1.0, 0.0);
    float2 v_n10_o = (v_n11_o + v_n17_o);
    float b_n19 = (Custom_n19(v_n10_o, 0.0, 1.0)).x;
    float v_n18_o = clamp(b_n19, 0.0, 1.0);
    float2 v_n9_o = (v_n10_o * ((float2)(v_n18_o)));
    float2 v_n24_o = float2(-1.0, 0.0);
    float2 v_n23_o = (v_n11_o + v_n24_o);
    float b_n26 = (Custom_n26(v_n23_o, 0.0, 1.0)).x;
    float v_n25_o = clamp(b_n26, 0.0, 1.0);
    float2 v_n22_o = (v_n23_o * ((float2)(v_n25_o)));
    float2 v_n8_o = (v_n9_o + v_n22_o);
    float v_f2_n2_o = (float4(v_n8_o, 0.0, 1.0)).x;
    float v_n6_o = (1.0 - v_f2_n2_o);
    float v_f4_n4_o = (float4(v_n8_o, 0.0, 1.0)).y;
    float2 v_f1_n3_o = float2(v_n6_o, v_f4_n4_o);
    float v_n29_o = (v_n18_o + v_n25_o);
    float2 v_n4_o = (v_f1_n3_o * ((float2)(v_n29_o)));
    float2 v_n36_o = float2(0.0, 1.0);
    float2 v_n35_o = (v_n11_o + v_n36_o);
    float b_n38 = (Custom_n38(v_n35_o, 0.0, 1.0)).x;
    float v_n37_o = clamp(b_n38, 0.0, 1.0);
    float2 v_n34_o = (v_n35_o * ((float2)(v_n37_o)));
    float2 v_n43_o = float2(0.0, -1.0);
    float2 v_n42_o = (v_n11_o + v_n43_o);
    float b_n45 = (Custom_n45(v_n42_o, 0.0, 1.0)).x;
    float v_n44_o = clamp(b_n45, 0.0, 1.0);
    float2 v_n41_o = (v_n42_o * ((float2)(v_n44_o)));
    float2 v_n33_o = (v_n34_o + v_n41_o);
    float v_f6_n2_o = (float4(v_n33_o, 0.0, 1.0)).x;
    float v_f7_n4_o = (float4(v_n33_o, 0.0, 1.0)).y;
    float v_n48_o = (1.0 - v_f7_n4_o);
    float2 v_f5_n3_o = float2(v_f6_n2_o, v_n48_o);
    float v_n49_o = (v_n37_o + v_n44_o);
    float2 v_n30_o = (v_f5_n3_o * ((float2)(v_n49_o)));
    float2 v_n3_o = (v_n4_o + v_n30_o);
    float2 v_n56_o = float2(1.0, 1.0);
    float2 v_n55_o = (v_n11_o + v_n56_o);
    float b_n58 = (Custom_n58(v_n55_o, 0.0, 1.0)).x;
    float v_n57_o = clamp(b_n58, 0.0, 1.0);
    float2 v_n54_o = (v_n55_o * ((float2)(v_n57_o)));
    float2 v_n63_o = float2(-1.0, -1.0);
    float2 v_n62_o = (v_n11_o + v_n63_o);
    float b_n65 = (Custom_n65(v_n62_o, 0.0, 1.0)).x;
    float v_n64_o = clamp(b_n65, 0.0, 1.0);
    float2 v_n61_o = (v_n62_o * ((float2)(v_n64_o)));
    float2 v_n53_o = (v_n54_o + v_n61_o);
    float2 v_n52_o = (1.0 - v_n53_o);
    float v_n68_o = (v_n57_o + v_n64_o);
    float2 v_n51_o = (v_n52_o * ((float2)(v_n68_o)));
    float2 v_n74_o = float2(-1.0, 1.0);
    float2 v_n73_o = (v_n11_o + v_n74_o);
    float b_n76 = (Custom_n76(v_n73_o, 0.0, 1.0)).x;
    float v_n75_o = clamp(b_n76, 0.0, 1.0);
    float2 v_n72_o = (v_n73_o * ((float2)(v_n75_o)));
    float2 v_n81_o = float2(1.0, -1.0);
    float2 v_n80_o = (v_n11_o + v_n81_o);
    float b_n83 = (Custom_n83(v_n80_o, 0.0, 1.0)).x;
    float v_n82_o = clamp(b_n83, 0.0, 1.0);
    float2 v_n79_o = (v_n80_o * ((float2)(v_n82_o)));
    float2 v_n71_o = (v_n72_o + v_n79_o);
    float2 v_n70_o = (1.0 - v_n71_o);
    float v_n86_o = (v_n75_o + v_n82_o);
    float2 v_n69_o = (v_n70_o * ((float2)(v_n86_o)));
    float2 v_n50_o = (v_n51_o + v_n69_o);
    float2 v_n2_o = (v_n3_o + v_n50_o);
    float2 v_n87_o = (b_n12 * ((float2)(1.0)));
    float b_n89 = (Custom_n89(v_n87_o, 0.0, 1.0)).x;
    float v_n88_o = clamp(b_n89, 0.0, 1.0);
    float2 v_n1_o = lerp(v_n2_o, v_n87_o, v_n88_o);
    float4 b_n0 = CyTex(0, v_n1_o);
    float3 v_n0_RGB = (b_n0).xyz;
    float4 b_n93 = CyTex(0, UV);
    float v_n92_o = (b_n93).w;
    return float4(v_n0_RGB, v_n92_o);
}
