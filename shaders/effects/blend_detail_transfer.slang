//@name Detail Transfer
//@category Creative Blends
//@blend
//@input Background
//@input Detail Source
//@input Mask
//@halo radius 1 1
//@param amount float 0.8 -2 4 "Detail Strength"
//@param radius int 3 1 16 "Detail Radius (px)"
//@param threshold float 0.01 0 0.5 "Detail Threshold"
//@param color_detail bool 0 "Transfer Color Detail"
//@param opacity float 1 0 1 "Transfer Opacity"

// Transfer source high-frequency luminance or color detail while retaining background alpha and broad tone.
float4 Effect(EffectContext ctx)
{
    float4 a=Input(0,ctx.px), b=Input(1,ctx.px);
    if(amount==0.0||opacity<=0.0) return a;
    float3 sum=0.0; float weight=0.0; int r=clamp(radius,1,16);
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
    {
        float4 t=Input(1,ctx.px+int2(x,y)*r);
        sum+=LinearToSrgb(max(t.rgb,0.0))*t.a; weight+=t.a;
    }
    float3 rgb=LinearToSrgb(max(b.rgb,0.0));
    float3 d=rgb-(weight>0.000001?sum/weight:rgb);
    if(color_detail==0) d=Luma(d).xxx;
    float gate=smoothstep(threshold,threshold+0.02,max(abs(d.r),max(abs(d.g),abs(d.b))));
    if(gate<=0.0 || b.a<=0.0) return a;
    float mask=HasInput(2)?saturate(Input(2,ctx.px).r*Input(2,ctx.px).a):1.0;
    float3 result=max(LinearToSrgb(max(a.rgb,0.0))+d*gate*amount*mask*opacity*b.a,0.0);
    return float4(SrgbToLinear(result),a.a);
}
