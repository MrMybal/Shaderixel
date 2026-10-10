//@name Graph Concentric Rings
//@category Graph Layers
//@graph {"name":"Graph Concentric Rings","category":"Graph Layers","inputs":0,"premultiplied":false,"nextId":43,"nodes":[{"id":1,"op":"paramcolor","pos":[40,40],"in":[],"value":[0.02,0.5,0.8,1],"name":"line_color","text":"Ring Color","min":0,"max":1,"index":1},{"id":2,"op":"paramcolor","pos":[40,170],"in":[],"value":[0,0,0,0],"name":"background","text":"Background Color","min":0,"max":1,"index":1},{"id":3,"op":"param2","pos":[40,300],"in":[],"value":[0.5,0.5,0,0],"name":"center","text":"Center (UV)","min":0,"max":1,"index":1},{"id":4,"op":"param","pos":[40,430],"in":[],"value":[32,0,0,0],"name":"spacing","text":"Ring Spacing (px)","min":2,"max":512,"index":1},{"id":5,"op":"param","pos":[40,560],"in":[],"value":[3,0,0,0],"name":"width","text":"Ring Width (px)","min":0,"max":64,"index":1},{"id":6,"op":"param","pos":[40,690],"in":[],"value":[1,0,0,0],"name":"softness","text":"Edge Softness (px)","min":0.1,"max":8,"index":1},{"id":7,"op":"pixel","pos":[40,820],"in":[],"value":[0,0,0,0]},{"id":8,"op":"const","pos":[40,950],"in":[],"value":[0.5,0,0,0]},{"id":9,"op":"add","pos":[280,40],"in":[[7,0],[8,0]],"value":[0,0,0,0]},{"id":10,"op":"size","pos":[40,1080],"in":[],"value":[0,0,0,0]},{"id":11,"op":"mul","pos":[280,170],"in":[[10,0],[3,0]],"value":[0,0,0,0]},{"id":12,"op":"sub","pos":[520,40],"in":[[9,0],[11,0]],"value":[0,0,0,0]},{"id":13,"op":"length","pos":[760,40],"in":[[12,0]],"value":[0,0,0,0]},{"id":14,"op":"const","pos":[40,1210],"in":[],"value":[0.0001,0,0,0]},{"id":15,"op":"max","pos":[280,300],"in":[[4,0],[14,0]],"value":[0,0,0,0]},{"id":16,"op":"div","pos":[1000,40],"in":[[13,0],[15,0]],"value":[0,0,0,0]},{"id":17,"op":"const","pos":[40,1340],"in":[],"value":[0.5,0,0,0]},{"id":18,"op":"add","pos":[1240,40],"in":[[16,0],[17,0]],"value":[0,0,0,0]},{"id":19,"op":"frac","pos":[1480,40],"in":[[18,0]],"value":[0,0,0,0]},{"id":20,"op":"const","pos":[40,1470],"in":[],"value":[0.5,0,0,0]},{"id":21,"op":"sub","pos":[1720,40],"in":[[19,0],[20,0]],"value":[0,0,0,0]},{"id":22,"op":"abs","pos":[1960,40],"in":[[21,0]],"value":[0,0,0,0]},{"id":23,"op":"mul","pos":[2200,40],"in":[[22,0],[4,0]],"value":[0,0,0,0]},{"id":24,"op":"const","pos":[40,1600],"in":[],"value":[0.5,0,0,0]},{"id":25,"op":"mul","pos":[280,430],"in":[[5,0],[24,0]],"value":[0,0,0,0]},{"id":26,"op":"const","pos":[40,1730],"in":[],"value":[0.5,0,0,0]},{"id":27,"op":"mul","pos":[280,560],"in":[[4,0],[26,0]],"value":[0,0,0,0]},{"id":28,"op":"min","pos":[520,170],"in":[[25,0],[27,0]],"value":[0,0,0,0]},{"id":29,"op":"sub","pos":[760,170],"in":[[28,0],[6,0]],"value":[0,0,0,0]},{"id":30,"op":"add","pos":[760,300],"in":[[28,0],[6,0]],"value":[0,0,0,0]},{"id":31,"op":"smoothstep","pos":[2440,40],"in":[[29,0],[30,0],[23,0]],"value":[0,0,0,0]},{"id":32,"op":"oneminus","pos":[2680,40],"in":[[31,0]],"value":[0,0,0,0]},{"id":33,"op":"const","pos":[40,1860],"in":[],"value":[0,0,0,0]},{"id":34,"op":"const","pos":[40,1990],"in":[],"value":[1,0,0,0]},{"id":35,"op":"clamp","pos":[280,690],"in":[[5,0],[33,0],[34,0]],"value":[0,0,0,0]},{"id":36,"op":"mul","pos":[2920,40],"in":[[32,0],[35,0]],"value":[0,0,0,0]},{"id":37,"op":"saturate","pos":[3160,40],"in":[[36,0]],"value":[0,0,0,0]},{"id":38,"op":"lerp","pos":[3400,40],"in":[[2,0],[1,0],[37,0]],"value":[0,0,0,0]},{"id":39,"op":"mask","pos":[3640,40],"in":[[38,0]],"value":[0,0,0,0],"index":8},{"id":40,"op":"const","pos":[40,2120],"in":[],"value":[0,0,0,0]},{"id":41,"op":"const","pos":[40,2250],"in":[],"value":[1,0,0,0]},{"id":42,"op":"output","pos":[3880,40],"in":[[38,0],[0,0],[39,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param line_color color 0.02 0.5 0.8 1.0 "Ring Color"
//@param background color 0.0 0.0 0.0 0.0 "Background Color"
//@param center float2 0.5 0.5 0.0 1.0 "Center (UV)"
//@param spacing float 32.0 2.0 512.0 "Ring Spacing (px)"
//@param width float 3.0 0.0 64.0 "Ring Width (px)"
//@param softness float 1.0 0.1 8.0 "Edge Softness (px)"

// Concentric pixel-scale rings with transparent background and native-node distance controls.
float4 Effect(EffectContext ctx)
{
    float2 g7_0 = float2(ctx.px);
    float2 g9_0 = (g7_0+(float2)(0.5));
    float2 g10_0 = ctx.size;
    float2 g11_0 = (g10_0*center);
    float2 g12_0 = (g9_0-g11_0);
    float g13_0 = length(g12_0);
    float g15_0 = max(spacing,0.0001);
    float g16_0 = (g13_0/g15_0);
    float g18_0 = (g16_0+0.5);
    float g19_0 = frac(g18_0);
    float g21_0 = (g19_0-0.5);
    float g22_0 = abs(g21_0);
    float g23_0 = (g22_0*spacing);
    float g25_0 = (width*0.5);
    float g27_0 = (spacing*0.5);
    float g28_0 = min(g25_0,g27_0);
    float g29_0 = (g28_0-softness);
    float g30_0 = (g28_0+softness);
    float g31_0 = smoothstep(g29_0,g30_0,g23_0);
    float g32_0 = (1.0-g31_0);
    float g35_0 = clamp(width,0.0,1.0);
    float g36_0 = (g32_0*g35_0);
    float g37_0 = saturate(g36_0);
    float4 g38_0 = lerp(background,line_color,g37_0);
    float g39_0 = (g38_0).w;
    return float4((g38_0).rgb,g39_0);
}
