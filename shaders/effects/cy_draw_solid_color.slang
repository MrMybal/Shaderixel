//@name Solid Color
//@category Dessin
//@graph {"name":"Solid Color","category":"Dessin","inputs":0,"premultiplied":false,"nextId":7,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":4,"op":"paramcolor","pos":[40,330],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"Color","text":"Color","index":1},{"id":5,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0]],"value":[0,0,0,0],"name":"Solid Color","text":"    return float4(Color,Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Opacity",1],["Color",3]],"outs":[["Resultat",4]]},{"id":6,"op":"output","pos":[780,40],"in":[[5,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Opacity float 1 0 1 "Opacity"
//@param Color color 1 1 1 1 "Color"

// Imported preset (Draw / Shape & Gradient): Fills the complete layer with an HDR color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity, float3 Color)
{
    return float4(Color,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity, Color.rgb);
}
