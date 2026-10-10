//@name Crosshatch Shading
//@category Illustration and Print
//@input Image
//@param spacing float 8 2 128 "Line Spacing (px)"
//@param width float 1 0.2 12 "Stroke Width (px)"
//@param angle float 25 -180 180 "First Direction (degrees)"
//@param layers int 4 1 4 "Hatch Layers"
//@param contrast float 1 0.1 4 "Shadow Curve"
//@param strength float 1 0 1 "Effect Strength"
//@param ink color 0.012 0.009 0.006 1 "Ink Color"
//@param paper color 0.9 0.84 0.7 1 "Paper Color"

// Convert image shadows into up to four directional hatch layers while preserving source alpha.
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
    float4 c=Input(0,ctx.px);
    float tone=pow(saturate(1.0-Luma(LinearToSrgb(max(c.rgb,0.0)))),max(contrast,0.1));
    float covered=0.0, s=max(spacing,2.0);
    for(int i=0;i<clamp(layers,1,4);++i)
    {
        float a=radians(angle+i*45.0);
        float phase=dot(float2(ctx.px)+0.5,float2(cos(a),sin(a)))/s+i*0.137;
        float d=abs(frac(phase+0.5)-0.5)*s;
        float line=1.0-smoothstep(width*0.5,width*0.5+0.75,d);
        float active=smoothstep((i+0.2)/4.0,(i+0.8)/4.0,tone);
        covered=1.0-(1.0-covered)*(1.0-line*active);
    }
    float4 outColor=ExtMix(paper,ink,covered);
    outColor.a*=c.a;
    return ExtMix(c,outColor,strength);
}
