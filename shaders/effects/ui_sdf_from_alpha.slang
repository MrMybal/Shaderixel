//@name UI SDF From Alpha
//@category UI Toolkit/SDF
//@passes 16
//@halo full
//@input Image
//@param channel enum 0 "Alpha|Red|Luminance" "Source channel"
//@param threshold float 0.5 0.0001 0.9999 "Inside threshold"
//@param invert bool 0 "Invert source"

// Approximate signed distance from an alpha/red/luminance threshold with 16-pass jump flooding; raster fields have no contour arclength.
// Jump-flooded nearest threshold boundary: first jump is folded into seed initialization.
// Output R=approximate signed pixel distance, G=B=0, A=1; no ordered contour is reconstructed.
float SourceValue(int2 px,float2 size) {
    if(any(px<0)||any(px>=int2(size))) return 0;
    float4 c=Input(0,px);float v=channel==0?c.a:(channel==1?c.r:Luma(c.rgb));
    return invert>.5?1-v:v;
}
float3 SeedAt(int2 px,float2 size) {
    if(any(px<0)||any(px>=int2(size))) return float3(0,0,0);
    float a=SourceValue(px,size),best=1e20;float2 seed=0;
    int2 dirs[4]={int2(1,0),int2(-1,0),int2(0,1),int2(0,-1)};
    for(int i=0;i<4;++i) {
        float b=SourceValue(px+dirs[i],size);
        if((a>=threshold)!=(b>=threshold)) {
            float t=saturate((threshold-a)/(b-a));
            if(t<best) { best=t;seed=float2(px)+.5+float2(dirs[i])*t; }
        }
    }
    return float3(seed,best<1e19?1.0:0.0);
}
float4 Effect(EffectContext ctx) {
    float base=exp2(max(ceil(log2(max(ctx.size.x,ctx.size.y)))-1,0));
    if(ctx.pass==15) {
        float4 seed=Prev(ctx.px);float d=seed.b>.5?length(float2(ctx.px)+.5-seed.rg):length(ctx.size)+1;
        if(SourceValue(ctx.px,ctx.size)>=threshold) d=-d;
        return float4(d,0,0,1);
    }
    int step=int(max(base/exp2(float(ctx.pass)),1));
    float best=1e30;float3 nearest=float3(0,0,0);
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
        int2 p=ctx.px+int2(x,y)*step;
        if(any(p<0)||any(p>=int2(ctx.size))) continue;
        float3 candidate=ctx.pass==0?SeedAt(p,ctx.size):Prev(p).rgb;
        float2 v=float2(ctx.px)+.5-candidate.xy;float dist=dot(v,v);
        if(candidate.z>.5 && dist<best) { best=dist;nearest=candidate; }
    }
    return float4(nearest,1);
}
