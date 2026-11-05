//@name UI Transition
//@category UI Toolkit/Motion
//@blend
//@premultiplied
//@halo full
//@input Image A
//@input Image B
//@input Mask
//@input UV Map
//@param mode enum 0 "Crossfade|Slide|Wipe|Iris|Dissolve|Zoom" "Transition"
//@param progress float 0.5 0 1 "Progress"
//@param angle float 0 -3600 3600 "Direction angle (degrees)"
//@param softness float 2 0 1024 "Edge softness (px)"
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param center float2 0.5 0.5 -4 4 "Iris / zoom center (local UV)"
//@param block_size float 8 0.25 4096 "Dissolve block size (px)"
//@param seed float 0 -100000 100000 "Dissolve seed"
//@param zoom float 0.4 0 8 "Zoom travel"
//@param min_scale float 0.2 0.001 1 "Incoming minimum scale"
//@param opacity float 1 0 1 "Opacity"
//@param output_mode enum 0 "Image|Transition mask" "Output"

// Six premultiplied transitions with exact progress endpoints, optional local UV field and region mask.
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
 float4 m=HasInput(3)?Input(3,ctx.px):float4(ctx.uv,1,1);float2 q=m.rg;
 float2 size=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.00001);
 float p=saturate(progress),a=radians(angle);float2 dir=float2(cos(a),sin(a));
 float4 original=HasInput(0)?UIAddressed(0,q,ctx.size,0):float4(0,0,0,0);
 float4 next=HasInput(1)?UIAddressed(1,q,ctx.size,0):float4(0,0,0,0),color=lerp(original,next,p);
 float amount=p;
 if(mode==1) {
  float2 aq=q+dir*p,bq=q+dir*(p-1);
  float4 ca=HasInput(0)?UIAddressed(0,aq,ctx.size,3):float4(0,0,0,0);
  float4 cb=HasInput(1)?UIAddressed(1,bq,ctx.size,3):float4(0,0,0,0);
  color=cb+ca*(1-cb.a);amount=all(bq>=0)&&all(bq<=1)?1.0:0.0;
 } else if(mode==2) {
  float span=max(dot(abs(dir),size),.00001),t=dot((q-.5)*size,dir)/span+.5;
  amount=softness>0?saturate((p-t)*span/softness+.5):(t<=p?1.0:0.0);color=lerp(original,next,amount);
 } else if(mode==3) {
  float2 far=max(abs(center),abs(1-center))*size;float limit=length(far),d=length((q-center)*size);
  amount=softness>0?saturate((p*limit-d)/softness+.5):(d<=p*limit?1.0:0.0);color=lerp(original,next,amount);
 } else if(mode==4) {
  float noise=Hash12(floor(q*size/max(block_size,.00001))+float2(seed,seed*.173));
  amount=softness>0?saturate((p-noise)*max(min(size.x,size.y),1)/softness+.5):(noise<=p?1.0:0.0);color=lerp(original,next,amount);
 } else if(mode==5) {
  float2 aq=center+(q-center)/max(1+p*zoom,.00001),bq=center+(q-center)/max(lerp(min_scale,1,p),.00001);
  float4 ca=HasInput(0)?UIAddressed(0,aq,ctx.size,0):float4(0,0,0,0);
  float4 cb=HasInput(1)?UIAddressed(1,bq,ctx.size,3):float4(0,0,0,0);color=lerp(ca,cb,p);
 }
 if(p<=0) { color=original;amount=0; } else if(p>=1) { color=next;amount=1; }
 float mask=HasInput(2)?saturate(Input(2,ctx.px).r):1;
 if(output_mode==1) { float v=amount*mask*saturate(m.b);return float4(v,v,v,1); }
 return lerp(original,color,mask)*saturate(m.b)*opacity;
}
