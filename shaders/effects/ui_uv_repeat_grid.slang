//@name UI UV Repeated Grid
//@category UI UV/Coordinates
//@input UV Map
//@param space_size float2 0 0 0 32768 "Local space size (px; 0 = canvas)"
//@param tile_size float2 48 48 0.001 32768 "Tile size (px)"
//@param gap float2 8 8 0 32768 "Gap (px)"
//@param offset float2 0 0 -32768 32768 "Scroll offset (px)"
//@param stagger float 0 -1 1 "Alternate row stagger (tiles)"
//@param mirror_x bool 0 "Mirror alternate columns"
//@param mirror_y bool 0 "Mirror alternate rows"
//@param softness float 1 0 16 "Tile edge softness (px)"

// Pixel-sized repeating icons or strips with gaps, stagger, mirroring and explicit scroll offsets.
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float2 space=float2(space_size.x>0?space_size.x:ctx.size.x,space_size.y>0?space_size.y:ctx.size.y);
float2 period=max(tile_size+gap,.0001), p=m.rg*space+offset;
float row=floor(p.y/period.y); p.x+=fmod(abs(row),2)*stagger*period.x;
float2 index=floor(p/period), local=frac(p/period)*period;
float2 q=local/max(tile_size,.0001);
float edge=min(min(local.x,local.y),min(tile_size.x-local.x,tile_size.y-local.y));
float coverage=softness>0?saturate(edge/softness+0.5):(edge>=0?1.0:0.0);
if (mirror_x>0.5 && fmod(abs(index.x),2)>0.5) q.x=1-q.x;
if (mirror_y>0.5 && fmod(abs(index.y),2)>0.5) q.y=1-q.y;
return float4(q,m.b*coverage,1);
}
