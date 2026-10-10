//@name Topographic Lines from Height
//@category Illustration and Print
//@input Height
//@halo 1
//@param decode_srgb bool 1 "Decode PNG Numeric Height"

//@param levels int 18 2 128 "Contour Levels"
//@param width float 1 0.2 8 "Line Width (px)"
//@param major_every int 5 1 16 "Major Contour Interval"
//@param major_width float 2 0.2 12 "Major Line Width (px)"
//@param offset float 0 -1 1 "Contour Offset"
//@param line color 0.02 0.14 0.12 1 "Line Color"
//@param background color 0.72 0.86 0.72 1 "Background Color"

// Extract styled contours from an existing grayscale height map; PNG numeric decoding is on by default.
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

float4 ExtContours(float h, float gradient)
{
    if (gradient < 0.000001) return background; // Constant plateaus have no contour direction.
    float count=clamp(float(levels),2.0,128.0), v=(h+offset)*count;
    float nearest=floor(v+0.5), dist=abs(v-nearest)/max(gradient*count,0.00001);
    float major=abs(nearest-round(nearest/max(float(major_every),1.0))*max(float(major_every),1.0))<0.1?1.0:0.0;
    float stroke=lerp(width,major_width,major);
    float coverage=1.0-smoothstep(stroke*0.5,stroke*0.5+0.75,dist);
    return ExtOver(background,line,coverage);
}

float ExtHeight(int2 p)
{
    float4 h=Input(0,p);
    return saturate((decode_srgb!=0?LinearToSrgb1(h.r):h.r)*h.a);
}
float4 Effect(EffectContext ctx)
{
    float h=ExtHeight(ctx.px);
    float dx=(ExtHeight(ctx.px+int2(1,0))-ExtHeight(ctx.px-int2(1,0)))*0.5;
    float dy=(ExtHeight(ctx.px+int2(0,1))-ExtHeight(ctx.px-int2(0,1)))*0.5;
    return ExtContours(h,length(float2(dx,dy)));
}
