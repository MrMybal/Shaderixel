//@name Graph Linear Gradient
//@category Graph Layers
//@graph {"name":"Graph Linear Gradient","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":36,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.015,0.04,0.15,1],"name":"color_a","text":"Start Color","min":0,"max":1,"index":1},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0.6,0.08,0.3,1],"name":"color_b","text":"End Color","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[90,0,0,0],"name":"angle","text":"Angle (degrees)","min":-180,"max":180,"index":1},{"id":4,"op":"param2","pos":[40,430],"in":[],"value":[0.5,0.5,0,0],"name":"center","text":"Center (UV)","min":0,"max":1,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[1,0,0,0],"name":"span","text":"Span (short image side)","min":0.01,"max":4,"index":1},{"id":6,"op":"size","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":7,"op":"mask","pos":[280,40],"in":[[6,0]],"value":[0,0,0,0],"index":1},{"id":8,"op":"mask","pos":[280,170],"in":[[6,0]],"value":[0,0,0,0],"index":2},{"id":9,"op":"min","pos":[520,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"pixel","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":11,"op":"const","pos":[40,950],"in":[],"value":[0.5,0,0,0]},{"id":12,"op":"add","pos":[280,300],"in":[[10,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"size","pos":[40,1080],"in":[],"value":[0,0,0,0]},{"id":14,"op":"mul","pos":[280,430],"in":[[13,0],[4,0]],"value":[0,0,0,0]},{"id":15,"op":"sub","pos":[520,170],"in":[[12,0],[14,0]],"value":[0,0,0,0]},{"id":16,"op":"const","pos":[40,1210],"in":[],"value":[0.017453292519943295,0,0,0]},{"id":17,"op":"mul","pos":[280,560],"in":[[3,0],[16,0]],"value":[0,0,0,0]},{"id":18,"op":"cos","pos":[520,300],"in":[[17,0]],"value":[0,0,0,0]},{"id":19,"op":"sin","pos":[520,430],"in":[[17,0]],"value":[0,0,0,0]},{"id":20,"op":"make2","pos":[760,40],"in":[[18,0],[19,0]],"value":[0,0,0,0]},{"id":21,"op":"dot","pos":[1000,40],"in":[[15,0],[20,0]],"value":[0,0,0,0]},{"id":22,"op":"mul","pos":[760,170],"in":[[9,0],[5,0]],"value":[0,0,0,0]},{"id":23,"op":"const","pos":[40,1340],"in":[],"value":[0.0001,0,0,0]},{"id":24,"op":"max","pos":[1000,170],"in":[[22,0],[23,0]],"value":[0,0,0,0]},{"id":25,"op":"div","pos":[1240,40],"in":[[21,0],[24,0]],"value":[0,0,0,0]},{"id":26,"op":"const","pos":[40,1470],"in":[],"value":[0.5,0,0,0]},{"id":27,"op":"add","pos":[1480,40],"in":[[25,0],[26,0]],"value":[0,0,0,0]},{"id":28,"op":"const","pos":[40,1600],"in":[],"value":[0,0,0,0]},{"id":29,"op":"const","pos":[40,1730],"in":[],"value":[1,0,0,0]},{"id":30,"op":"clamp","pos":[1720,40],"in":[[27,0],[28,0],[29,0]],"value":[0,0,0,0]},{"id":31,"op":"lerp","pos":[1960,40],"in":[[1,0],[2,0],[30,0]],"value":[0,0,0,0]},{"id":32,"op":"mask","pos":[2200,40],"in":[[31,0]],"value":[0,0,0,0],"index":8},{"id":33,"op":"const","pos":[40,1860],"in":[],"value":[0,0,0,0]},{"id":34,"op":"const","pos":[40,1990],"in":[],"value":[1,0,0,0]},{"id":35,"op":"output","pos":[2440,40],"in":[[31,0],[0,0],[32,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param color_a color 0.015 0.04 0.15 1.0 "Start Color"
//@param color_b color 0.6 0.08 0.3 1.0 "End Color"
//@param angle float 90.0 -180.0 180.0 "Angle (degrees)"
//@param center float2 0.5 0.5 0.0 1.0 "Center (UV)"
//@param span float 1.0 0.01 4.0 "Span (short image side)"

// Aspect-aware directional color and alpha gradient, assembled from native graph nodes.
float4 Effect(EffectContext ctx)
{
    float2 g6_0 = ctx.size;
    float g7_0 = (float4(g6_0,0.0,1.0)).x;
    float g8_0 = (float4(g6_0,0.0,1.0)).y;
    float g9_0 = min(g7_0,g8_0);
    float2 g10_0 = float2(ctx.px);
    float2 g12_0 = (g10_0+(float2)(0.5));
    float2 g13_0 = ctx.size;
    float2 g14_0 = (g13_0*center);
    float2 g15_0 = (g12_0-g14_0);
    float g17_0 = (angle*0.017453293);
    float g18_0 = cos(g17_0);
    float g19_0 = sin(g17_0);
    float2 g20_0 = float2(g18_0,g19_0);
    float g21_0 = dot(g15_0,g20_0);
    float g22_0 = (g9_0*span);
    float g24_0 = max(g22_0,0.0001);
    float g25_0 = (g21_0/g24_0);
    float g27_0 = (g25_0+0.5);
    float g30_0 = clamp(g27_0,0.0,1.0);
    float4 g31_0 = lerp(color_a,color_b,g30_0);
    float g32_0 = (g31_0).w;
    return float4((g31_0).rgb,g32_0);
}
