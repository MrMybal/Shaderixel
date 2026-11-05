//@name UI UV Affine Transform
//@category UI UV/Coordinates
//@input UV Map
//@param offset float2 0 0 -1000 1000 "UV offset"
//@param repeat float2 1 1 -1000 1000 "UV scale / repeat"
//@param pivot float2 0.5 0.5 -4 4 "Pivot (local UV)"
//@param rotation float 0 -3600 3600 "Rotation (degrees)"
//@param shear float2 0 0 -10 10 "Shear X / Y"

// Transforms coordinates only; coverage remains in widget space. Chain addressing before the sampler.
float4 Effect(EffectContext ctx) {
float4 m = HasInput(0) ? Input(0, ctx.px) : float4(ctx.uv, 1, 1);
float2 p = m.rg - pivot;
float a = radians(rotation), s = sin(a), c = cos(a);
p = float2(c*p.x-s*p.y, s*p.x+c*p.y);
p = float2(p.x + shear.x*p.y, p.y + shear.y*p.x);
return float4(p * repeat + pivot + offset, m.b, 1);
}
