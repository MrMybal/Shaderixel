//@name Graph Glow Disc
//@category Graph Layers
//@graph {"name":"Graph Glow Disc","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":27,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.05,0.4,1,1],"name":"color","text":"Glow Color","min":0,"max":1,"index":1},{"id":2,"op":"param2","pos":[40,170],"in":[],"value":[0.5,0.5,0,0],"name":"center","text":"Center (UV)","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[160,0,0,0],"name":"radius","text":"Glow Radius (px)","min":1,"max":2048,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[2,0,0,0],"name":"falloff","text":"Falloff","min":0.1,"max":8,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[0.8,0,0,0],"name":"opacity","text":"Glow Opacity","min":0,"max":1,"index":1},{"id":6,"op":"pixel","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":7,"op":"const","pos":[40,820],"in":[],"value":[0.5,0,0,0]},{"id":8,"op":"add","pos":[280,40],"in":[[6,0],[7,0]],"value":[0,0,0,0]},{"id":9,"op":"size","pos":[40,950],"in":[],"value":[0,0,0,0]},{"id":10,"op":"mul","pos":[280,170],"in":[[9,0],[2,0]],"value":[0,0,0,0]},{"id":11,"op":"sub","pos":[520,40],"in":[[8,0],[10,0]],"value":[0,0,0,0]},{"id":12,"op":"length","pos":[760,40],"in":[[11,0]],"value":[0,0,0,0]},{"id":13,"op":"const","pos":[40,1080],"in":[],"value":[0.0001,0,0,0]},{"id":14,"op":"max","pos":[280,300],"in":[[3,0],[13,0]],"value":[0,0,0,0]},{"id":15,"op":"div","pos":[1000,40],"in":[[12,0],[14,0]],"value":[0,0,0,0]},{"id":16,"op":"oneminus","pos":[1240,40],"in":[[15,0]],"value":[0,0,0,0]},{"id":17,"op":"const","pos":[40,1210],"in":[],"value":[0,0,0,0]},{"id":18,"op":"const","pos":[40,1340],"in":[],"value":[1,0,0,0]},{"id":19,"op":"clamp","pos":[1480,40],"in":[[16,0],[17,0],[18,0]],"value":[0,0,0,0]},{"id":20,"op":"pow","pos":[1720,40],"in":[[19,0],[4,0]],"value":[0,0,0,0]},{"id":21,"op":"mask","pos":[280,430],"in":[[1,0]],"value":[0,0,0,0],"index":8},{"id":22,"op":"mul","pos":[520,170],"in":[[21,0],[5,0]],"value":[0,0,0,0]},{"id":23,"op":"mul","pos":[1960,40],"in":[[20,0],[22,0]],"value":[0,0,0,0]},{"id":24,"op":"const","pos":[40,1470],"in":[],"value":[0,0,0,0]},{"id":25,"op":"const","pos":[40,1600],"in":[],"value":[1,0,0,0]},{"id":26,"op":"output","pos":[2200,40],"in":[[1,0],[0,0],[23,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param color color 0.05 0.4 1.0 1.0 "Glow Color"
//@param center float2 0.5 0.5 0.0 1.0 "Center (UV)"
//@param radius float 160.0 1.0 2048.0 "Glow Radius (px)"
//@param falloff float 2.0 0.1 8.0 "Falloff"
//@param opacity float 0.8 0.0 1.0 "Glow Opacity"

// Soft radial colored disc on transparency, with center, pixel radius and falloff.
float4 Effect(EffectContext ctx)
{
    float2 g6_0 = float2(ctx.px);
    float2 g8_0 = (g6_0+(float2)(0.5));
    float2 g9_0 = ctx.size;
    float2 g10_0 = (g9_0*center);
    float2 g11_0 = (g8_0-g10_0);
    float g12_0 = length(g11_0);
    float g14_0 = max(radius,0.0001);
    float g15_0 = (g12_0/g14_0);
    float g16_0 = (1.0-g15_0);
    float g19_0 = clamp(g16_0,0.0,1.0);
    float g20_0 = pow(max(g19_0,0.0),falloff);
    float g21_0 = (color).w;
    float g22_0 = (g21_0*opacity);
    float g23_0 = (g20_0*g22_0);
    return float4((color).rgb,g23_0);
}
