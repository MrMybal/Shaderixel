//@name UI UV Pixel Snap
//@category UI Toolkit/Layout
//@input UV Map
//@param space_size float2 0 0 0 32768 "Reference size (px; 0 = canvas)"
//@param grid float2 1 1 1e-05 8192 "Snap grid X / Y (px)"
//@param origin float2 0.5 0.5 -8192 8192 "Grid origin (px)"
//@param mode enum 0 "Nearest|Floor|Ceil" "Snap mode"
//@param amount float 1 0 1 "Snap amount"

// Quantize UV lookups to texel centers or a fractional pixel grid while preserving widget coverage.
float4 Effect(EffectContext ctx) {
 float4 m=HasInput(0)?Input(0,ctx.px):float4(ctx.uv,1,1);
 float2 size=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.00001),step=max(abs(grid),.00001);
 float2 n=(m.rg*size-origin)/step,s=sign(n)*floor(abs(n)+.5);
 if(mode==1) s=floor(n);else if(mode==2) s=ceil(n);
 return float4(lerp(m.rg,(s*step+origin)/size,saturate(amount)),m.b,1);
}
