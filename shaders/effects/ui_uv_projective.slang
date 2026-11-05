//@name UI UV Perspective Quad
//@category UI UV/Coordinates
//@generator
//@param top_left float2 0.15 0.2 -4 4 "Top left (document UV)"
//@param top_right float2 0.85 0.1 -4 4 "Top right (document UV)"
//@param bottom_right float2 0.8 0.85 -4 4 "Bottom right (document UV)"
//@param bottom_left float2 0.2 0.8 -4 4 "Bottom left (document UV)"
//@param softness float 1 0 16 "Edge softness (px)"

// Inverse homography for perspective-correct UI cards. Degenerate and nonconvex quads produce zero coverage.
float Cross2(float2 a,float2 b) { return a.x*b.y-a.y*b.x; }
float4 Effect(EffectContext ctx) {
float2 p0=top_left,p1=top_right,p2=bottom_right,p3=bottom_left;
float2 d1=p1-p2,d2=p3-p2,d3=p0-p1+p2-p3;
float det=Cross2(d1,d2);
if (abs(det)<1e-9) return float4(0,0,0,1);
float g=Cross2(d3,d2)/det,h=Cross2(d1,d3)/det;
float2 a=p1-p0+g*p1,b=p3-p0+h*p3,c=p0;
float2 aa=a-ctx.uv*g,bb=b-ctx.uv*h,dd=ctx.uv-c;
float den=Cross2(aa,bb);
float s0=Cross2(p1-p0,p2-p1),s1=Cross2(p2-p1,p3-p2),s2=Cross2(p3-p2,p0-p3),s3=Cross2(p0-p3,p1-p0);
if (abs(den)<1e-9 || s0*s1<=0 || s0*s2<=0 || s0*s3<=0) return float4(0,0,0,1);
float2 q=float2(Cross2(dd,bb),Cross2(aa,dd))/den;
float winding=s0>0?1.0:-1.0;
float2 px=ctx.uv*ctx.size;
float2 corners[4]={p0*ctx.size,p1*ctx.size,p2*ctx.size,p3*ctx.size};
float edge=1e10;
for (int i=0;i<4;++i) { float2 v=corners[(i+1)%4]-corners[i]; edge=min(edge,winding*Cross2(v,px-corners[i])/max(length(v),.0001)); }
float coverage=softness>0?saturate(edge/softness+.5):(edge>=0?1.0:0.0);
return float4(q,coverage,1);
}
