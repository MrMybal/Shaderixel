//@name UI UV Wave
//@category UI UV/Coordinates
//@input UV Map
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param amplitude float2 8 0 -4096 4096 "Wave amplitude X / Y (px)"
//@param frequency float2 3 3 -100 100 "Wave cycles across V / U"
//@param phase float2 0 0 -100000 100000 "Phase X / Y (cycles)"

// Cross-axis sine warp in pixel units. Phase is explicit and can be driven by graph Time inputs.
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float2 size=max(float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y),.001);
float2 wave=sin((m.gr*frequency+phase)*6.28318530718)*amplitude/size;
return float4(m.rg+wave,m.b,1);
}
