//@name Fractal Noise
//@category Dessin
//@graph {"name":"Fractal Noise","category":"Dessin","inputs":0,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[6.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.100000001,"max":512.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Lacunarity","text":"Lacunarity","index":1,"min":1.10000002,"max":4.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Gain","text":"Gain","index":1,"min":0.100000001,"max":0.899999976},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":8.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityA","text":"Opacity A","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityB","text":"Opacity B","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Fractal Noise","text":"    float2 p=UV*max(Scale,1.0)+Seed; float n=0.0; float amp=0.5714; float sum=0.0; [unroll] for(int o=0;o<5;o++){float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f); float a=frac(sin(dot(i,float2(127.1,311.7)))*43758.5453); float b=frac(sin(dot(i+float2(1,0),float2(127.1,311.7)))*43758.5453); float c=frac(sin(dot(i+float2(0,1),float2(127.1,311.7)))*43758.5453); float d=frac(sin(dot(i+1.0,float2(127.1,311.7)))*43758.5453); n+=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y)*amp; sum+=amp; p=p*Lacunarity+13.17; amp*=Gain;} n=saturate((n/max(sum,0.001)-0.5)*Contrast+0.5); return float4(lerp(ColorA.rgb,ColorB.rgb,n),lerp(OpacityA,OpacityB,n));","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Lacunarity",1],["Gain",1],["Contrast",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Scale float 6 0.1 512 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Lacunarity float 2 1.1 4 "Lacunarity"
//@param Gain float 0.5 0.1 0.9 "Gain"
//@param Contrast float 1 0 8 "Contrast"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Noise): Multi-octave procedural noise for clouds, masks, and roughness.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Lacunarity, float Gain, float Contrast, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p=UV*max(Scale,1.0)+Seed; float n=0.0; float amp=0.5714; float sum=0.0; [unroll] for(int o=0;o<5;o++){float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f); float a=frac(sin(dot(i,float2(127.1,311.7)))*43758.5453); float b=frac(sin(dot(i+float2(1,0),float2(127.1,311.7)))*43758.5453); float c=frac(sin(dot(i+float2(0,1),float2(127.1,311.7)))*43758.5453); float d=frac(sin(dot(i+1.0,float2(127.1,311.7)))*43758.5453); n+=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y)*amp; sum+=amp; p=p*Lacunarity+13.17; amp*=Gain;} n=saturate((n/max(sum,0.001)-0.5)*Contrast+0.5); return float4(lerp(ColorA.rgb,ColorB.rgb,n),lerp(OpacityA,OpacityB,n));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Lacunarity, Gain, Contrast, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
