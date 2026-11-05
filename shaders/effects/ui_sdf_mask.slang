//@name UI SDF Mask and Preview
//@category UI Toolkit/SDF
//@input Distance Field
//@param view enum 0 "Fill mask|Inner border|Outer border|Centered border|Distance preview|Contour phase" "Output"
//@param grow float 0 -4096 4096 "Grow / shrink (px)"
//@param width float 4 0.001 4096 "Border width (px)"
//@param softness float 1 0 256 "Edge softness (px)"
//@param range float 32 0.001 32768 "Distance preview range (px)"
//@param invert bool 0 "Invert mask"

// Convert a data field into fill/border masks or a visible distance/contour preview; not a reusable field output.
float4 Effect(EffectContext ctx) {
    float4 f=HasInput(0)?Input(0,ctx.px):float4(1e6,0,0,1);float d=f.r-grow;
    float v;
    if(view==4) v=saturate(.5+d/(2*max(range,.00001)));
    else if(view==5) v=f.b>0?frac(f.g/f.b):0;
    else {
        if(view==1) d=abs(d+width*.5)-width*.5;
        else if(view==2) d=abs(d-width*.5)-width*.5;
        else if(view==3) d=abs(d)-width*.5;
        v=softness>0?saturate(.5-d/softness):(d<=0?1.0:0.0);
        if(invert>.5) v=1-v;
    }
    return float4(v,v,v,1);
}
