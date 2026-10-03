//@name Bokeh Rings
//@category Dessin
//@graph {"name":"Bokeh Rings","category":"Dessin","inputs":0,"premultiplied":false,"nextId":15,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.100000001,"max":8.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.219999999,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00100000005,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0250000004,0.0,0.0,0.0],"name":"RingWidth","text":"Ring Width","index":1,"min":0.00100000005,"max":0.5},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":0.100000001,"max":16.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":64.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"param2","pos":[40,880],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":10,"op":"param2","pos":[40,990],"in":[],"value":[0.200000003,0.0700000003,0.0,0.0],"name":"Offset","text":"Offset","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[1.0,0.449999988,0.119999997,1.0],"name":"ColorA","text":"Color A","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[0.100000001,0.400000006,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":13,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0]],"value":[0,0,0,0],"name":"Bokeh Rings","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float ring1=pow(saturate(1.0-abs(length(p)-Radius)/max(RingWidth,0.0001)),Softness); float ring2=pow(saturate(1.0-abs(length(p-Offset.xy)-Radius*0.55)/max(RingWidth*0.7,0.0001)),Softness); float ring3=pow(saturate(1.0-abs(length(p+Offset.xy*1.4)-Radius*0.35)/max(RingWidth*0.55,0.0001)),Softness); float3 rgb=ColorA.rgb*ring1+ColorB.rgb*(ring2+ring3); float a=saturate((ring1+ring2+ring3)*Opacity); return float4(rgb*Intensity,a);","pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["Radius",1],["RingWidth",1],["Softness",1],["Intensity",1],["Opacity",1],["Center",3],["Offset",3],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":14,"op":"output","pos":[780,40],"in":[[13,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param Radius float 0.22 0.001 2 "Radius"
//@param RingWidth float 0.025 0.001 0.5 "Ring Width"
//@param Softness float 2 0.1 16 "Softness"
//@param Intensity float 2 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Offset float2 0.2 0.07 0 1 "Offset"
//@param ColorA color 1 0.45 0.12 1 "Color A"
//@param ColorB color 0.1 0.4 1 1 "Color B"

// Imported preset (Draw / Light & Space): Procedural bokeh ring cluster for flare and depth-of-field overlays.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float RingWidth, float Softness, float Intensity, float Opacity, float3 Center, float3 Offset, float3 ColorA, float3 ColorB)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float ring1=pow(saturate(1.0-abs(length(p)-Radius)/max(RingWidth,0.0001)),Softness); float ring2=pow(saturate(1.0-abs(length(p-Offset.xy)-Radius*0.55)/max(RingWidth*0.7,0.0001)),Softness); float ring3=pow(saturate(1.0-abs(length(p+Offset.xy*1.4)-Radius*0.35)/max(RingWidth*0.55,0.0001)),Softness); float3 rgb=ColorA.rgb*ring1+ColorB.rgb*(ring2+ring3); float a=saturate((ring1+ring2+ring3)*Opacity); return float4(rgb*Intensity,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, RingWidth, Softness, Intensity, Opacity, float3(Center, 0.0), float3(Offset, 0.0), ColorA.rgb, ColorB.rgb);
}
