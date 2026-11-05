//@name UI UV Displacement
//@category UI UV/Coordinates
//@halo full
//@input UV Map
//@input Displacement
//@input Mask
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param strength float2 16 16 -4096 4096 "Displacement X / Y (px)"
//@param neutral float2 0.5 0.5 0 1 "Neutral displacement RG"
//@param sampling enum 0 "Document space|Mapped UV space" "Displacement lookup"

// RG displacement in document or mapped space, with optional strength mask and neutral-value control.
// Explicit interpolation keeps float32 texture filtering consistent across backends.
float4 UIReadLinear(uint index, float2 uv, float2 size) {
    float2 p = uv * size - 0.5;
    int2 base = int2(floor(p));
    float2 f = frac(p);
    float4 top = lerp(Input(index, base), Input(index, base + int2(1, 0)), f.x);
    float4 bottom = lerp(Input(index, base + int2(0, 1)), Input(index, base + int2(1, 1)), f.x);
    return lerp(top, bottom, f.y);
}

float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
if (!HasInput(1)) return float4(m.rg,m.b,1);
float2 size=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.001);
float2 d=sampling>0.5?UIReadLinear(1,m.rg,ctx.size).rg:Input(1,ctx.px).rg;
float mask=HasInput(2)?saturate(Input(2,ctx.px).r):1;
return float4(m.rg+(d-neutral)*2*strength/size*mask,m.b,1);
}
