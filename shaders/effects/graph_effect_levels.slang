//@name Graph Perceptual Levels
//@category Graph Effects
//@graph {"name":"Graph Perceptual Levels","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":21,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"param","pos":[40,170],"in":[],"value":[0,0,0,0],"name":"black","text":"Black Point","min":0,"max":0.99,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[1,0,0,0],"name":"white","text":"White Point","min":0.01,"max":1,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[1,0,0,0],"name":"gamma","text":"Tone Curve","min":0.1,"max":4,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[1,0,0,0],"name":"amount","text":"Strength","min":0,"max":1,"index":1},{"id":6,"op":"tosrgb","pos":[280,40],"in":[[1,1]],"value":[0,0,0,0]},{"id":7,"op":"sub","pos":[280,170],"in":[[3,0],[2,0]],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,690],"in":[],"value":[0.0001,0,0,0]},{"id":9,"op":"max","pos":[520,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"sub","pos":[520,170],"in":[[6,0],[2,0]],"value":[0,0,0,0]},{"id":11,"op":"div","pos":[760,40],"in":[[10,0],[9,0]],"value":[0,0,0,0]},{"id":12,"op":"const","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":13,"op":"const","pos":[40,950],"in":[],"value":[1,0,0,0]},{"id":14,"op":"clamp","pos":[1000,40],"in":[[11,0],[12,0],[13,0]],"value":[0,0,0,0]},{"id":15,"op":"pow","pos":[1240,40],"in":[[14,0],[4,0]],"value":[0,0,0,0]},{"id":16,"op":"tolinear","pos":[1480,40],"in":[[15,0]],"value":[0,0,0,0]},{"id":17,"op":"lerp","pos":[1720,40],"in":[[1,1],[16,0],[5,0]],"value":[0,0,0,0]},{"id":18,"op":"const","pos":[40,1080],"in":[],"value":[0,0,0,0]},{"id":19,"op":"const","pos":[40,1210],"in":[],"value":[1,0,0,0]},{"id":20,"op":"output","pos":[1960,40],"in":[[1,0],[17,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param black float 0.0 0.0 0.99 "Black Point"
//@param white float 1.0 0.01 1.0 "White Point"
//@param gamma float 1.0 0.1 4.0 "Tone Curve"
//@param amount float 1.0 0.0 1.0 "Strength"

// Display-space levels and gamma with source alpha preservation and strength.
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float3 g6_0 = LinearToSrgb(g1_1);
    float g7_0 = (white-black);
    float g9_0 = max(g7_0,0.0001);
    float3 g10_0 = (g6_0-(float3)(black));
    float3 g11_0 = (g10_0/(float3)(g9_0));
    float3 g14_0 = clamp(g11_0,(float3)(0.0),(float3)(1.0));
    float3 g15_0 = pow(max(g14_0,0.0),(float3)(gamma));
    float3 g16_0 = SrgbToLinear(g15_0);
    float3 g17_0 = lerp(g1_1,g16_0,amount);
    return float4((float4(g17_0,(g1_0).a)).rgb,g1_5);
}
