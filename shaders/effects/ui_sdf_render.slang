//@name UI SDF Styled Fill
//@category UI Toolkit/SDF
//@premultiplied
//@halo full
//@input Distance Field
//@param color_a color 0.035 0.18 0.38 1 "Gradient start color"
//@param color_b color 0.008 0.045 0.12 1 "Gradient end color"
//@param gradient_center float2 0.5 0.5 -4 4 "Gradient center (document UV)"
//@param gradient_angle float 90 -3600 3600 "Gradient angle (degrees)"
//@param gradient_span float 200 0.001 32768 "Gradient span (px)"
//@param grow float 0 -4096 4096 "Grow / shrink (px)"
//@param softness float 1 0 256 "Edge softness (px)"
//@param border float 2 0 4096 "Inner border width (px)"
//@param border_color color 0.08 0.55 1 1 "Border color"
//@param bevel float 3 0 4096 "Inner bevel width (px)"
//@param bevel_strength float 0.15 0 4 "Bevel light strength"
//@param light_angle float -135 -3600 3600 "Light angle (degrees)"
//@param shadow_offset float2 0 6 -8192 8192 "Shadow offset (px)"
//@param shadow_softness float 10 0 4096 "Shadow softness (px)"
//@param shadow_color color 0 0 0 0.5 "Shadow color"
//@param glow_radius float 0 0 4096 "Outer glow radius (px)"
//@param glow_color color 0.02 0.3 1 0.65 "Glow color"
//@param opacity float 1 0 1 "Opacity"

// Gradient fill, inner border, finite-difference bevel, shadow and glow from any signed distance field.
float4 PM(float4 c) { return float4(c.rgb*c.a,c.a); }
float4 Over(float4 a,float4 b) { return a+b*(1-a.a); }
float4 Effect(EffectContext ctx) {
    if(!HasInput(0)) return float4(0,0,0,0);
    float d=Input(0,ctx.px).r-grow,aa=max(softness,.00001);
    float fill=softness>0?saturate(.5-d/aa):(d<=0?1.0:0.0);
    float angle=radians(gradient_angle);float2 axis=float2(cos(angle),sin(angle));
    float ramp=saturate(.5+dot((ctx.uv-gradient_center)*ctx.size,axis)/max(gradient_span,.00001));
    float4 c=lerp(color_a,color_b,ramp);
    float edge=border>0?saturate(.5+(d+border)/aa):0;
    c=lerp(c,border_color,edge);
    if(bevel>0 && bevel_strength>0) {
        float2 n=float2(Input(0,ctx.px+int2(1,0)).r-Input(0,ctx.px-int2(1,0)).r,Input(0,ctx.px+int2(0,1)).r-Input(0,ctx.px-int2(0,1)).r);
        n/=max(length(n),.00001);float a=radians(light_angle);
        float light=dot(n,float2(cos(a),sin(a)))*saturate(1-abs(d)/bevel)*bevel_strength;
        c.rgb=max(c.rgb+light,0);
    }
    float sd=Input(0,ctx.px-int2(round(shadow_offset))).r-grow;
    float shadow=shadow_softness>0?1-smoothstep(-shadow_softness,shadow_softness,sd):(sd<=0?1.0:0.0);
    float glow=glow_radius>0?exp(-max(d,0)*3/max(glow_radius,.00001)):0;
    return Over(PM(c)*fill,Over(PM(glow_color)*glow,PM(shadow_color)*shadow))*opacity;
}
