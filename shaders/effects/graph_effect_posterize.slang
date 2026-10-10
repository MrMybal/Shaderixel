//@name Graph Dithered Posterize
//@category Graph Effects
//@graph {"name":"Graph Dithered Posterize","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":31,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"param","pos":[40,170],"in":[],"value":[6,0,0,0],"name":"levels","text":"Levels per Channel","min":2,"max":64,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[0.25,0,0,0],"name":"dither","text":"Dither Strength","min":0,"max":1,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[1,0,0,0],"name":"amount","text":"Strength","min":0,"max":1,"index":1},{"id":5,"op":"pixel","pos":[40,560],"in":[],"value":[0,0,0,0]},{"id":6,"op":"const","pos":[40,690],"in":[],"value":[1,0,0,0]},{"id":7,"op":"noise","pos":[280,40],"in":[[5,0],[6,0]],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,820],"in":[],"value":[0.5,0,0,0]},{"id":9,"op":"sub","pos":[520,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"mul","pos":[760,40],"in":[[9,0],[3,0]],"value":[0,0,0,0]},{"id":11,"op":"floor","pos":[280,170],"in":[[2,0]],"value":[0,0,0,0]},{"id":12,"op":"const","pos":[40,950],"in":[],"value":[1,0,0,0]},{"id":13,"op":"sub","pos":[520,170],"in":[[11,0],[12,0]],"value":[0,0,0,0]},{"id":14,"op":"const","pos":[40,1080],"in":[],"value":[1,0,0,0]},{"id":15,"op":"max","pos":[760,170],"in":[[13,0],[14,0]],"value":[0,0,0,0]},{"id":16,"op":"tosrgb","pos":[280,300],"in":[[1,1]],"value":[0,0,0,0]},{"id":17,"op":"mul","pos":[1000,40],"in":[[16,0],[15,0]],"value":[0,0,0,0]},{"id":18,"op":"const","pos":[40,1210],"in":[],"value":[0.5,0,0,0]},{"id":19,"op":"add","pos":[1000,170],"in":[[10,0],[18,0]],"value":[0,0,0,0]},{"id":20,"op":"add","pos":[1240,40],"in":[[17,0],[19,0]],"value":[0,0,0,0]},{"id":21,"op":"floor","pos":[1480,40],"in":[[20,0]],"value":[0,0,0,0]},{"id":22,"op":"div","pos":[1720,40],"in":[[21,0],[15,0]],"value":[0,0,0,0]},{"id":23,"op":"const","pos":[40,1340],"in":[],"value":[0,0,0,0]},{"id":24,"op":"const","pos":[40,1470],"in":[],"value":[1,0,0,0]},{"id":25,"op":"clamp","pos":[1960,40],"in":[[22,0],[23,0],[24,0]],"value":[0,0,0,0]},{"id":26,"op":"tolinear","pos":[2200,40],"in":[[25,0]],"value":[0,0,0,0]},{"id":27,"op":"lerp","pos":[2440,40],"in":[[1,1],[26,0],[4,0]],"value":[0,0,0,0]},{"id":28,"op":"const","pos":[40,1600],"in":[],"value":[0,0,0,0]},{"id":29,"op":"const","pos":[40,1730],"in":[],"value":[1,0,0,0]},{"id":30,"op":"output","pos":[2680,40],"in":[[1,0],[27,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param levels float 6.0 2.0 64.0 "Levels per Channel"
//@param dither float 0.25 0.0 1.0 "Dither Strength"
//@param amount float 1.0 0.0 1.0 "Strength"

// Perceptual RGB quantization with deterministic pixel dithering and original alpha.
float GraphNoise(float2 p)
{
    float2 i = floor(p), t = frac(p);
    t = t * t * (3.0 - 2.0 * t);
    return lerp(lerp(Hash12(i),Hash12(i+float2(1,0)),t.x),lerp(Hash12(i+float2(0,1)),Hash12(i+float2(1,1)),t.x),t.y);
}
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float2 g5_0 = float2(ctx.px);
    float g7_0 = GraphNoise(g5_0*1.0);
    float g9_0 = (g7_0-0.5);
    float g10_0 = (g9_0*dither);
    float g11_0 = floor(levels);
    float g13_0 = (g11_0-1.0);
    float g15_0 = max(g13_0,1.0);
    float3 g16_0 = LinearToSrgb(g1_1);
    float3 g17_0 = (g16_0*(float3)(g15_0));
    float g19_0 = (g10_0+0.5);
    float3 g20_0 = (g17_0+(float3)(g19_0));
    float3 g21_0 = floor(g20_0);
    float3 g22_0 = (g21_0/(float3)(g15_0));
    float3 g25_0 = clamp(g22_0,(float3)(0.0),(float3)(1.0));
    float3 g26_0 = SrgbToLinear(g25_0);
    float3 g27_0 = lerp(g1_1,g26_0,amount);
    return float4((float4(g27_0,(g1_0).a)).rgb,g1_5);
}
