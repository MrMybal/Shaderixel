//@name Selective Detail
//@category Texture Processing
//@input Image
//@input Mask
//@halo radius 1 1
//@param amount float 0.7 -1 3 "Detail Strength"
//@param radius int 2 1 8 "Detail Radius (px)"
//@param threshold float 0.025 0 0.5 "Detail Threshold"
//@param low float 0.05 0 1 "Luminance Low"
//@param high float 0.95 0 1 "Luminance High"
//@param softness float 0.08 0.001 0.5 "Luminance Softness"

// Sparse alpha-weighted high-pass detail with luminance gating and a flat-region threshold.
float4 Effect(EffectContext ctx)
{
    float4 c=Input(0,ctx.px);
    if(amount==0.0) return c;
    float3 sum=0.0; float weight=0.0; int r=clamp(radius,1,8);
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    {
        float4 t=Input(0,ctx.px+int2(x,y)*r);
        sum+=LinearToSrgb(max(t.rgb,0.0))*t.a; weight+=t.a;
    }
    float3 rgb=LinearToSrgb(max(c.rgb,0.0));
    float3 d=rgb-(weight>0.000001?sum/weight:rgb);
    float detail=smoothstep(threshold,threshold+0.02,max(abs(d.r),max(abs(d.g),abs(d.b))));
    if(detail<=0.0) return c;
    float l=Luma(rgb), lo=min(low,high), hi=max(low,high);
    float range=smoothstep(lo-softness,lo,l)*(1.0-smoothstep(hi,hi+softness,l));
    float mask=HasInput(1)?saturate(Input(1,ctx.px).r*Input(1,ctx.px).a):1.0;
    return float4(SrgbToLinear(max(rgb+d*amount*detail*range*mask,0.0)),c.a);
}
