//@name Réglage Lab
//@category Réglages
//@param lightness float 0 -100 100 "Luminance L (décalage)"
//@param contrast float 1 0 3 "Contraste L"
//@param a_shift float 0 -100 100 "a (vert <-> rouge)"
//@param b_shift float 0 -100 100 "b (bleu <-> jaune)"
//@param chroma float 1 0 3 "Chroma (a, b)"
// Edits in CIE Lab (D50, Bradford from sRGB D65): lightness / contrast on L without
// touching the colors, color balance and chroma on a and b.
float3 LabF(float3 t)
{
    return select(t > 216.0 / 24389.0, pow(max(t, 0.0), 1.0 / 3.0), (24389.0 / 27.0 * t + 16.0) / 116.0);
}
float3 LabFinv(float3 f)
{
    float3 t = f * f * f;
    return select(t > 216.0 / 24389.0, t, (116.0 * f - 16.0) / (24389.0 / 27.0));
}

float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    const float3x3 toXyz = float3x3(0.4360747, 0.3850649, 0.1430804, 0.2225045, 0.7168786, 0.0606169, 0.0139322, 0.0971045, 0.7141733);
    const float3x3 toRgb = float3x3(3.1338561, -1.6168667, -0.4906146, -0.9787684, 1.9161415, 0.0334540, 0.0719453, -0.2289914, 1.4052427);
    const float3 white = float3(0.9642, 1.0, 0.8251);
    float3 f = LabF(mul(toXyz, max(c.rgb, 0.0)) / white);
    float L = 116.0 * f.y - 16.0, A = 500.0 * (f.x - f.y), B = 200.0 * (f.y - f.z);
    L = (L - 50.0) * contrast + 50.0 + lightness;
    A = A * chroma + a_shift;
    B = B * chroma + b_shift;
    float fy = (L + 16.0) / 116.0;
    float3 xyz = LabFinv(float3(fy + A / 500.0, fy, fy - B / 200.0)) * white;
    c.rgb = max(mul(toRgb, xyz), 0.0);
    return c;
}
