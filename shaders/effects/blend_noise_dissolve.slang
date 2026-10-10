//@name Noise Dissolve
//@category Creative Blends
//@blend
//@input Background
//@input Foreground
//@input Mask
//@param progress float 0.5 0 1 "Transition Progress"
//@param scale float 24 1 512 "Noise Scale (px)"
//@param softness float 0.08 0.001 0.5 "Reveal Softness"
//@param seed int 5 0 65535 "Seed"
//@param opacity float 1 0 1 "Foreground Opacity"

// Progressive procedural noise reveal with optional linear coverage mask and exact transition endpoints.
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
    float4 a=Input(0,ctx.px), b=Input(1,ctx.px);
    if(progress<=0.0||opacity<=0.0) return a;
    float n=ExtNoise((float2(ctx.px)+0.5)/max(scale,1.0)+seed*11.7);
    float reveal=progress>=1.0?1.0:1.0-smoothstep(progress-softness,progress+softness,n);
    float mask=HasInput(2)?saturate(Input(2,ctx.px).r*Input(2,ctx.px).a):1.0;
    return ExtOver(a,b,reveal*mask*opacity);
}
