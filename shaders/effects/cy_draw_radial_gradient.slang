//@name Radial Gradient
//@category Dessin
//@graph {"name":"Radial Gradient","category":"Dessin","inputs":0,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00100000005,"max":2.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.150000006,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"InnerOpacity","text":"Inner Opacity","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"OuterOpacity","text":"Outer Opacity","index":1},{"id":7,"op":"param2","pos":[40,660],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"InnerColor","text":"Inner Color","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"OuterColor","text":"Outer Color","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"Radial Gradient","text":"    float d=length(UV-Center.xy); float t=smoothstep(max(Radius-Softness,0.0),Radius,d); return float4(lerp(InnerColor,OuterColor,t),lerp(InnerOpacity,OuterOpacity,t));","pins":[["UV",2],["CyCanvasSize",4],["Radius",1],["Softness",1],["InnerOpacity",1],["OuterOpacity",1],["Center",3],["InnerColor",3],["OuterColor",3]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Radius float 0.35 0.001 2 "Radius"
//@param Softness float 0.15 0 1 "Softness"
//@param InnerOpacity float 1 0 1 "Inner Opacity"
//@param OuterOpacity float 0 0 1 "Outer Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param InnerColor color 1 1 1 1 "Inner Color"
//@param OuterColor color 0 0 0 0 "Outer Color"

// Imported preset (Draw / Shape & Gradient): Configurable radial gradient useful for shapes and vignettes.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Softness, float InnerOpacity, float OuterOpacity, float3 Center, float3 InnerColor, float3 OuterColor)
{
    float d=length(UV-Center.xy); float t=smoothstep(max(Radius-Softness,0.0),Radius,d); return float4(lerp(InnerColor,OuterColor,t),lerp(InnerOpacity,OuterOpacity,t));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Softness, InnerOpacity, OuterOpacity, float3(Center, 0.0), InnerColor.rgb, OuterColor.rgb);
}
