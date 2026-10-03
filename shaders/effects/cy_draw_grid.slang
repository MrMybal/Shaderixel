//@name Grid
//@category Dessin
//@graph {"name":"Grid","category":"Dessin","inputs":0,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[16.0,0.0,0.0,0.0],"name":"Divisions","text":"Divisions","index":1,"min":1.0,"max":512.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0350000001,0.0,0.0,0.0],"name":"LineWidth","text":"Line Width","index":1,"min":0.00100000005,"max":0.49000001},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.00999999978,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.200000003},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"LineOpacity","text":"Line Opacity","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"BackgroundOpacity","text":"Background Opacity","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"LineColor","text":"Line Color","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"BackgroundColor","text":"Background Color","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"Grid","text":"    float2 g=abs(frac(UV*max(Divisions,1.0)+0.5)-0.5); float gridLine=1.0-smoothstep(LineWidth,LineWidth+Softness,min(g.x,g.y)); return float4(lerp(BackgroundColor.rgb,LineColor.rgb,gridLine),lerp(BackgroundOpacity,LineOpacity,gridLine));","pins":[["UV",2],["CyCanvasSize",4],["Divisions",1],["LineWidth",1],["Softness",1],["LineOpacity",1],["BackgroundOpacity",1],["LineColor",3],["BackgroundColor",3]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Divisions float 16 1 512 "Divisions"
//@param LineWidth float 0.035 0.001 0.49 "Line Width"
//@param Softness float 0.01 0.0001 0.2 "Softness"
//@param LineOpacity float 1 0 1 "Line Opacity"
//@param BackgroundOpacity float 1 0 1 "Background Opacity"
//@param LineColor color 1 1 1 1 "Line Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported preset (Draw / Pattern): Procedural grid with independent background and line colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Divisions, float LineWidth, float Softness, float LineOpacity, float BackgroundOpacity, float3 LineColor, float3 BackgroundColor)
{
    float2 g=abs(frac(UV*max(Divisions,1.0)+0.5)-0.5); float gridLine=1.0-smoothstep(LineWidth,LineWidth+Softness,min(g.x,g.y)); return float4(lerp(BackgroundColor.rgb,LineColor.rgb,gridLine),lerp(BackgroundOpacity,LineOpacity,gridLine));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Divisions, LineWidth, Softness, LineOpacity, BackgroundOpacity, LineColor.rgb, BackgroundColor.rgb);
}
