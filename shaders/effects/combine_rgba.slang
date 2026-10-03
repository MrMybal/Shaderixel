//@name Combiner RGBA
//@category Canaux
//@input R
//@input G
//@input B
//@input A
//@halo 0
//@premultiplied
// Rebuilds a color image from four gray images (e.g. channels split in the
// graph): each input's red channel gives one channel. Missing inputs: R, G, B = 0, A = 1.

float4 Effect(EffectContext ctx)
{
    float r = HasInput(0) ? Input(0, ctx.px).r : 0.0;
    float g = HasInput(1) ? Input(1, ctx.px).r : 0.0;
    float b = HasInput(2) ? Input(2, ctx.px).r : 0.0;
    float a = HasInput(3) ? saturate(Input(3, ctx.px).r) : 1.0;
    return float4(float3(r, g, b) * a, a);  // premultiplied output
}
