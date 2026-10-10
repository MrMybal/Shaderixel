//@name Graph Woven Pattern
//@category Graph Layers
//@graph {"name":"Graph Woven Pattern","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":40,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.12,0.32,0.5,1],"name":"warp","text":"Warp Color","min":0,"max":1,"index":1},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0.65,0.3,0.12,1],"name":"weft","text":"Weft Color","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[8,0,0,0],"name":"spacing","text":"Thread Period (px)","min":2,"max":128,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[0.25,0,0,0],"name":"shading","text":"Thread Shading","min":0,"max":1,"index":1},{"id":5,"op":"pixel","pos":[40,560],"in":[],"value":[0,0,0,0]},{"id":6,"op":"const","pos":[40,690],"in":[],"value":[0.5,0,0,0]},{"id":7,"op":"add","pos":[280,40],"in":[[5,0],[6,0]],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,820],"in":[],"value":[0.0001,0,0,0]},{"id":9,"op":"max","pos":[280,170],"in":[[3,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"div","pos":[520,40],"in":[[7,0],[9,0]],"value":[0,0,0,0]},{"id":11,"op":"floor","pos":[760,40],"in":[[10,0]],"value":[0,0,0,0]},{"id":12,"op":"mask","pos":[1000,40],"in":[[11,0]],"value":[0,0,0,0],"index":1},{"id":13,"op":"mask","pos":[1000,170],"in":[[11,0]],"value":[0,0,0,0],"index":2},{"id":14,"op":"add","pos":[1240,40],"in":[[12,0],[13,0]],"value":[0,0,0,0]},{"id":15,"op":"const","pos":[40,950],"in":[],"value":[0.5,0,0,0]},{"id":16,"op":"mul","pos":[1480,40],"in":[[14,0],[15,0]],"value":[0,0,0,0]},{"id":17,"op":"frac","pos":[1720,40],"in":[[16,0]],"value":[0,0,0,0]},{"id":18,"op":"const","pos":[40,1080],"in":[],"value":[2,0,0,0]},{"id":19,"op":"mul","pos":[1960,40],"in":[[17,0],[18,0]],"value":[0,0,0,0]},{"id":20,"op":"lerp","pos":[2200,40],"in":[[1,0],[2,0],[19,0]],"value":[0,0,0,0]},{"id":21,"op":"mask","pos":[760,170],"in":[[10,0]],"value":[0,0,0,0],"index":1},{"id":22,"op":"mask","pos":[760,300],"in":[[10,0]],"value":[0,0,0,0],"index":2},{"id":23,"op":"lerp","pos":[2200,170],"in":[[21,0],[22,0],[19,0]],"value":[0,0,0,0]},{"id":24,"op":"const","pos":[40,1210],"in":[],"value":[6.283185307179586,0,0,0]},{"id":25,"op":"mul","pos":[2440,40],"in":[[23,0],[24,0]],"value":[0,0,0,0]},{"id":26,"op":"cos","pos":[2680,40],"in":[[25,0]],"value":[0,0,0,0]},{"id":27,"op":"const","pos":[40,1340],"in":[],"value":[0.5,0,0,0]},{"id":28,"op":"mul","pos":[2920,40],"in":[[26,0],[27,0]],"value":[0,0,0,0]},{"id":29,"op":"const","pos":[40,1470],"in":[],"value":[0.5,0,0,0]},{"id":30,"op":"add","pos":[3160,40],"in":[[28,0],[29,0]],"value":[0,0,0,0]},{"id":31,"op":"const","pos":[40,1600],"in":[],"value":[1,0,0,0]},{"id":32,"op":"mul","pos":[3400,40],"in":[[30,0],[4,0]],"value":[0,0,0,0]},{"id":33,"op":"sub","pos":[3640,40],"in":[[31,0],[32,0]],"value":[0,0,0,0]},{"id":34,"op":"mask","pos":[2440,170],"in":[[20,0]],"value":[0,0,0,0],"index":7},{"id":35,"op":"mul","pos":[3880,40],"in":[[34,0],[33,0]],"value":[0,0,0,0]},{"id":36,"op":"mask","pos":[2440,300],"in":[[20,0]],"value":[0,0,0,0],"index":8},{"id":37,"op":"const","pos":[40,1730],"in":[],"value":[0,0,0,0]},{"id":38,"op":"const","pos":[40,1860],"in":[],"value":[1,0,0,0]},{"id":39,"op":"output","pos":[4120,40],"in":[[20,0],[35,0],[36,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param warp color 0.12 0.32 0.5 1.0 "Warp Color"
//@param weft color 0.65 0.3 0.12 1.0 "Weft Color"
//@param spacing float 8.0 2.0 128.0 "Thread Period (px)"
//@param shading float 0.25 0.0 1.0 "Thread Shading"

// Alternating woven tiles with cosine thread shading, created entirely with native nodes.
float4 Effect(EffectContext ctx)
{
    float2 g5_0 = float2(ctx.px);
    float2 g7_0 = (g5_0+(float2)(0.5));
    float g9_0 = max(spacing,0.0001);
    float2 g10_0 = (g7_0/(float2)(g9_0));
    float2 g11_0 = floor(g10_0);
    float g12_0 = (float4(g11_0,0.0,1.0)).x;
    float g13_0 = (float4(g11_0,0.0,1.0)).y;
    float g14_0 = (g12_0+g13_0);
    float g16_0 = (g14_0*0.5);
    float g17_0 = frac(g16_0);
    float g19_0 = (g17_0*2.0);
    float4 g20_0 = lerp(warp,weft,g19_0);
    float g21_0 = (float4(g10_0,0.0,1.0)).x;
    float g22_0 = (float4(g10_0,0.0,1.0)).y;
    float g23_0 = lerp(g21_0,g22_0,g19_0);
    float g25_0 = (g23_0*6.2831853);
    float g26_0 = cos(g25_0);
    float g28_0 = (g26_0*0.5);
    float g30_0 = (g28_0+0.5);
    float g32_0 = (g30_0*shading);
    float g33_0 = (1.0-g32_0);
    float3 g34_0 = (g20_0).xyz;
    float3 g35_0 = (g34_0*(float3)(g33_0));
    float g36_0 = (g20_0).w;
    return float4((float4(g35_0,(g20_0).a)).rgb,g36_0);
}
