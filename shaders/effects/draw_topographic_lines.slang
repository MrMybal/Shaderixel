//@name Topographic Lines
//@category Procedural Patterns
//@generator
//@param scale float 180 4 2048 "Terrain Scale (px)"
//@param seed int 7 0 65535 "Seed"

//@param levels int 18 2 128 "Contour Levels"
//@param width float 1 0.2 8 "Line Width (px)"
//@param major_every int 5 1 16 "Major Contour Interval"
//@param major_width float 2 0.2 12 "Major Line Width (px)"
//@param offset float 0 -1 1 "Contour Offset"
//@param line color 0.02 0.14 0.12 1 "Line Color"
//@param background color 0.72 0.86 0.72 1 "Background Color"

// Procedural terrain contours with pixel-sized lines and emphasized major contours.
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

float4 Effect(EffectContext ctx)
{
    float2 p=(float2(ctx.px)+0.5)/max(scale,4.0)+seed*13.1;
    float t=1.0/max(scale,4.0), h=ExtTerrain(p);
    float dx=(ExtTerrain(p+float2(t,0))-ExtTerrain(p-float2(t,0)))*0.5;
    float dy=(ExtTerrain(p+float2(0,t))-ExtTerrain(p-float2(0,t)))*0.5;
    return ExtContours(h,length(float2(dx,dy)));
}
