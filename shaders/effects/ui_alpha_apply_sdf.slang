//@name UI Alpha Grow Shrink and Feather
//@category UI Toolkit/Alpha
//@premultiplied
//@halo full
//@input Image
//@input Distance Field
//@input Mask
//@param grow float 0 -4096 4096 "Grow / shrink (px)"
//@param softness float 1 0.001 1024 "Edge feather width (px)"
//@param combine enum 0 "Rebuild alpha|Union with original|Intersect with original" "Alpha combination"
//@param extend_rgb bool 1 "Extend nearest interior RGB"
//@param opacity float 1 0 1 "Opacity"

// Rebuild, grow, shrink or feather alpha using a distance field; project new edge colors toward the nearest interior.
float4 UILinear(uint index,float2 q,float2 size) {
 float2 p=q*size-.5;int2 b=int2(floor(p));float2 t=frac(p);
 return lerp(lerp(Input(index,b),Input(index,b+int2(1,0)),t.x),lerp(Input(index,b+int2(0,1)),Input(index,b+int2(1,1)),t.x),t.y);
}

float4 Effect(EffectContext ctx) {
 float4 c=Input(0,ctx.px);if(!HasInput(1)) return c;
 float d=Input(1,ctx.px).r;float alpha=saturate(.5-(d-grow)/max(softness,.00001));
 if(combine==1) alpha=max(alpha,c.a);else if(combine==2) alpha=min(alpha,c.a);
 float3 rgb=Unpremultiply(c).rgb;
 if(extend_rgb>.5 && c.a<=.000001 && alpha>0) {
  float2 n=float2(Input(1,ctx.px+int2(1,0)).r-Input(1,ctx.px-int2(1,0)).r,Input(1,ctx.px+int2(0,1)).r-Input(1,ctx.px-int2(0,1)).r);
  n=length(n)>.00001?normalize(n):float2(1,0);float4 best=float4(0,0,0,0);
  for(int i=1;i<=4;++i) { float4 s=UILinear(0,ctx.uv-n*(max(d,0)+i)/ctx.size,ctx.size);if(s.a>best.a) best=s; }
  if(best.a>.000001) rgb=Unpremultiply(best).rgb;
 }
 float mask=HasInput(2)?saturate(Input(2,ctx.px).r):1;
 return lerp(c,float4(rgb*alpha,alpha)*opacity,mask);
}
