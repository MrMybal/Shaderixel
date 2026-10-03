//@name Voronoi
//@category Dessin
//@graph {"name":"Voronoi","category":"Dessin","inputs":0,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[12.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":1.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":16.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"BorderContrast","text":"Border Contrast","index":1,"min":0.0,"max":64.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BorderMode","text":"Border Mode","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Voronoi","text":"    float2 p=UV*max(Scale,1.0); float2 ip=floor(p); float2 fp=frac(p); float nearest=10.0; float second=10.0; [unroll] for(int y=-1;y<=1;y++){[unroll] for(int x=-1;x<=1;x++){float2 o=float2(x,y); float2 cell=ip+o; float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+Seed)*43758.5453); float d=length(o+rnd-fp); if(d<nearest){second=nearest;nearest=d;}else if(d<second){second=d;}}} float border=saturate((second-nearest)*BorderContrast); float v=lerp(nearest,border,saturate(BorderMode)); v=saturate(v*Contrast); return float4(lerp(ColorA.rgb,ColorB.rgb,v),Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Contrast",1],["BorderContrast",1],["BorderMode",1],["Opacity",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Scale float 12 1 256 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Contrast float 1.5 0 16 "Contrast"
//@param BorderContrast float 8 0 64 "Border Contrast"
//@param BorderMode float 1 0 1 "Border Mode"
//@param Opacity float 1 0 1 "Opacity"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Noise): Voronoi cell distance and border generator.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Contrast, float BorderContrast, float BorderMode, float Opacity, float3 ColorA, float3 ColorB)
{
    float2 p=UV*max(Scale,1.0); float2 ip=floor(p); float2 fp=frac(p); float nearest=10.0; float second=10.0; [unroll] for(int y=-1;y<=1;y++){[unroll] for(int x=-1;x<=1;x++){float2 o=float2(x,y); float2 cell=ip+o; float2 rnd=frac(sin(float2(dot(cell,float2(127.1,311.7)),dot(cell,float2(269.5,183.3)))+Seed)*43758.5453); float d=length(o+rnd-fp); if(d<nearest){second=nearest;nearest=d;}else if(d<second){second=d;}}} float border=saturate((second-nearest)*BorderContrast); float v=lerp(nearest,border,saturate(BorderMode)); v=saturate(v*Contrast); return float4(lerp(ColorA.rgb,ColorB.rgb,v),Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Contrast, BorderContrast, BorderMode, Opacity, ColorA.rgb, ColorB.rgb);
}
