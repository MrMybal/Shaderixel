//@name UI UV Rounded Rectangle Clip
//@category UI UV/Masks
//@input UV Map
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param radius_tl float 16 0 16384 "Top left radius (px)"
//@param radius_tr float 16 0 16384 "Top right radius (px)"
//@param radius_br float 16 0 16384 "Bottom right radius (px)"
//@param radius_bl float 16 0 16384 "Bottom left radius (px)"
//@param border float 0 0 16384 "Hollow border width (px; 0 = filled)"
//@param softness float 1 0 16 "Edge softness (px)"
//@param invert bool 0 "Invert clip"
//@param output_mode enum 0 "UV coordinates|Coverage" "Output"

// Per-corner rounded rectangle or hollow border clips the field coverage; inverse and mask output are available.
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float2 size=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.001);
float2 p=(m.rg-.5)*size;
float r=p.y<0?(p.x<0?radius_tl:radius_tr):(p.x<0?radius_bl:radius_br);
r=clamp(r,0,min(size.x,size.y)*.5);
float2 v=abs(p)-size*.5+r;
float d=length(max(v,0))+min(max(v.x,v.y),0)-r;
float mask=softness>0?saturate(.5-d/softness):(d<=0?1.0:0.0);
if (border>0) mask*=softness>0?saturate(.5+(d+border)/softness):(d>=-border?1.0:0.0);
if (invert>0.5) mask=1-mask;
float coverage=m.b*mask;
return output_mode>0.5?float4(coverage,coverage,coverage,1):float4(m.rg,coverage,1);
}
