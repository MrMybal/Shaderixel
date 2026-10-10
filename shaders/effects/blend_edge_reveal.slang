//@name Edge Reveal
//@category Creative Blends
//@blend
//@input Background
//@input Foreground
//@input Mask
//@halo reach 1 1
//@param progress float 0.5 0 1 "Transition Progress"
//@param reach int 24 1 64 "Search Reach (px)"
//@param softness float 2 0.1 16 "Front Softness (px)"
//@param opacity float 1 0 1 "Foreground Opacity"

// Reveal inward from a mask contour using bounded eight-direction distance approximation; progress one reveals the full mask.
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

float ExtCoverage(int2 p, float2 size)
{
    if(any(p<0)||any(p>=int2(size))) return 0.0;
    return HasInput(2)?saturate(Input(2,p).r*Input(2,p).a):saturate(Input(1,p).a);
}
float4 Effect(EffectContext ctx)
{
    float4 a=Input(0,ctx.px), b=Input(1,ctx.px);
    if(progress<=0.0||opacity<=0.0) return a;
    float m=HasInput(2)?ExtCoverage(ctx.px,ctx.size):1.0;
    if(progress>=1.0) return ExtOver(a,b,m*opacity);
    int r=clamp(reach,1,64); float nearest=float(r)+softness+1.0;
    for(int dir=0;dir<8;++dir)
    {
        float angle=radians(float(dir)*45.0); float2 axis=float2(cos(angle),sin(angle));
        for(int i=1;i<=r;++i)
        {
            if(ExtCoverage(ctx.px+int2(round(axis*i)),ctx.size)<0.5) {nearest=min(nearest,float(i));break;}
        }
    }
    float front=progress*(float(r)+softness);
    float reveal=1.0-smoothstep(front-softness,front+softness,nearest);
    return ExtOver(a,b,m*reveal*opacity);
}
