//@name UI UV Atlas Cell
//@category UI UV/Atlas
//@input UV Map
//@param grid float2 4 4 1 256 "Atlas columns / rows"
//@param frame float 0 -65536 65536 "Frame index (zero based)"
//@param gutter float2 0 0 0 1024 "Cell inset (px per side)"
//@param wrap bool 1 "Wrap frame index"
//@param top_down bool 1 "Rows start at top"

// Selects a grid atlas cell; gutter and half-texel inset prevent bilinear reads from adjacent cells.
float2 AtlasUV(float2 q, float index, float2 count, float2 canvas, float2 padding, float topDown) {
    float col=fmod(index,count.x), row=floor(index/count.x);
    if (topDown<0.5) row=count.y-1-row;
    float2 cell=canvas/count;
    float2 inset=min(max(padding,0)+0.5,max(cell*0.5,0.5));
    return (float2(col,row)*cell+inset+saturate(q)*max(cell-2*inset,0))/canvas;
}
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float2 count=max(floor(grid),1); float total=count.x*count.y;
float index=wrap>0.5?fmod(fmod(floor(frame),total)+total,total):clamp(floor(frame),0,total-1);
return float4(AtlasUV(m.rg,index,count,ctx.size,gutter,top_down),m.b,1);
}
