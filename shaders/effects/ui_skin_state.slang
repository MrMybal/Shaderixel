//@name UI Skin Interaction State
//@category UI Toolkit/Skins
//@premultiplied
//@halo full
//@input Image
//@input Distance Field
//@input Mask
//@param hover float 0 0 1 "Hover amount"
//@param pressed float 0 0 1 "Pressed amount"
//@param selected float 0 0 1 "Selected amount"
//@param disabled float 0 0 1 "Disabled amount"
//@param focus float 0 0 1 "Focus amount"
//@param hover_tint color 1.15 1.3 1.5 1 "Hover tint"
//@param pressed_tint color 0.7 0.75 0.85 1 "Pressed tint"
//@param selected_color color 0.05 0.4 1 1 "Selected accent color"
//@param selected_mix float 0.35 0 1 "Selected color mix"
//@param pressed_offset float2 0 1 -4096 4096 "Pressed content offset (px)"
//@param disabled_saturation float 0 0 1 "Disabled saturation"
//@param disabled_opacity float 0.45 0 1 "Disabled opacity"
//@param focus_width float 3 0 4096 "Focus ring width (px)"
//@param focus_softness float 1 0.001 256 "Focus ring softness (px)"
//@param focus_color color 0.08 0.55 1 1 "Focus ring color"
//@param opacity float 1 0 1 "Overall opacity"

// Continuous normal/hover/pressed/selected/disabled styling with pressed offsets, desaturation and an optional distance-field focus ring.
int2 UIPixelAddress(int2 p,int2 size,int mode) {
 if(mode==1) return (p%size+size)%size;
 if(mode==2) { int2 v=(p%(2*size)+2*size)%(2*size);return min(v,2*size-1-v); }
 return clamp(p,int2(0,0),size-1);
}
float4 UIPixel(uint index,int2 p,int2 size,int mode) {
 if(mode==3 && (any(p<0)||any(p>=size))) return float4(0,0,0,0);
 return Input(index,UIPixelAddress(p,size,mode));
}
float4 UIAddressed(uint index,float2 q,float2 size,int mode) {
 // Reduce large periodic coordinates before conversion to integer texels.
 if(mode==1) q=frac(q);
 else if(mode==2) q=(q*.5-floor(q*.5))*2;
 else q=clamp(q,-1,2);
 float2 p=q*size-.5;int2 b=int2(floor(p));float2 t=frac(p);int2 dim=int2(size);
 return lerp(lerp(UIPixel(index,b,dim,mode),UIPixel(index,b+int2(1,0),dim,mode),t.x),lerp(UIPixel(index,b+int2(0,1),dim,mode),UIPixel(index,b+int2(1,1),dim,mode),t.x),t.y);
}

float4 Effect(EffectContext ctx) {
 float mask=HasInput(2)?saturate(Input(2,ctx.px).r):1;
 float h=saturate(hover)*mask,p=saturate(pressed)*mask,s=saturate(selected)*mask,d=saturate(disabled)*mask;
 float2 q=ctx.uv-pressed_offset*p/ctx.size;float4 color=HasInput(0)?UIAddressed(0,q,ctx.size,3):float4(0,0,0,0);
 float3 rgb=Unpremultiply(color).rgb;
 rgb*=lerp(float3(1,1,1),hover_tint.rgb,h)*lerp(float3(1,1,1),pressed_tint.rgb,p);
 rgb=lerp(rgb,selected_color.rgb,s*selected_mix);
 float gray=Luma(rgb);rgb=lerp(rgb,lerp(float3(gray,gray,gray),rgb,disabled_saturation),d);
 float alpha=color.a*lerp(1,hover_tint.a,h)*lerp(1,pressed_tint.a,p)*lerp(1,selected_color.a,s)*lerp(1,disabled_opacity,d);
 float4 outColor=float4(rgb*alpha,alpha);
 if(HasInput(1) && focus_width>0) {
  float distance=UIAddressed(1,q,ctx.size,0).r;
  float ring=saturate(.5-(abs(distance-focus_width*.5)-focus_width*.5)/max(focus_softness,.00001));
  float amount=max(s,saturate(focus)*mask)*lerp(1,disabled_opacity,d);
  float4 stroke=float4(focus_color.rgb*focus_color.a,focus_color.a)*ring*amount;
  outColor+=stroke*(1-outColor.a);
 }
 return outColor*opacity;
}
