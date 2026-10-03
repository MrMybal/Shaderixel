//@name Crosshatch
//@category Dessin
//@graph {"name":"Crosshatch","category":"Dessin","inputs":0,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[28.0,0.0,0.0,0.0],"name":"Frequency","text":"Frequency","index":1,"min":2.0,"max":512.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.449999988,0.0,0.0,0.0],"name":"Angle","text":"Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Width","text":"Width","index":1,"min":0.00100000005,"max":0.49000001},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0250000004,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.25},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.720000029,0.0,0.0,0.0],"name":"Tone","text":"Tone","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"InkOpacity","text":"Ink Opacity","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"PaperOpacity","text":"Paper Opacity","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0149999997,0.0120000001,0.00999999978,1.0],"name":"InkColor","text":"Ink Color","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[0.75,0.680000007,0.550000012,1.0],"name":"PaperColor","text":"Paper Color","index":1},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Crosshatch","text":"    float2 p=UV*max(Frequency,1.0); float c=cos(Angle),s=sin(Angle); float2 r=float2(c*p.x-s*p.y,s*p.x+c*p.y); float l0=1.0-smoothstep(Width,Width+Softness,abs(frac(r.x)-0.5)); float l1=1.0-smoothstep(Width,Width+Softness,abs(frac(r.y)-0.5)); float l2=1.0-smoothstep(Width,Width+Softness,abs(frac((r.x+r.y)*0.707)-0.5)); float l3=1.0-smoothstep(Width,Width+Softness,abs(frac((r.x-r.y)*0.707)-0.5)); float tone=saturate(Tone); float ink=max(l0*step(0.2,tone),max(l1*step(0.45,tone),max(l2*step(0.7,tone),l3*step(0.9,tone)))); return float4(lerp(PaperColor.rgb,InkColor.rgb,ink),lerp(PaperOpacity,InkOpacity,ink));","pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Angle",1],["Width",1],["Softness",1],["Tone",1],["InkOpacity",1],["PaperOpacity",1],["InkColor",3],["PaperColor",3]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Frequency float 28 2 512 "Frequency"
//@param Angle float 0.45 -6.283 6.283 "Angle"
//@param Width float 0.08 0.001 0.49 "Width"
//@param Softness float 0.025 0.0001 0.25 "Softness"
//@param Tone float 0.72 0 1 "Tone"
//@param InkOpacity float 1 0 1 "Ink Opacity"
//@param PaperOpacity float 1 0 1 "Paper Opacity"
//@param InkColor color 0.015 0.012 0.01 1 "Ink Color"
//@param PaperColor color 0.75 0.68 0.55 1 "Paper Color"

// Imported preset (Draw / Pattern): Layered crosshatch generator with tone-controlled stroke density and paper color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Angle, float Width, float Softness, float Tone, float InkOpacity, float PaperOpacity, float3 InkColor, float3 PaperColor)
{
    float2 p=UV*max(Frequency,1.0); float c=cos(Angle),s=sin(Angle); float2 r=float2(c*p.x-s*p.y,s*p.x+c*p.y); float l0=1.0-smoothstep(Width,Width+Softness,abs(frac(r.x)-0.5)); float l1=1.0-smoothstep(Width,Width+Softness,abs(frac(r.y)-0.5)); float l2=1.0-smoothstep(Width,Width+Softness,abs(frac((r.x+r.y)*0.707)-0.5)); float l3=1.0-smoothstep(Width,Width+Softness,abs(frac((r.x-r.y)*0.707)-0.5)); float tone=saturate(Tone); float ink=max(l0*step(0.2,tone),max(l1*step(0.45,tone),max(l2*step(0.7,tone),l3*step(0.9,tone)))); return float4(lerp(PaperColor.rgb,InkColor.rgb,ink),lerp(PaperOpacity,InkOpacity,ink));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Angle, Width, Softness, Tone, InkOpacity, PaperOpacity, InkColor.rgb, PaperColor.rgb);
}
