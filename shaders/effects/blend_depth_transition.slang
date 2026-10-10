//@name Depth Transition
//@category Creative Blends
//@blend
//@input Background
//@input Foreground
//@input Height
//@param progress float 0.5 0 1 "Transition Progress"
//@param softness float 0.08 0.001 0.5 "Depth Softness"
//@param invert bool 0 "Invert Depth"
//@param decode_srgb bool 1 "Decode PNG Numeric Depth"
//@param opacity float 1 0 1 "Foreground Opacity"

// Reveal the foreground from low to high depth, with explicit numeric PNG decoding and endpoint control.
float4 ExtMix(float4 a, float4 b, float t)
{
    return Unpremultiply(lerp(Premultiply(a), Premultiply(b), saturate(t)));
}
float4 ExtOver(float4 a, float4 b, float coverage)
{
    b.a = saturate(b.a * coverage);
    float4 ap = Premultiply(a), bp = Premultiply(b);
    return Unpremultiply(bp + ap * (1.0 - b.a));
}

float4 Effect(EffectContext ctx)
{
    float4 a=Input(0,ctx.px), b=Input(1,ctx.px);
    if(progress<=0.0||opacity<=0.0) return a;
    float4 map=HasInput(2)?Input(2,ctx.px):float4(0.5,0.5,0.5,1.0);
    float d=HasInput(2)&&decode_srgb!=0?LinearToSrgb1(map.r):map.r;
    d=saturate(d); if(invert!=0) d=1.0-d;
    float reveal=progress>=1.0?1.0:1.0-smoothstep(progress-softness,progress+softness,d);
    return ExtOver(a,b,reveal*opacity);
}
