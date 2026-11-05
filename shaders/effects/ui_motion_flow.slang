//@name UI UV Flow Map
//@category UI Toolkit/Motion
//@premultiplied
//@halo full
//@input Image
//@input Flow Map
//@input UV Map
//@input Mask
//@param time float 0 -100000 100000 "Time (seconds)"
//@param speed float 1 -1000 1000 "Flow cycles per second"
//@param phase float 0 -100000 100000 "Phase (cycles)"
//@param strength float2 24 24 -8192 8192 "Flow travel X / Y (px)"
//@param neutral float2 0.5 0.5 -4 4 "Neutral flow RG"
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param flow_space enum 0 "Document|Mapped UV" "Flow map lookup space"
//@param address enum 1 "Clamp|Repeat|Mirror|Transparent" "Source addressing"
//@param tint color 1 1 1 1 "Tint"
//@param opacity float 1 0 1 "Opacity"
//@param output_mode enum 0 "Image|Phase A UV|Phase B UV|Blend weight|Flow vectors" "Output"

// Seamless two-phase flow-map animation with explicit time, periodic bilinear lookup, optional UV field and distortion mask.
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
 float4 m=HasInput(2)?Input(2,ctx.px):float4(ctx.uv,1,1);
 float2 space=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.00001);
 float4 flow=HasInput(1)?Unpremultiply(UIAddressed(1,flow_space==0?ctx.uv:m.rg,ctx.size,0)):float4(neutral,0,1);
 float mask=HasInput(3)?saturate(Input(3,ctx.px).r):1;
 float2 delta=(flow.rg-neutral)*2*strength/space*mask;
 float a=frac(time*speed+phase),b=frac(a+.5),weight=1-abs(2*a-1);
 float2 qa=m.rg-delta*(a-.5),qb=m.rg-delta*(b-.5);
 if(output_mode==1) return float4(qa,m.b,1);
 if(output_mode==2) return float4(qb,m.b,1);
 if(output_mode==3) return float4(weight,weight,weight,1);
 if(output_mode==4) return float4(flow.rg,0,1);
 if(!HasInput(0)) return float4(0,0,0,0);
 float4 color=lerp(UIAddressed(0,qb,ctx.size,address),UIAddressed(0,qa,ctx.size,address),weight);
 return color*float4(tint.rgb*tint.a,tint.a)*saturate(m.b)*opacity;
}
