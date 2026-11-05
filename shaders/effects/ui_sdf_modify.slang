//@name UI SDF Modify
//@category UI Toolkit/SDF
//@input Distance Field
//@param grow float 0 -4096 4096 "Grow / shrink (px)"
//@param shell bool 0 "Make hollow shell"
//@param thickness float 8 0.001 4096 "Shell thickness (px)"
//@param invert bool 0 "Invert inside / outside"

// Grow, shrink, invert or turn any distance field into an editable hollow shell.
float4 Effect(EffectContext ctx) {
    float4 f=HasInput(0)?Input(0,ctx.px):float4(1e6,0,0,1);
    float d=f.r-grow;
    if(shell>.5) d=abs(d)-thickness*.5;
    if(invert>.5) d=-d;
    return float4(d,f.gb,1);
}
