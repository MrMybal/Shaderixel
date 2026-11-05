//@name UI SDF Boolean
//@category UI Toolkit/SDF
//@input Field A
//@input Field B
//@param operation enum 0 "Union|Intersection|Subtract B|XOR|Smooth union|Smooth intersection|Smooth subtract" "Operation"
//@param smoothness float 12 0 1024 "Blend radius (px)"
//@param grow_a float 0 -4096 4096 "Grow field A (px)"
//@param grow_b float 0 -4096 4096 "Grow field B (px)"

// Hard and smooth field operations. Contour coordinates remain local to the contributing source contour.
float SmoothMin(float a,float b,float k) {
    if(k<=.00001) return min(a,b);
    float h=saturate(.5+.5*(b-a)/k);return lerp(b,a,h)-k*h*(1-h);
}
float4 Effect(EffectContext ctx) {
    float4 a=HasInput(0)?Input(0,ctx.px):float4(1e6,0,0,1);
    float4 b=HasInput(1)?Input(1,ctx.px):float4(1e6,0,0,1);
    a.r-=grow_a;b.r-=grow_b;
    float d=min(a.r,b.r);
    if(operation==1) d=max(a.r,b.r);
    else if(operation==2) d=max(a.r,-b.r);
    else if(operation==3) d=max(min(a.r,b.r),-max(a.r,b.r));
    else if(operation==4) d=SmoothMin(a.r,b.r,smoothness);
    else if(operation==5) d=-SmoothMin(-a.r,-b.r,smoothness);
    else if(operation==6) d=-SmoothMin(-a.r,b.r,smoothness);
    bool useA=abs(d-a.r)<=min(abs(d-b.r),abs(d+b.r));
    float2 contour=useA?a.gb:b.gb;
    return float4(d,contour,1);
}
