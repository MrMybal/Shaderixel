//@name Truchet Tiles
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Truchet Tiles","nextId":16,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":256,"min":1,"name":"Tiling","op":"param","pos":[40, 220],"text":"Tiling","value":[10, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.25,"min":0.00100000005,"name":"Thickness","op":"param","pos":[40, 440],"text":"Thickness","value":[0.0350000001, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.100000001,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.00999999978, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":0.699999988,"min":0.00100000005,"name":"GlowRadius","op":"param","pos":[40, 660],"text":"Glow Radius","value":[0.180000007, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":16,"min":0.100000001,"name":"GlowFalloff","op":"param","pos":[40, 770],"text":"Glow Falloff","value":[3, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":16,"min":0,"name":"GlowIntensity","op":"param","pos":[40, 880],"text":"Glow Intensity","value":[0.25, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"LineOpacity","op":"param","pos":[40, 990],"text":"Line Opacity","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"BackgroundOpacity","op":"param","pos":[40, 1100],"text":"Background Opacity","value":[1, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"LineColor","op":"paramcolor","pos":[40, 1210],"text":"Line Color","value":[0.150000006, 0.800000012, 1.79999995, 1]},{"id":13,"in":[],"index":1,"name":"BackgroundColor","op":"paramcolor","pos":[40, 1320],"text":"Background Color","value":[0, 0, 0, 1]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"Truchet Tiles","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Tiling",1],["Seed",1],["Thickness",1],["Softness",1],["GlowRadius",1],["GlowFalloff",1],["GlowIntensity",1],["LineOpacity",1],["BackgroundOpacity",1],["LineColor",3],["BackgroundColor",3]],"pos":[420, 40],"text":"    float2 q = UV * max(Tiling, 1.0);\n    float2 id = floor(q);\n    float2 p = frac(q);\n    float rnd = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);\n    if (rnd > 0.5) p.x = 1.0 - p.x;\n    float d = min(abs(length(p) - 0.5), abs(length(p - 1.0) - 0.5));\n    float lineMask = 1.0 - smoothstep(Thickness, Thickness + Softness, d);\n    float glow = pow(saturate(1.0 - d / max(GlowRadius, 0.0001)), max(GlowFalloff, 0.1));\n    float3 rgb = BackgroundColor.rgb + LineColor.rgb * (lineMask + glow * GlowIntensity);\n    return float4(rgb, lerp(BackgroundOpacity, LineOpacity, max(lineMask, glow)));","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Tiling float 10 1 256 "Tiling"
//@param Seed float 0 -1000 1000 "Seed"
//@param Thickness float 0.035 0.001 0.25 "Thickness"
//@param Softness float 0.01 0.0001 0.1 "Softness"
//@param GlowRadius float 0.18 0.001 0.7 "Glow Radius"
//@param GlowFalloff float 3 0.1 16 "Glow Falloff"
//@param GlowIntensity float 0.25 0 16 "Glow Intensity"
//@param LineOpacity float 1 0 1 "Line Opacity"
//@param BackgroundOpacity float 1 0 1 "Background Opacity"
//@param LineColor color 0.15 0.8 1.8 1 "Line Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"
// Imported preset (Draw / Pattern): Seeded Truchet arc tiles for procedural ornament, paths, and seamless patterns.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tiling, float Seed, float Thickness, float Softness, float GlowRadius, float GlowFalloff, float GlowIntensity, float LineOpacity, float BackgroundOpacity, float3 LineColor, float3 BackgroundColor)
{
    float2 q = UV * max(Tiling, 1.0);
    float2 id = floor(q);
    float2 p = frac(q);
    float rnd = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);
    if (rnd > 0.5) p.x = 1.0 - p.x;
    float d = min(abs(length(p) - 0.5), abs(length(p - 1.0) - 0.5));
    float lineMask = 1.0 - smoothstep(Thickness, Thickness + Softness, d);
    float glow = pow(saturate(1.0 - d / max(GlowRadius, 0.0001)), max(GlowFalloff, 0.1));
    float3 rgb = BackgroundColor.rgb + LineColor.rgb * (lineMask + glow * GlowIntensity);
    return float4(rgb, lerp(BackgroundOpacity, LineOpacity, max(lineMask, glow)));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tiling, Seed, Thickness, Softness, GlowRadius, GlowFalloff, GlowIntensity, LineOpacity, BackgroundOpacity, LineColor.rgb, BackgroundColor.rgb);
}
