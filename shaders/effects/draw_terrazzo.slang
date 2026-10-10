//@name Terrazzo
//@category Procedural Patterns
//@generator
//@param size float 36 4 512 "Chip Cell Size (px)"
//@param density float 0.8 0 1 "Chip Density"
//@param variation float 0.7 0 1 "Chip Size Variation"
//@param seed int 11 0 65535 "Seed"
//@param background color 0.78 0.72 0.6 1 "Binder Color"
//@param color_a color 0.04 0.12 0.18 1 "Chip Color A"
//@param color_b color 0.5 0.12 0.06 1 "Chip Color B"
//@param color_c color 0.9 0.83 0.6 1 "Chip Color C"

// Deterministic convex polygon chips with three palette colors, density and size variation.
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
    float s=max(size,4.0); float2 p=(float2(ctx.px)+0.5)/s, cell=floor(p);
    float4 result=background;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    {
        float2 id=cell+float2(x,y), key=id+seed*7.3;
        if(Hash12(key+53.7)>=density) continue;
        float2 center=id+0.5+(Hash22(key)-0.5)*0.35;
        float2 q=(p-center)*s;
        float radius=s*lerp(0.34,0.15+Hash12(key+9.8)*0.25,variation);
        float rot=Hash12(key+19.1)*6.2831853, d=-1e10;
        for(int i=0;i<6;++i)
        {
            float a=rot+i*1.04719755;
            float side=radius*(0.65+Hash12(key+float(i)*3.9)*0.35);
            d=max(d,dot(q,float2(cos(a),sin(a)))-side);
        }
        float coverage=1.0-smoothstep(-0.7,0.7,d);
        float choose=Hash12(key+103.7);
        float4 chip=choose<0.333333?color_a:(choose<0.666667?color_b:color_c);
        result=ExtOver(result,chip,coverage);
    }
    return result;
}
