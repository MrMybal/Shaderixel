//@name UI UV Parallax
//@category UI Toolkit/Motion
//@input UV Map
//@input Depth
//@input Mask
//@param pointer float2 0.5 0.5 -4 4 "Driver position (normalized)"
//@param neutral float2 0.5 0.5 -4 4 "Neutral driver position"
//@param strength float2 24 16 -8192 8192 "Parallax travel X / Y (px)"
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param plane_depth float 0.5 -8 8 "Plane depth"
//@param depth_reference float 0.5 0 1 "Depth map neutral value"
//@param depth_amount float 1 -8 8 "Depth map influence"

// Explicit-position parallax coordinate field, with optional document-space depth and distortion masks; no pointer events are registered.
float4 Effect(EffectContext ctx) {
 float4 m=HasInput(0)?Input(0,ctx.px):float4(ctx.uv,1,1);
 float2 size=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.00001);
 float depth=plane_depth+(HasInput(1)?(Input(1,ctx.px).r-depth_reference)*depth_amount:0);
 float mask=HasInput(2)?saturate(Input(2,ctx.px).r):1;
 return float4(m.rg-(pointer-neutral)*2*strength/size*depth*mask,m.b,1);
}
