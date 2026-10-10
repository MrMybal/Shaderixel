//@name Print Halftone Screen
//@category Print and Halftone
//@input Image
//@param spacing float 8 2 128 "Cell Spacing (px)"
//@param angle float 45 -180 180 "Screen Angle (degrees)"
//@param pattern enum 0 "Dots|Lines|Crosshatch" "Pattern"
//@param gamma float 1 0.1 4 "Ink Curve"
//@param ink color 0.01 0.01 0.01 1 "Ink Color"
//@param paper color 1 1 1 1 "Paper Color"
//@param transparent_paper bool 0 "Transparent Paper"
//@param invert bool 0 "Invert Tone"
// Pointwise tone modulation; no cell-center sampling or neighboring reads.
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = saturate(Luma(LinearToSrgb(max(c.rgb, 0.0))));
    y = y < 0.000001 ? 0.0 : (y > 0.999999 ? 1.0 : y);
    float tone = pow(invert != 0 ? y : 1.0 - y, max(gamma, 0.1));
    float a = radians(angle), s = max(spacing, 2.0);
    float2 p = float2(ctx.px) + 0.5;
    float2 q = float2(dot(p, float2(cos(a), sin(a))), dot(p, float2(-sin(a), cos(a))));
    float2 cell = frac(q / s) - 0.5;
    float metric = pattern == 0 ? length(cell) : (pattern == 1 ? abs(cell.x) : min(abs(cell.x), abs(cell.y)));
    float radius = pattern == 0 ? sqrt(tone / 3.14159265) : (pattern == 1 ? tone * 0.5 : (1.0 - sqrt(1.0 - tone)) * 0.5);
    float aa = 0.65 / s;
    float m = 1.0 - smoothstep(radius - aa, radius + aa, metric);
    // Transfer-function roundoff must not leave faint ink on pure white.
    if (tone <= 0.000001) m = 0.0;
    if (tone >= 0.999999) m = 1.0;
    float ia = saturate(m * ink.a), pa = transparent_paper != 0 ? 0.0 : saturate(paper.a);
    float outAlpha = ia + pa * (1.0 - ia);
    float3 rgb = ink.rgb * ia + paper.rgb * pa * (1.0 - ia);
    return float4(outAlpha > 0.000001 ? rgb / outAlpha : 0.0, outAlpha * c.a);
}
