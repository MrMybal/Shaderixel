//@name Graph Dot Grid
//@category Graph Layers
//@graph {"name":"Graph Dot Grid","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":36,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.8,0.15,0.025,1],"name":"dot_color","text":"Dot Color","min":0,"max":1,"index":1},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0,0,0,0],"name":"background","text":"Background Color","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[24,0,0,0],"name":"spacing","text":"Cell Spacing (px)","min":2,"max":256,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[4,0,0,0],"name":"radius","text":"Dot Radius (px)","min":0,"max":64,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[1,0,0,0],"name":"softness","text":"Edge Softness (px)","min":0.1,"max":8,"index":1},{"id":6,"op":"param2","pos":[40,690],"in":[],"value":[0,0,0,0],"name":"offset","text":"Offset (px)","min":-256,"max":256,"index":1},{"id":7,"op":"pixel","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,950],"in":[],"value":[0.5,0,0,0]},{"id":9,"op":"add","pos":[280,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"sub","pos":[520,40],"in":[[9,0],[6,0]],"value":[0,0,0,0]},{"id":11,"op":"const","pos":[40,1080],"in":[],"value":[0.0001,0,0,0]},{"id":12,"op":"max","pos":[280,170],"in":[[3,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"div","pos":[760,40],"in":[[10,0],[12,0]],"value":[0,0,0,0]},{"id":14,"op":"frac","pos":[1000,40],"in":[[13,0]],"value":[0,0,0,0]},{"id":15,"op":"const","pos":[40,1210],"in":[],"value":[0.5,0,0,0]},{"id":16,"op":"sub","pos":[1240,40],"in":[[14,0],[15,0]],"value":[0,0,0,0]},{"id":17,"op":"mul","pos":[1480,40],"in":[[16,0],[3,0]],"value":[0,0,0,0]},{"id":18,"op":"const","pos":[40,1340],"in":[],"value":[0.5,0,0,0]},{"id":19,"op":"mul","pos":[280,300],"in":[[3,0],[18,0]],"value":[0,0,0,0]},{"id":20,"op":"min","pos":[520,170],"in":[[4,0],[19,0]],"value":[0,0,0,0]},{"id":21,"op":"sub","pos":[760,170],"in":[[20,0],[5,0]],"value":[0,0,0,0]},{"id":22,"op":"add","pos":[760,300],"in":[[20,0],[5,0]],"value":[0,0,0,0]},{"id":23,"op":"length","pos":[1720,40],"in":[[17,0]],"value":[0,0,0,0]},{"id":24,"op":"smoothstep","pos":[1960,40],"in":[[21,0],[22,0],[23,0]],"value":[0,0,0,0]},{"id":25,"op":"oneminus","pos":[2200,40],"in":[[24,0]],"value":[0,0,0,0]},{"id":26,"op":"const","pos":[40,1470],"in":[],"value":[0,0,0,0]},{"id":27,"op":"const","pos":[40,1600],"in":[],"value":[1,0,0,0]},{"id":28,"op":"clamp","pos":[280,430],"in":[[4,0],[26,0],[27,0]],"value":[0,0,0,0]},{"id":29,"op":"mul","pos":[2440,40],"in":[[25,0],[28,0]],"value":[0,0,0,0]},{"id":30,"op":"saturate","pos":[2680,40],"in":[[29,0]],"value":[0,0,0,0]},{"id":31,"op":"lerp","pos":[2920,40],"in":[[2,0],[1,0],[30,0]],"value":[0,0,0,0]},{"id":32,"op":"mask","pos":[3160,40],"in":[[31,0]],"value":[0,0,0,0],"index":8},{"id":33,"op":"const","pos":[40,1730],"in":[],"value":[0,0,0,0]},{"id":34,"op":"const","pos":[40,1860],"in":[],"value":[1,0,0,0]},{"id":35,"op":"output","pos":[3400,40],"in":[[31,0],[0,0],[32,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param dot_color color 0.8 0.15 0.025 1.0 "Dot Color"
//@param background color 0.0 0.0 0.0 0.0 "Background Color"
//@param spacing float 24.0 2.0 256.0 "Cell Spacing (px)"
//@param radius float 4.0 0.0 64.0 "Dot Radius (px)"
//@param softness float 1.0 0.1 8.0 "Edge Softness (px)"
//@param offset float2 0.0 0.0 -256.0 256.0 "Offset (px)"

// Repeating antialiased dots in document pixels with phase and transparent background.
float4 Effect(EffectContext ctx)
{
    float2 g7_0 = float2(ctx.px);
    float2 g9_0 = (g7_0+(float2)(0.5));
    float2 g10_0 = (g9_0-offset);
    float g12_0 = max(spacing,0.0001);
    float2 g13_0 = (g10_0/(float2)(g12_0));
    float2 g14_0 = frac(g13_0);
    float2 g16_0 = (g14_0-(float2)(0.5));
    float2 g17_0 = (g16_0*(float2)(spacing));
    float g19_0 = (spacing*0.5);
    float g20_0 = min(radius,g19_0);
    float g21_0 = (g20_0-softness);
    float g22_0 = (g20_0+softness);
    float g23_0 = length(g17_0);
    float g24_0 = smoothstep(g21_0,g22_0,g23_0);
    float g25_0 = (1.0-g24_0);
    float g28_0 = clamp(radius,0.0,1.0);
    float g29_0 = (g25_0*g28_0);
    float g30_0 = saturate(g29_0);
    float4 g31_0 = lerp(background,dot_color,g30_0);
    float g32_0 = (g31_0).w;
    return float4((g31_0).rgb,g32_0);
}
