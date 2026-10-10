//@name Flow Lines
//@category Procedural Patterns
//@generator
//@param spacing float 14 2 256 "Line Spacing (px)"
//@param width float 1.5 0.2 16 "Line Width (px)"
//@param scale float 160 4 2048 "Flow Scale (px)"
//@param bend float 70 0 512 "Bend Amplitude (px)"
//@param angle float 0 -180 180 "Direction (degrees)"
//@param seed int 4 0 65535 "Seed"
//@param line color 0.015 0.1 0.18 1 "Line Color"
//@param background color 0.66 0.85 0.91 1 "Background Color"

// Curved contours of a smooth directional field with gradient-corrected stroke width.
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

float ExtFlow(float2 p)
{
    float a=radians(angle); float2 q=float2(dot(p,float2(cos(a),sin(a))),dot(p,float2(-sin(a),cos(a))));
    return q.y+(ExtTerrain(q/max(scale,4.0)+seed*17.3)-0.5)*bend;
}
float4 Effect(EffectContext ctx)
{
    float2 p=float2(ctx.px)+0.5; float s=max(spacing,2.0), f=ExtFlow(p);
    float gradient=length(float2(ExtFlow(p+float2(0.5,0))-ExtFlow(p-float2(0.5,0)),
                                 ExtFlow(p+float2(0,0.5))-ExtFlow(p-float2(0,0.5))));
    float d=abs(frac(f/s+0.5)-0.5)*s/max(gradient,0.1);
    return ExtOver(background,line,1.0-smoothstep(width*0.5,width*0.5+0.75,d));
}
