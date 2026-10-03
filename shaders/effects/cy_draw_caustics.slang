//@name Caustics
//@category Dessin
//@graph {"name":"Caustics","category":"Dessin","inputs":0,"premultiplied":false,"nextId":15,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[3.5,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.00999999978,"max":64.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.699999988,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1,"min":0.0,"max":4.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[2.20000005,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":16.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[2.5,0.0,0.0,0.0],"name":"Sharpness","text":"Sharpness","index":1,"min":0.0500000007,"max":16.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":64.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BackgroundOpacity","text":"Background Opacity","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[0.0,0.0149999997,0.0399999991,1.0],"name":"DeepColor","text":"Deep Color","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[0.0500000007,0.850000024,2.5,1.0],"name":"LightColor","text":"Light Color","index":1},{"id":13,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0]],"value":[0,0,0,0],"name":"Caustics","text":"    float2 p=(UV-0.5)*max(Scale,0.01); float v=0.0; float amp=0.55; [unroll]for(int i=0;i<5;i++){p=float2(p.x*1.62-p.y*1.17,p.x*1.17+p.y*1.62)+float2(Seed*.17+i*1.31,Seed*.11-i*.73); float w=sin(p.x+sin(p.y*1.37))+sin(p.y+sin(p.x*1.21)); v+=abs(w)*amp; amp*=0.52;} v=pow(saturate(1.0-abs(v-Threshold)*Contrast),max(Sharpness,0.05)); float3 rgb=lerp(DeepColor.rgb,LightColor.rgb,v)*Intensity; return float4(rgb,lerp(BackgroundOpacity,Opacity,v));","pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Threshold",1],["Contrast",1],["Sharpness",1],["Intensity",1],["Opacity",1],["BackgroundOpacity",1],["DeepColor",3],["LightColor",3]],"outs":[["Resultat",4]]},{"id":14,"op":"output","pos":[780,40],"in":[[13,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Scale float 3.5 0.01 64 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Threshold float 0.7 0 4 "Threshold"
//@param Contrast float 2.2 0 16 "Contrast"
//@param Sharpness float 2.5 0.05 16 "Sharpness"
//@param Intensity float 2 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundOpacity float 1 0 1 "Background Opacity"
//@param DeepColor color 0 0.015 0.04 1 "Deep Color"
//@param LightColor color 0.05 0.85 2.5 1 "Light Color"

// Imported preset (Draw / Light & Space): Animated-looking layered caustic field for water light, energy, and emissive masks.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Threshold, float Contrast, float Sharpness, float Intensity, float Opacity, float BackgroundOpacity, float3 DeepColor, float3 LightColor)
{
    float2 p=(UV-0.5)*max(Scale,0.01); float v=0.0; float amp=0.55; [unroll]for(int i=0;i<5;i++){p=float2(p.x*1.62-p.y*1.17,p.x*1.17+p.y*1.62)+float2(Seed*.17+i*1.31,Seed*.11-i*.73); float w=sin(p.x+sin(p.y*1.37))+sin(p.y+sin(p.x*1.21)); v+=abs(w)*amp; amp*=0.52;} v=pow(saturate(1.0-abs(v-Threshold)*Contrast),max(Sharpness,0.05)); float3 rgb=lerp(DeepColor.rgb,LightColor.rgb,v)*Intensity; return float4(rgb,lerp(BackgroundOpacity,Opacity,v));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Threshold, Contrast, Sharpness, Intensity, Opacity, BackgroundOpacity, DeepColor.rgb, LightColor.rgb);
}
