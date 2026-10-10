//@name Ink Bleed
//@category Illustration and Print
//@input Ink
//@halo radius 1 1
//@param radius int 3 0 12 "Spread Radius (px)"
//@param amount float 0.65 0 1 "Absorption"
//@param irregularity float 0.55 0 1 "Edge Irregularity"
//@param scale float 8 1 128 "Absorption Scale (px)"
//@param seed int 1 0 65535 "Seed"

// Spread colored alpha ink into irregular absorbent edges; opaque scans should first become ink on transparency.
float ExtNoise(float2 p)
{
    float2 cell = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(Hash12(cell), Hash12(cell + float2(1,0)), f.x),
                lerp(Hash12(cell + float2(0,1)), Hash12(cell + 1.0), f.x), f.y);
}
float ExtTerrain(float2 p)
{
    return ExtNoise(p)*0.5714286 + ExtNoise(p*2.03+19.7)*0.2857143 + ExtNoise(p*4.11+71.3)*0.1428571;
}

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
    int r=clamp(radius,0,12);
    if(r==0 || amount<=0.0) return c;
    float varying=lerp(1.0,0.35+0.65*ExtNoise((float2(ctx.px)+seed*13.1)/max(scale,1.0)),irregularity);
    float reach=r*varying, best=0.0; float3 pigment=c.rgb;
    for(int y=-r;y<=r;++y) for(int x=-r;x<=r;++x)
    {
        int2 p=ctx.px+int2(x,y);
        if(any(p<0)||any(p>=int2(ctx.size))) continue;
        float4 tap=Input(0,p);
        float weight=tap.a*(1.0-smoothstep(max(0.0,reach-1.0),reach+0.5,length(float2(x,y))));
        if(weight>best) { best=weight; pigment=tap.rgb; }
    }
    return ExtOver(float4(pigment,saturate(best*amount)),c,1.0);
}
