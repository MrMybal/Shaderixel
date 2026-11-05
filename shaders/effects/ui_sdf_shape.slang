//@name UI SDF Shape
//@category UI Toolkit/SDF
//@generator
//@param shape enum 1 "Circle|Rounded rectangle|Capsule|Triangle|Polygon|Star|Arrow" "Shape"
//@param position float2 0.5 0.5 -4 4 "Shape center (document UV)"
//@param size float2 160 100 0.001 32768 "Shape size (px)"
//@param rotation float 0 -3600 3600 "Rotation (degrees)"
//@param radius float 18 0 16384 "Corner radius (px)"
//@param sides float 6 3 32 "Polygon sides / star points"
//@param inner_ratio float 0.45 0.01 0.99 "Star inner radius ratio"
//@param shaft float 0.35 0.01 0.99 "Arrow shaft half thickness"
//@param head float 0.2 -0.9 0.9 "Arrow head start"

// Seven analytic shapes with signed pixel distances and perimeter coordinates for uniform dashed contours.
// SDF data: R=signed distance in pixels (negative inside), G=contour arclength,
// B=contour perimeter, A=1. Keep fields in float32; this is not a UV map.
float Segment(float2 p,float2 a,float2 b,out float t) {
    float2 v=b-a; t=saturate(dot(p-a,v)/max(dot(v,v),1e-12));
    return length(p-a-v*t);
}
void SegmentCandidate(float2 p,float2 a,float2 b,float prefix,inout float best,inout float path) {
    float t; float d=Segment(p,a,b,t);
    if(d<best) { best=d; path=prefix+t*length(b-a); }
}
void ArcCandidate(float2 p,float2 c,float r,float begin,float end,float prefix,inout float best,inout float path) {
    if(r<=0.00001) return;
    float a=atan2(p.y-c.y,p.x-c.x);
    if(a<begin) a+=6.28318530718;
    if(a>end) { float d0=length(p-c-r*float2(cos(begin),sin(begin))); float d1=length(p-c-r*float2(cos(end),sin(end))); a=d0<d1?begin:end; }
    a=clamp(a,begin,end);
    float d=length(p-c-r*float2(cos(a),sin(a)));
    if(d<best) { best=d; path=prefix+(a-begin)*r; }
}
float3 RoundedBox(float2 p,float2 h,float r) {
    r=clamp(r,0,min(h.x,h.y)); float2 q=abs(p)-h+r;
    float d=length(max(q,0))+min(max(q.x,q.y),0)-r;
    float w=max(2*h.x-2*r,0),v=max(2*h.y-2*r,0),arc=1.57079632679*r;
    float best=1e20,path=0,prefix=0;
    SegmentCandidate(p,float2(-h.x+r,-h.y),float2(h.x-r,-h.y),prefix,best,path); prefix+=w;
    ArcCandidate(p,float2(h.x-r,-h.y+r),r,-1.57079632679,0,prefix,best,path); prefix+=arc;
    SegmentCandidate(p,float2(h.x,-h.y+r),float2(h.x,h.y-r),prefix,best,path); prefix+=v;
    ArcCandidate(p,float2(h.x-r,h.y-r),r,0,1.57079632679,prefix,best,path); prefix+=arc;
    SegmentCandidate(p,float2(h.x-r,h.y),float2(-h.x+r,h.y),prefix,best,path); prefix+=w;
    ArcCandidate(p,float2(-h.x+r,h.y-r),r,1.57079632679,3.14159265359,prefix,best,path); prefix+=arc;
    SegmentCandidate(p,float2(-h.x,h.y-r),float2(-h.x,-h.y+r),prefix,best,path); prefix+=v;
    ArcCandidate(p,float2(-h.x+r,-h.y+r),r,3.14159265359,4.71238898038,prefix,best,path); prefix+=arc;
    return float3(d,path,max(prefix,0.00001));
}
float2 Vertex(int i,int n,float2 h) {
    if(shape==3) { if(i==0) return float2(0,-h.y); if(i==1) return h; return float2(-h.x,h.y); }
    if(shape==6) {
        float2 points[7]={float2(-1,-shaft),float2(head,-shaft),float2(head,-1),float2(1,0),float2(head,1),float2(head,shaft),float2(-1,shaft)};
        return points[i]*h;
    }
    float a=-1.57079632679+6.28318530718*float(i)/float(n);
    float r=(shape==5 && (i&1)!=0)?inner_ratio:1.0;
    return float2(cos(a),sin(a))*h*r;
}
float4 Effect(EffectContext ctx) {
    float2 p=(ctx.uv-position)*ctx.size,h=max(size*.5,.00001);
    float a=radians(rotation),co=cos(a),si=sin(a); p=float2(co*p.x+si*p.y,-si*p.x+co*p.y);
    if(shape==0) { float r=min(h.x,h.y),s=frac((atan2(p.y,p.x)+1.57079632679)/6.28318530718); return float4(length(p)-r,s*6.28318530718*r,6.28318530718*r,1); }
    if(shape==1 || shape==2) return float4(RoundedBox(p,h,shape==2?min(h.x,h.y):radius),1);
    int count=shape==3?3:(shape==6?7:clamp(int(floor(sides)),3,32)*(shape==5?2:1));
    float best=1e20,path=0,prefix=0; bool inside=false;
    for(int i=0;i<count;++i) {
        float2 v0=Vertex(i,count,h),v1=Vertex((i+1)%count,count,h);
        float t; float dist=Segment(p,v0,v1,t),len=length(v1-v0);
        if(dist<best) { best=dist; path=prefix+t*len; }
        if((v0.y>p.y)!=(v1.y>p.y)) {
            float x=v0.x+(p.y-v0.y)*(v1.x-v0.x)/(v1.y-v0.y);
            if(p.x<x) inside=!inside;
        }
        prefix+=len;
    }
    return float4(inside?-best:best,path,max(prefix,.00001),1);
}
