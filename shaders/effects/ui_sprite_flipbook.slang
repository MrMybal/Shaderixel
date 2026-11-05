//@name UI Sprite Flipbook
//@category UI UV/Atlas
//@premultiplied
//@halo full
//@input Image
//@input UV Map
//@param grid float2 4 4 1 256 "Atlas columns / rows"
//@param frame float 0 -65536 65536 "Frame index (zero based)"
//@param gutter float2 0 0 0 1024 "Cell inset (px per side)"
//@param wrap bool 1 "Wrap frame index"
//@param top_down bool 1 "Rows start at top"
//@param first_frame float 0 0 65535 "First frame"
//@param frame_count float 0 0 65536 "Playback frame count (0 = remaining)"
//@param interpolate bool 0 "Blend consecutive frames"
//@param opacity float 1 0 1 "Opacity"

// Explicit frame-driven sprite-sheet animation with constrained range, loop/clamp and premultiplied frame blending.
// Explicit interpolation keeps float32 texture filtering consistent across backends.
float4 UIReadLinear(uint index, float2 uv, float2 size) {
    float2 p = uv * size - 0.5;
    int2 base = int2(floor(p));
    float2 f = frac(p);
    float4 top = lerp(Input(index, base), Input(index, base + int2(1, 0)), f.x);
    float4 bottom = lerp(Input(index, base + int2(0, 1)), Input(index, base + int2(1, 1)), f.x);
    return lerp(top, bottom, f.y);
}

float2 AtlasUV(float2 q, float index, float2 count, float2 canvas, float2 padding, float topDown) {
    float col=fmod(index,count.x), row=floor(index/count.x);
    if (topDown<0.5) row=count.y-1-row;
    float2 cell=canvas/count;
    float2 inset=min(max(padding,0)+0.5,max(cell*0.5,0.5));
    return (float2(col,row)*cell+inset+saturate(q)*max(cell-2*inset,0))/canvas;
}
float4 Effect(EffectContext ctx) {
if (!HasInput(0)) return float4(0,0,0,0);
float4 m=HasInput(1)?Input(1,ctx.px):float4(ctx.uv,1,1);
float2 count=max(floor(grid),1); float total=count.x*count.y;
float first=clamp(floor(first_frame),0,total-1);
float length=max(1,min(frame_count>0?floor(frame_count):total-first,total-first));
float f=wrap>0.5?(frame-floor(frame/length)*length):clamp(frame,0,length-1);
float i=floor(f), j=wrap>0.5?fmod(i+1,length):min(i+1,length-1);
float4 a=UIReadLinear(0,AtlasUV(m.rg,first+i,count,ctx.size,gutter,top_down),ctx.size);
float4 b=UIReadLinear(0,AtlasUV(m.rg,first+j,count,ctx.size,gutter,top_down),ctx.size);
return lerp(a,b,interpolate>0.5?frac(f):0)*saturate(m.b)*opacity;
}
