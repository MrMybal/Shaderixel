//@name UI UV Progress Clip
//@category UI UV/Masks
//@input UV Map
//@param direction enum 0 "Horizontal|Vertical|Radial" "Fill direction"
//@param progress float 0.65 0 1 "Progress"
//@param reverse bool 0 "Reverse fill"
//@param start_angle float -90 -3600 3600 "Radial start angle (degrees)"
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param segments float 1 1 512 "Segment count"
//@param gap float 0.15 0 0.99 "Gap fraction per segment"
//@param softness float 1 0 16 "Edge softness (px)"
//@param output_mode enum 0 "UV coordinates|Coverage" "Output"

// Linear, vertical or radial progress with reverse fill, segmented gaps, pixel antialiasing and mask output.
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float2 size=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.001);
float t=direction<.5?m.r:m.g, pixels=direction<.5?size.x:size.y;
if (direction>1.5) { float2 p=(m.rg-.5)*size; t=frac((atan2(p.y,p.x)-radians(start_angle))/6.28318530718); pixels=max(length(p)*6.28318530718,1); }
if (reverse>0.5) t=1-t;
float mask=softness>0?saturate((progress-t)*pixels/softness+.5):(t<=progress?1.0:0.0);
if (progress<=0) mask=0; else if (progress>=1) mask=1;
float n=max(floor(segments),1);
if (n>1 && gap>0) {
    float local=frac(t*n), edge=min(local-gap*.5,1-gap*.5-local)*pixels/n;
    mask*=softness>0?saturate(edge/softness+.5):(edge>=0?1.0:0.0);
}
float coverage=m.b*mask;
return output_mode>.5?float4(coverage,coverage,coverage,1):float4(m.rg,coverage,1);
}
