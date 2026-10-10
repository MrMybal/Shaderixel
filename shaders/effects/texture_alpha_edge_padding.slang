//@name Alpha Edge Padding
//@category Texture Processing
//@input Image
//@halo radius 1 1
//@param radius int 8 0 32 "Padding Radius (px)"
//@param source_alpha float 0.1 0.001 1 "Minimum Source Alpha"
//@param carrier_alpha float 0.0001 0.000002 0.001 "RGB Carrier Alpha"

// Extend nearest nontransparent RGB into empty alpha; tiny carrier alpha quantizes to zero in 8-bit PNG.
float4 Effect(EffectContext ctx)
{
    float4 c=Input(0,ctx.px);
    int r=clamp(radius,0,32);
    if(c.a>0.000001 || r==0) return c;
    float best=1e20; float3 rgb=0.0; bool found=false;
    for(int y=-r;y<=r;++y) for(int x=-r;x<=r;++x)
    {
        float d=float(x*x+y*y);
        if(d>float(r*r)||d>=best) continue;
        int2 p=ctx.px+int2(x,y);
        if(any(p<0)||any(p>=int2(ctx.size))) continue;
        float4 t=Input(0,p);
        if(t.a>=max(source_alpha,0.001)) { best=d; rgb=t.rgb; found=true; }
    }
    // Keep carrier above Unpremultiply's 1e-6 threshold but below half one RGBA8 alpha step.
    return found?float4(rgb,clamp(carrier_alpha,0.000002,0.001)):c;
}
