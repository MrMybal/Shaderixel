//@name UI Draw Interface Pattern
//@category UI Toolkit/Patterns
//@generator
//@premultiplied
//@param pattern enum 0 "Hatch|Chevrons|Dots|Technical grid|Hex grid|Panel lines|Scanlines|Crosshairs" "Pattern"
//@param space_size float2 0 0 0 32768 "Pattern space size (px; 0 = canvas)"
//@param pitch float2 32 32 0.001 8192 "Pattern pitch X / Y (px)"
//@param offset float2 0 0 -100000 100000 "Pattern offset (px)"
//@param angle float 0 -3600 3600 "Rotation (degrees)"
//@param width float 1.5 0.001 1024 "Line / dot width (px)"
//@param radius float 6 0 4096 "Panel corner radius (px)"
//@param major_step float 4 1 128 "Major grid interval"
//@param major_width float 3 0 1024 "Major grid width (px)"
//@param softness float 1 0 64 "Edge softness (px)"
//@param foreground color 0.06 0.4 0.9 0.85 "Foreground color"
//@param background color 0 0 0 0 "Background color"
//@param major_color color 0.3 0.65 1 1 "Major grid color"
//@param opacity float 1 0 1 "Opacity"

// Eight antialiased UI overlay patterns with pixel pitch, major grid lines, rotation, offset and transparent backgrounds.
float Box(float2 p,float2 h,float r) { r=min(r,min(h.x,h.y));float2 v=abs(p)-h+r;return length(max(v,0))+min(max(v.x,v.y),0)-r; }
float Coverage(float d) { return softness>0?saturate(.5-d/softness):(d<=0?1.0:0.0); }
float4 PM(float4 c) { return float4(c.rgb*c.a,c.a); }
float4 Effect(EffectContext ctx) {
 float2 space=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.00001);
 float2 p=ctx.uv*space+offset;float a=radians(angle),s=sin(a),c=cos(a);p=float2(c*p.x+s*p.y,-s*p.x+c*p.y);
 float2 period=max(pitch,.00001),local=(frac(p/period+.5)-.5)*period;
 float d=abs(local.x)-width*.5,major=0;
 if(pattern==1) { float slope=period.x/max(period.y,.00001);d=abs((frac((p.x-abs(local.y)*slope)/period.x+.5)-.5)*period.x)/sqrt(1+slope*slope)-width*.5; }
 else if(pattern==2) d=length(local)-width*.5;
 else if(pattern==3) {
  d=min(abs(local.x),abs(local.y))-width*.5;
  float2 mp=period*max(floor(major_step),1),ml=(frac(p/mp+.5)-.5)*mp;
  major=Coverage(min(abs(ml.x),abs(ml.y))-major_width*.5);
 } else if(pattern==4) {
  float r=max(period.x*.5,.001),sx=1.73205080757*r,sy=1.5*r,best=1e30;float2 rel=0;
  float row=floor(p.y/sy);
  for(int y=-1;y<=1;++y) {
   float yy=row+y,col=floor(p.x/sx-yy*.5);
   for(int x=-1;x<=1;++x) {
    float2 q=p-float2((col+x+yy*.5)*sx,yy*sy);float v=dot(q,q);
    if(v<best) { best=v;rel=q; }
   }
  }
  float edge=max(abs(rel.x),max(abs(dot(rel,float2(.5,.86602540378))),abs(dot(rel,float2(-.5,.86602540378)))))-.86602540378*r;
  d=abs(edge)-width*.5;
 } else if(pattern==5) d=abs(Box(local,period*.43,min(radius,min(period.x,period.y)*.4)))-width*.5;
 else if(pattern==6) d=abs(local.y)-width*.5;
 else if(pattern==7) d=min(Box(local,float2(period.x*.22,width*.5),0),Box(local,float2(width*.5,period.y*.22),0));
 float coverage=Coverage(d);float4 fg=lerp(PM(foreground),PM(major_color),major),bg=PM(background);
 coverage=max(coverage,major);return lerp(bg,fg,coverage)*opacity;
}
