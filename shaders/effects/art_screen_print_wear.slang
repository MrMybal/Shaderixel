//@name Screen Print Wear
//@category Illustration and Print
//@input Ink
//@input Mask
//@param amount float 0.25 0 1 "Wear Amount"
//@param scale float 5 1 256 "Hole Scale (px)"
//@param softness float 0.06 0.001 0.3 "Hole Softness"
//@param scratches float 0.2 0 1 "Scratch Contribution"
//@param seed int 3 0 65535 "Seed"

// Remove ink alpha with deterministic holes and elongated scratches, optionally restricted by a linear coverage mask.
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

float4 Effect(EffectContext ctx)
{
    float4 c=Input(0,ctx.px);
    if(amount<=0.0) return c;
    float2 p=float2(ctx.px)+float2(seed*17.0,seed*31.0);
    float holes=ExtNoise(p/max(scale,1.0));
    float streak=ExtNoise(p/float2(max(scale,1.0)*12.0,max(scale,1.0)*0.3));
    float field=lerp(holes,min(holes,streak),scratches);
    float worn=amount>=1.0?1.0:1.0-smoothstep(amount-softness,amount+softness,field);
    float mask=HasInput(1)?saturate(Input(1,ctx.px).r*Input(1,ctx.px).a):1.0;
    return float4(c.rgb,c.a*(1.0-worn*mask));
}
