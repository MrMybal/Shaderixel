//@name UI SDF Border and Stroke
//@category UI Toolkit/SDF
//@premultiplied
//@input Distance Field
//@param pattern enum 0 "Solid|Dashed|Dotted" "Stroke pattern"
//@param alignment enum 1 "Inside|Centered|Outside" "Stroke alignment"
//@param width float 3 0.001 4096 "Line width (px)"
//@param double_line bool 0 "Double line"
//@param double_gap float 4 0 4096 "Double line gap (px)"
//@param dash float 12 0.001 4096 "Dash length / dot period base (px)"
//@param gap float 8 0 4096 "Dash / dot gap (px)"
//@param phase float 0 -100000 100000 "Dash phase (px)"
//@param fit bool 1 "Fit whole periods around contour"
//@param caps enum 1 "Butt|Round" "Dash caps"
//@param path_mode enum 0 "Contour|Angular|Projected" "Dash coordinate mode"
//@param center float2 0.5 0.5 -4 4 "Angular / projected origin (UV)"
//@param angle float 0 -3600 3600 "Projection angle (degrees)"
//@param softness float 1 0 64 "Edge softness (px)"
//@param color_a color 0.1 0.65 1 1 "Stroke start color"
//@param color_b color 0.65 0.2 1 1 "Stroke end color"
//@param gradient bool 0 "Gradient along contour"
//@param opacity float 1 0 1 "Opacity"

// Pixel-sized solid, dashed, dotted and double borders; analytic shapes carry perimeter coordinates, raster/boolean fields can use projected phase.
float4 Effect(EffectContext ctx) {
    if(!HasInput(0)) return float4(0,0,0,0);
    float4 f=Input(0,ctx.px);float full=double_line>.5?2*width+double_gap:width;
    float shift=alignment==0?-full*.5:(alignment==2?full*.5:0);
    float cross=f.r-shift;
    if(double_line>.5) cross=abs(cross)-(double_gap+width)*.5;
    float across=abs(cross)-width*.5;
    float s=f.g,total=f.b;
    float2 p=(ctx.uv-center)*ctx.size;
    if(path_mode==1) { total=6.28318530718*max(length(p),1); s=frac((atan2(p.y,p.x)+1.57079632679)/6.28318530718)*total; }
    else if(path_mode==2 || total<=.00001) { float a=radians(angle);s=dot(p,float2(cos(a),sin(a)));total=0; }
    float period=max(dash+gap,.00001),dashWidth=dash;
    if(fit>.5 && total>0) { float count=max(floor(total/period+.5),1); float old=period;period=total/count;dashWidth*=period/old; }
    float u=(frac((s+phase)/period+.5)-.5)*period;
    float d=across;
    if(pattern==1) {
        if(caps==0) d=max(across,abs(u)-dashWidth*.5);
        else { float2 v=float2(abs(u)-max(dashWidth-width,0)*.5,abs(cross));d=length(max(v,0))+min(max(v.x,v.y),0)-width*.5; }
    } else if(pattern==2) d=length(float2(u,cross))-width*.5;
    float coverage=softness>0?saturate(.5-d/softness):(d<=0?1.0:0.0);
    float ramp=gradient>.5 && total>0?saturate(s/total):0;float4 c=lerp(color_a,color_b,ramp);
    return float4(c.rgb*c.a,c.a)*coverage*opacity;
}
