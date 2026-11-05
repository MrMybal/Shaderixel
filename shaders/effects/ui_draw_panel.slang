//@name UI Draw Styled Panel
//@category UI UV/Draw
//@generator
//@premultiplied
//@param position float2 0.5 0.5 -4 4 "Panel center (document UV)"
//@param size float2 320 120 0.001 32768 "Panel size (px)"
//@param radius float 18 0 16384 "Corner radius (px)"
//@param softness float 1 0 16 "Edge softness (px)"
//@param color_top color 0.035 0.12 0.24 1 "Gradient start color"
//@param color_bottom color 0.01 0.035 0.08 1 "Gradient end color"
//@param angle float 90 -3600 3600 "Gradient angle (degrees)"
//@param border float 2 0 1024 "Border width (px)"
//@param border_color color 0.08 0.45 0.8 1 "Border color"
//@param bevel float 2 0 1024 "Bevel width (px)"
//@param bevel_strength float 0.18 0 2 "Bevel light strength"
//@param shadow_offset float2 0 6 -4096 4096 "Shadow offset (px)"
//@param shadow_softness float 10 0 1024 "Shadow softness (px)"
//@param shadow_color color 0 0 0 0.5 "Shadow color"
//@param glow_size float 0 0 1024 "Outer glow size (px)"
//@param glow_color color 0.02 0.3 1 0.5 "Outer glow color"

// Analytic rounded panel with gradient, border, bevel, shadow and glow; no neighbor sampling needed.
float PanelDistance(float2 p,float2 halfSize,float r) {
float2 q=abs(p)-halfSize+r;
return length(max(q,0))+min(max(q.x,q.y),0)-r;
}
float4 Over(float4 a,float4 b) { return a+b*(1-a.a); }
float4 Effect(EffectContext ctx) {
float2 halfSize=max(size*.5,.0001),p=(ctx.uv-position)*ctx.size;
float r=min(radius,min(halfSize.x,halfSize.y));
float d=PanelDistance(p,halfSize,r),aa=max(softness,.0001);
float mask=softness>0?saturate(.5-d/aa):(d<=0?1.0:0.0);
float a=radians(angle); float2 axis=float2(cos(a),sin(a));
float ramp=saturate(.5+dot(p,axis)/max(dot(abs(axis),size),.0001));
float4 color=lerp(color_top,color_bottom,ramp);
float borderMask=border>0?saturate(.5+(d+border)/aa):0;
color=lerp(color,border_color,borderMask);
float bevelMask=bevel>0?saturate(1-abs(d)/max(bevel,.0001)):0;
float light=dot(normalize(p+float2(.0001,.0001)),normalize(float2(-1,-1)));
color.rgb=max(0,color.rgb+bevelMask*bevel_strength*light);
float4 face=float4(color.rgb*color.a,color.a)*mask;
float sd=PanelDistance(p-shadow_offset,halfSize,r);
float shadow=shadow_softness>0?(1-smoothstep(-shadow_softness,shadow_softness,sd)):(sd<=0?1.0:0.0);
float4 back=float4(shadow_color.rgb*shadow_color.a,shadow_color.a)*shadow;
float glow=glow_size>0?exp(-max(d,0)*3/max(glow_size,.0001)):0;
float4 gc=float4(glow_color.rgb*glow_color.a,glow_color.a)*glow;
return Over(face,Over(gc,back));
}
