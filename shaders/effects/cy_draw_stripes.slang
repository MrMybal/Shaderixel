//@name Stripes
//@category Dessin
//@graph {"name":"Stripes","category":"Dessin","inputs":0,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[16.0,0.0,0.0,0.0],"name":"Frequency","text":"Frequency","index":1,"min":1.0,"max":512.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Angle","text":"Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Phase","text":"Phase","index":1,"min":-10.0,"max":10.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Duty","text":"Duty","index":1,"min":0.00999999978,"max":0.99000001},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.00999999978,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.25},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityA","text":"Opacity A","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityB","text":"Opacity B","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Stripes","text":"    float2 p=UV-0.5; float c=cos(Angle); float s=sin(Angle); float u=dot(p,float2(c,s))*Frequency+Phase; float v=frac(u); float edge=max(Softness,0.0001); float m=1.0-smoothstep(Duty-edge,Duty+edge,v); return float4(lerp(ColorB.rgb,ColorA.rgb,m),lerp(OpacityB,OpacityA,m));","pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Angle",1],["Phase",1],["Duty",1],["Softness",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Frequency float 16 1 512 "Frequency"
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Phase float 0 -10 10 "Phase"
//@param Duty float 0.5 0.01 0.99 "Duty"
//@param Softness float 0.01 0.0001 0.25 "Softness"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 1 1 1 1 "Color A"
//@param ColorB color 0 0 0 1 "Color B"

// Imported preset (Draw / Shape & Gradient): Rotatable antialiased stripe generator.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Angle, float Phase, float Duty, float Softness, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p=UV-0.5; float c=cos(Angle); float s=sin(Angle); float u=dot(p,float2(c,s))*Frequency+Phase; float v=frac(u); float edge=max(Softness,0.0001); float m=1.0-smoothstep(Duty-edge,Duty+edge,v); return float4(lerp(ColorB.rgb,ColorA.rgb,m),lerp(OpacityB,OpacityA,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Angle, Phase, Duty, Softness, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
