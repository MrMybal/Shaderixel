//@name UI UV Texture Sampler
//@category UI UV/Sampling
//@premultiplied
//@halo full
//@input Image
//@input UV Map
//@input Mask
//@param address_x enum 0 "Clamp|Repeat|Mirror|Transparent" "U addressing"
//@param address_y enum 0 "Clamp|Repeat|Mirror|Transparent" "V addressing"
//@param filter enum 0 "Bilinear|Nearest" "Texture filtering"
//@param tint color 1 1 1 1 "Tint (linear RGBA)"
//@param opacity float 1 0 1 "Opacity"

// Final premultiplied image lookup. Image input 0, coordinate field input 1, optional mask input 2.
// Explicit interpolation keeps float32 texture filtering consistent across backends.
float4 UIReadLinear(uint index, float2 uv, float2 size) {
    float2 p = uv * size - 0.5;
    int2 base = int2(floor(p));
    float2 f = frac(p);
    float4 top = lerp(Input(index, base), Input(index, base + int2(1, 0)), f.x);
    float4 bottom = lerp(Input(index, base + int2(0, 1)), Input(index, base + int2(1, 1)), f.x);
    return lerp(top, bottom, f.y);
}

float Address(float v,float mode) {
if (mode<.5||mode>2.5) return saturate(v);
return mode<1.5?frac(v):1-abs(frac(v*.5)*2-1);
}
float4 Effect(EffectContext ctx) {
if (!HasInput(0)) return float4(0,0,0,0);
float4 m=HasInput(1)?Input(1,ctx.px):float4(ctx.uv,1,1);
float coverage=saturate(m.b);
if (address_x>2.5 && (m.r<0||m.r>1)) coverage=0;
if (address_y>2.5 && (m.g<0||m.g>1)) coverage=0;
float2 q=float2(Address(m.r,address_x),Address(m.g,address_y));
float4 color=filter>.5?Input(0,int2(floor(q*ctx.size))):UIReadLinear(0,q,ctx.size);
float mask=HasInput(2)?saturate(Input(2,ctx.px).r):1;
return color*float4(tint.rgb*tint.a,tint.a)*(coverage*mask*opacity);
}
