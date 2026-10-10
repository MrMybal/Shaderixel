//@name Graph RGB Channel Shift
//@category Graph Effects
//@graph {"name":"Graph RGB Channel Shift","category":"Graph Effects","inputs":1,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"input","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"param2","pos":[40,170],"in":[],"value":[2,0,0,0],"name":"offset","text":"Channel Offset (px)","min":-64,"max":64,"index":1},{"id":3,"op":"param","pos":[40,300],"in":[],"value":[1,0,0,0],"name":"amount","text":"Strength","min":0,"max":1,"index":1},{"id":4,"op":"uv","pos":[40,430],"in":[],"value":[0,0,0,0]},{"id":5,"op":"size","pos":[40,560],"in":[],"value":[0,0,0,0]},{"id":6,"op":"div","pos":[280,40],"in":[[2,0],[5,0]],"value":[0,0,0,0]},{"id":7,"op":"add","pos":[520,40],"in":[[4,0],[6,0]],"value":[0,0,0,0]},{"id":8,"op":"sample","pos":[760,40],"in":[[7,0]],"value":[0,0,0,0]},{"id":9,"op":"sub","pos":[520,170],"in":[[4,0],[6,0]],"value":[0,0,0,0]},{"id":10,"op":"sample","pos":[760,170],"in":[[9,0]],"value":[0,0,0,0]},{"id":11,"op":"make3","pos":[1000,40],"in":[[8,2],[1,3],[10,4]],"value":[0,0,0,0]},{"id":12,"op":"lerp","pos":[1240,40],"in":[[1,1],[11,0],[3,0]],"value":[0,0,0,0]},{"id":13,"op":"const","pos":[40,690],"in":[],"value":[0,0,0,0]},{"id":14,"op":"const","pos":[40,820],"in":[],"value":[1,0,0,0]},{"id":15,"op":"output","pos":[1480,40],"in":[[1,0],[12,0],[1,5]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param offset float2 2.0 0.0 -64.0 64.0 "Channel Offset (px)"
//@param amount float 1.0 0.0 1.0 "Strength"

// Opposing red/blue sampling offsets with central green and alpha; boundaries clamp.
float4 Effect(EffectContext ctx)
{
    float4 g1_0 = Input(0,ctx.px);
    float3 g1_1 = (Input(0,ctx.px)).rgb;
    float g1_2 = (Input(0,ctx.px)).r;
    float g1_3 = (Input(0,ctx.px)).g;
    float g1_4 = (Input(0,ctx.px)).b;
    float g1_5 = (Input(0,ctx.px)).a;
    float2 g5_0 = ctx.size;
    float2 g6_0 = (offset/g5_0);
    float2 g7_0 = (ctx.uv+g6_0);
    float4 g8_0 = InputUV(0,g7_0);
    float3 g8_1 = (InputUV(0,g7_0)).rgb;
    float g8_2 = (InputUV(0,g7_0)).r;
    float g8_3 = (InputUV(0,g7_0)).g;
    float g8_4 = (InputUV(0,g7_0)).b;
    float g8_5 = (InputUV(0,g7_0)).a;
    float2 g9_0 = (ctx.uv-g6_0);
    float4 g10_0 = InputUV(0,g9_0);
    float3 g10_1 = (InputUV(0,g9_0)).rgb;
    float g10_2 = (InputUV(0,g9_0)).r;
    float g10_3 = (InputUV(0,g9_0)).g;
    float g10_4 = (InputUV(0,g9_0)).b;
    float g10_5 = (InputUV(0,g9_0)).a;
    float3 g11_0 = float3(g8_2,g1_3,g10_4);
    float3 g12_0 = lerp(g1_1,g11_0,amount);
    return float4((float4(g12_0,(g1_0).a)).rgb,g1_5);
}
