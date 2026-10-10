//@name Graph Color Wash
//@category Graph Effects
//@graph {"name":"Graph Color Wash","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":19,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0.15,0.4,0.8,1],"name":"tint","text":"Tint Color (RGB)","min":0,"max":1,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[0.5,0,0,0],"name":"amount","text":"Strength","min":0,"max":1,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[1,0,0,0],"name":"preserve","text":"Luminance Preservation","min":0,"max":1,"index":1},{"id":5,"op":"luma","pos":[280,40],"in":[[1,1]],"value":[0,0,0,0]},{"id":6,"op":"mask","pos":[280,170],"in":[[2,0]],"value":[0,0,0,0],"index":7},{"id":7,"op":"luma","pos":[520,40],"in":[[6,0]],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,560],"in":[],"value":[0.0001,0,0,0]},{"id":9,"op":"max","pos":[760,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"mask","pos":[280,300],"in":[[2,0]],"value":[0,0,0,0],"index":7},{"id":11,"op":"div","pos":[1000,40],"in":[[5,0],[9,0]],"value":[0,0,0,0]},{"id":12,"op":"mul","pos":[1240,40],"in":[[10,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"mask","pos":[280,430],"in":[[2,0]],"value":[0,0,0,0],"index":7},{"id":14,"op":"lerp","pos":[1480,40],"in":[[13,0],[12,0],[4,0]],"value":[0,0,0,0]},{"id":15,"op":"lerp","pos":[1720,40],"in":[[1,1],[14,0],[3,0]],"value":[0,0,0,0]},{"id":16,"op":"const","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":17,"op":"const","pos":[40,820],"in":[],"value":[1,0,0,0]},{"id":18,"op":"output","pos":[1960,40],"in":[[1,0],[15,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param tint color 0.15 0.4 0.8 1.0 "Tint Color (RGB)"
//@param amount float 0.5 0.0 1.0 "Strength"
//@param preserve float 1.0 0.0 1.0 "Luminance Preservation"

// Linear color wash with optional luminance preservation and original alpha.
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float g5_0 = Luma(g1_1);
    float3 g6_0 = (tint).xyz;
    float g7_0 = Luma(g6_0);
    float g9_0 = max(g7_0,0.0001);
    float3 g10_0 = (tint).xyz;
    float g11_0 = (g5_0/g9_0);
    float3 g12_0 = (g10_0*(float3)(g11_0));
    float3 g13_0 = (tint).xyz;
    float3 g14_0 = lerp(g13_0,g12_0,preserve);
    float3 g15_0 = lerp(g1_1,g14_0,amount);
    return float4((float4(g15_0,(g1_0).a)).rgb,g1_5);
}
