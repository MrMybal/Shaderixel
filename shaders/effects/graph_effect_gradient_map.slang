//@name Graph Gradient Map
//@category Graph Effects
//@graph {"name":"Graph Gradient Map","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":19,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0.02,0.04,0.15,1],"name":"dark","text":"Dark Color (RGB)","min":0,"max":1,"index":1},{"id":3,"op":"paramcolor","pos":[40,300],"in":[],"value":[1,0.65,0.2,1],"name":"light","text":"Light Color (RGB)","min":0,"max":1,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[1,0,0,0],"name":"gamma","text":"Tone Curve","min":0.1,"max":4,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[1,0,0,0],"name":"amount","text":"Strength","min":0,"max":1,"index":1},{"id":6,"op":"tosrgb","pos":[280,40],"in":[[1,1]],"value":[0,0,0,0]},{"id":7,"op":"luma","pos":[520,40],"in":[[6,0]],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":9,"op":"const","pos":[40,820],"in":[],"value":[1,0,0,0]},{"id":10,"op":"clamp","pos":[760,40],"in":[[7,0],[8,0],[9,0]],"value":[0,0,0,0]},{"id":11,"op":"pow","pos":[1000,40],"in":[[10,0],[4,0]],"value":[0,0,0,0]},{"id":12,"op":"mask","pos":[280,170],"in":[[2,0]],"value":[0,0,0,0],"index":7},{"id":13,"op":"mask","pos":[280,300],"in":[[3,0]],"value":[0,0,0,0],"index":7},{"id":14,"op":"lerp","pos":[1240,40],"in":[[12,0],[13,0],[11,0]],"value":[0,0,0,0]},{"id":15,"op":"lerp","pos":[1480,40],"in":[[1,1],[14,0],[5,0]],"value":[0,0,0,0]},{"id":16,"op":"const","pos":[40,950],"in":[],"value":[0,0,0,0]},{"id":17,"op":"const","pos":[40,1080],"in":[],"value":[1,0,0,0]},{"id":18,"op":"output","pos":[1720,40],"in":[[1,0],[15,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param dark color 0.02 0.04 0.15 1.0 "Dark Color (RGB)"
//@param light color 1.0 0.65 0.2 1.0 "Light Color (RGB)"
//@param gamma float 1.0 0.1 4.0 "Tone Curve"
//@param amount float 1.0 0.0 1.0 "Strength"

// Two-color perceptual luminance mapping with curve and strength, preserving source alpha.
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float3 g6_0 = LinearToSrgb(g1_1);
    float g7_0 = Luma(g6_0);
    float g10_0 = clamp(g7_0,0.0,1.0);
    float g11_0 = pow(max(g10_0,0.0),gamma);
    float3 g12_0 = (dark).xyz;
    float3 g13_0 = (light).xyz;
    float3 g14_0 = lerp(g12_0,g13_0,g11_0);
    float3 g15_0 = lerp(g1_1,g14_0,amount);
    return float4((float4(g15_0,(g1_0).a)).rgb,g1_5);
}
