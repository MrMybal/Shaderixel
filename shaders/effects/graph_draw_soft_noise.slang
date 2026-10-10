//@name Graph Soft Noise
//@category Graph Layers
//@graph {"name":"Graph Soft Noise","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":28,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.015,0.04,0.1,1],"name":"color_a","text":"Low Color","min":0,"max":1,"index":1},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0.3,0.55,0.8,1],"name":"color_b","text":"High Color","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[64,0,0,0],"name":"scale","text":"Noise Scale (px)","min":2,"max":512,"index":1},{"id":4,"op":"param2","pos":[40,430],"in":[],"value":[0,0,0,0],"name":"offset","text":"Noise Offset (px)","min":-4096,"max":4096,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[1,0,0,0],"name":"contrast","text":"Noise Contrast","min":0.1,"max":4,"index":1},{"id":6,"op":"pixel","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":7,"op":"const","pos":[40,820],"in":[],"value":[0.5,0,0,0]},{"id":8,"op":"add","pos":[280,40],"in":[[6,0],[7,0]],"value":[0,0,0,0]},{"id":9,"op":"add","pos":[520,40],"in":[[8,0],[4,0]],"value":[0,0,0,0]},{"id":10,"op":"const","pos":[40,950],"in":[],"value":[0.0001,0,0,0]},{"id":11,"op":"max","pos":[280,170],"in":[[3,0],[10,0]],"value":[0,0,0,0]},{"id":12,"op":"div","pos":[760,40],"in":[[9,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"const","pos":[40,1080],"in":[],"value":[1,0,0,0]},{"id":14,"op":"noise","pos":[1000,40],"in":[[12,0],[13,0]],"value":[0,0,0,0]},{"id":15,"op":"const","pos":[40,1210],"in":[],"value":[0.5,0,0,0]},{"id":16,"op":"sub","pos":[1240,40],"in":[[14,0],[15,0]],"value":[0,0,0,0]},{"id":17,"op":"mul","pos":[1480,40],"in":[[16,0],[5,0]],"value":[0,0,0,0]},{"id":18,"op":"const","pos":[40,1340],"in":[],"value":[0.5,0,0,0]},{"id":19,"op":"add","pos":[1720,40],"in":[[17,0],[18,0]],"value":[0,0,0,0]},{"id":20,"op":"const","pos":[40,1470],"in":[],"value":[0,0,0,0]},{"id":21,"op":"const","pos":[40,1600],"in":[],"value":[1,0,0,0]},{"id":22,"op":"clamp","pos":[1960,40],"in":[[19,0],[20,0],[21,0]],"value":[0,0,0,0]},{"id":23,"op":"lerp","pos":[2200,40],"in":[[1,0],[2,0],[22,0]],"value":[0,0,0,0]},{"id":24,"op":"mask","pos":[2440,40],"in":[[23,0]],"value":[0,0,0,0],"index":8},{"id":25,"op":"const","pos":[40,1730],"in":[],"value":[0,0,0,0]},{"id":26,"op":"const","pos":[40,1860],"in":[],"value":[1,0,0,0]},{"id":27,"op":"output","pos":[2680,40],"in":[[23,0],[0,0],[24,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param color_a color 0.015 0.04 0.1 1.0 "Low Color"
//@param color_b color 0.3 0.55 0.8 1.0 "High Color"
//@param scale float 64.0 2.0 512.0 "Noise Scale (px)"
//@param offset float2 0.0 0.0 -4096.0 4096.0 "Noise Offset (px)"
//@param contrast float 1.0 0.1 4.0 "Noise Contrast"

// Smooth deterministic value noise with pixel scale, offset, color ramp and contrast.
float GraphNoise(float2 p)
{
    float2 i = floor(p), t = frac(p);
    t = t * t * (3.0 - 2.0 * t);
    return lerp(lerp(Hash12(i),Hash12(i+float2(1,0)),t.x),lerp(Hash12(i+float2(0,1)),Hash12(i+float2(1,1)),t.x),t.y);
}
float4 Effect(EffectContext ctx)
{
    float2 g6_0 = float2(ctx.px);
    float2 g8_0 = (g6_0+(float2)(0.5));
    float2 g9_0 = (g8_0+offset);
    float g11_0 = max(scale,0.0001);
    float2 g12_0 = (g9_0/(float2)(g11_0));
    float g14_0 = GraphNoise(g12_0*1.0);
    float g16_0 = (g14_0-0.5);
    float g17_0 = (g16_0*contrast);
    float g19_0 = (g17_0+0.5);
    float g22_0 = clamp(g19_0,0.0,1.0);
    float4 g23_0 = lerp(color_a,color_b,g22_0);
    float g24_0 = (g23_0).w;
    return float4((g23_0).rgb,g24_0);
}
