//@name Torn Paper
//@category Procedural Patterns
//@generator
//@param center float2 0.5 0.5 0 1 "Paper Center (UV)"
//@param half_size float2 0.35 0.35 0.01 1 "Paper Half Size (UV)"
//@param angle float -6 -180 180 "Rotation (degrees)"
//@param roughness float 12 0 128 "Tear Amplitude (px)"
//@param scale float 32 2 256 "Tear Scale (px)"
//@param softness float 1.2 0.1 8 "Edge Softness (px)"
//@param grain float 0.03 0 0.3 "Paper Grain"
//@param seed int 9 0 65535 "Seed"
//@param color color 0.9 0.83 0.67 1 "Paper Color"

// Rotatable paper cutout with multiscale torn edges, fine grain and transparent surroundings.
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
    float a=radians(angle); float2 p=(ctx.uv-center)*ctx.size;
    p=float2(dot(p,float2(cos(a),sin(a))),dot(p,float2(-sin(a),cos(a))));
    float2 halfSize=half_size*ctx.size;
    float dx=abs(p.x)-halfSize.x+(ExtTerrain(float2(p.y/max(scale,2.0),sign(p.x)*17.0)+seed*9.1)-0.5)*roughness;
    float dy=abs(p.y)-halfSize.y+(ExtTerrain(float2(p.x/max(scale,2.0),sign(p.y)*31.0)+seed*7.3)-0.5)*roughness;
    float coverage=1.0-smoothstep(-max(softness,0.1)*0.5,max(softness,0.1)*0.5,max(dx,dy));
    float variation=(Hash12(float2(ctx.px)+seed*19.7)-0.5)*grain;
    float3 rgb=SrgbToLinear(saturate(LinearToSrgb(max(color.rgb,0.0))+variation));
    return float4(rgb,coverage*color.a);
}
