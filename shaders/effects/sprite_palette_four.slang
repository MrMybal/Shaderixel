//@name Sprite Four-Color Palette
//@category 2D Game Art
//@param color_a color 0.01 0.015 0.025 1 "Palette Color 1"
//@param color_b color 0.05 0.16 0.12 1 "Palette Color 2"
//@param color_c color 0.35 0.55 0.22 1 "Palette Color 3"
//@param color_d color 0.8 0.9 0.5 1 "Palette Color 4"
//@param dither float 0.15 0 1 "Ordered Dither"
//@param amount float 1 0 1 "Palette Strength"
static const float bayer4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int index = (ctx.px.y & 3) * 4 + (ctx.px.x & 3);
    float3 p = LinearToSrgb(max(c.rgb, 0.0)) + ((bayer4[index] + 0.5) / 16.0 - 0.5) * dither * 0.25;
    float4 colors[4] = {color_a, color_b, color_c, color_d};
    float best = 1e30;
    float4 chosen = color_a;
    for (int i = 0; i < 4; ++i)
    {
        float3 delta = p - LinearToSrgb(max(colors[i].rgb, 0.0));
        float d = dot(delta, delta);
        if (d < best) { best = d; chosen = colors[i]; }
    }
    float strength = saturate(amount);
    return float4(lerp(c.rgb, chosen.rgb, strength), c.a * lerp(1.0, chosen.a, strength));
}
