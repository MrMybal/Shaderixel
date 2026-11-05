//@name UI Blend Clipped Layer
//@category UI UV/Blend
//@blend
//@premultiplied
//@input Background
//@input Foreground
//@input Mask
//@param opacity float 1 0 1 "Foreground opacity"
//@param clip_to_background bool 1 "Clip to background alpha"
//@param invert_mask bool 0 "Invert mask"
//@param mode enum 0 "Normal|Add|Multiply|Screen" "Blend mode"

// Premultiplied over/add/multiply/screen with optional alpha clipping and an independent mask.
float4 Effect(EffectContext ctx) {
float4 a=Input(0,ctx.px), b=Input(1,ctx.px);
float mask=HasInput(2)?saturate(Input(2,ctx.px).r):1;
if (invert_mask>.5) mask=1-mask;
b*=opacity*mask*(clip_to_background>.5?a.a:1);
float3 ca=a.a>.000001?a.rgb/a.a:float3(0,0,0),cb=b.a>.000001?b.rgb/b.a:float3(0,0,0);
float3 blended=cb;
if (mode>.5 && mode<1.5) blended=ca+cb;
else if (mode>1.5 && mode<2.5) blended=ca*cb;
else if (mode>2.5) blended=1-(1-ca)*(1-cb);
float3 rgb=(1-b.a)*a.rgb+(1-a.a)*b.rgb+a.a*b.a*blended;
return float4(rgb,b.a+a.a*(1-b.a));
}
