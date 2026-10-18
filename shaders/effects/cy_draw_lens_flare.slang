//@name Lens Flare
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Lens Flare","nextId":26,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":8,"min":0.100000001,"name":"Aspect","op":"param","pos":[40, 220],"text":"Aspect","value":[1, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"CoreRadius","op":"param","pos":[40, 330],"text":"Core Radius","value":[0.0599999987, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":16,"min":0.100000001,"name":"CoreFalloff","op":"param","pos":[40, 440],"text":"Core Falloff","value":[2, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":64,"min":0,"name":"CoreIntensity","op":"param","pos":[40, 550],"text":"Core Intensity","value":[8, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"HaloRadius","op":"param","pos":[40, 660],"text":"Halo Radius","value":[0.219999999, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"HaloWidth","op":"param","pos":[40, 770],"text":"Halo Width","value":[0.0250000004, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":16,"min":0.100000001,"name":"HaloFalloff","op":"param","pos":[40, 880],"text":"Halo Falloff","value":[2, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":32,"min":0,"name":"HaloIntensity","op":"param","pos":[40, 990],"text":"Halo Intensity","value":[1.5, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":64,"min":1,"name":"RayCount","op":"param","pos":[40, 1100],"text":"Ray Count","value":[8, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":64,"min":1,"name":"RaySharpness","op":"param","pos":[40, 1210],"text":"Ray Sharpness","value":[12, 0, 0, 0]},{"id":13,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"RayLength","op":"param","pos":[40, 1320],"text":"Ray Length","value":[0.449999988, 0, 0, 0]},{"id":14,"in":[],"index":1,"max":32,"min":0,"name":"RayIntensity","op":"param","pos":[40, 1430],"text":"Ray Intensity","value":[1.5, 0, 0, 0]},{"id":15,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Rotation","op":"param","pos":[40, 1540],"text":"Rotation","value":[0, 0, 0, 0]},{"id":16,"in":[],"index":1,"max":3,"min":0,"name":"GhostSpacing","op":"param","pos":[40, 1650],"text":"Ghost Spacing","value":[0.5, 0, 0, 0]},{"id":17,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"GhostRadius","op":"param","pos":[40, 1760],"text":"Ghost Radius","value":[0.0799999982, 0, 0, 0]},{"id":18,"in":[],"index":1,"max":32,"min":0,"name":"GhostIntensity","op":"param","pos":[40, 1870],"text":"Ghost Intensity","value":[1, 0, 0, 0]},{"id":19,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1980],"text":"Opacity","value":[1, 0, 0, 0]},{"id":20,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 2090],"text":"Center","value":[0.379999995, 0.419999987, 0, 0]},{"id":21,"in":[],"index":1,"name":"Direction","op":"param2","pos":[40, 2200],"text":"Direction","value":[0.280000001, 0.0799999982, 0, 0]},{"id":22,"in":[],"index":1,"name":"PrimaryColor","op":"paramcolor","pos":[40, 2310],"text":"Primary Color","value":[1, 0.649999976, 0.25, 1]},{"id":23,"in":[],"index":1,"name":"SecondaryColor","op":"paramcolor","pos":[40, 2420],"text":"Secondary Color","value":[0.180000007, 0.449999988, 1, 1]},{"id":24,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0],[16, 0],[17, 0],[18, 0],[19, 0],[20, 0],[21, 0],[22, 0],[23, 0]],"name":"Lens Flare","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["CoreRadius",1],["CoreFalloff",1],["CoreIntensity",1],["HaloRadius",1],["HaloWidth",1],["HaloFalloff",1],["HaloIntensity",1],["RayCount",1],["RaySharpness",1],["RayLength",1],["RayIntensity",1],["Rotation",1],["GhostSpacing",1],["GhostRadius",1],["GhostIntensity",1],["Opacity",1],["Center",3],["Direction",3],["PrimaryColor",3],["SecondaryColor",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float d = length(p);\n    float core = pow(saturate(1.0 - d / max(CoreRadius, 0.0001)), max(CoreFalloff, 0.1)) * CoreIntensity;\n    float halo = pow(saturate(1.0 - abs(d - HaloRadius) / max(HaloWidth, 0.0001)), max(HaloFalloff, 0.1)) * HaloIntensity;\n    float angle = atan2(p.y, p.x) + Rotation;\n    float rays = pow(abs(cos(angle * max(RayCount, 1.0))), max(RaySharpness, 1.0)) * pow(saturate(1.0 - d / max(RayLength, 0.0001)), 2.0) * RayIntensity;\n    float2 axis = Direction.xy;\n    float g1 = pow(saturate(1.0 - length(p - axis * GhostSpacing) / max(GhostRadius, 0.0001)), 2.0);\n    float g2 = pow(saturate(1.0 - length(p + axis * GhostSpacing * 1.7) / max(GhostRadius * 0.7, 0.0001)), 2.0);\n    float3 rgb = PrimaryColor.rgb * (core + halo + rays) + SecondaryColor.rgb * (g1 + g2) * GhostIntensity;\n    float a = saturate(max(max(rgb.r, rgb.g), rgb.b) * Opacity);\n    return float4(rgb * Opacity, a);","value":[0, 0, 0, 0]},{"id":25,"in":[[24, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param CoreRadius float 0.06 0.001 1 "Core Radius"
//@param CoreFalloff float 2 0.1 16 "Core Falloff"
//@param CoreIntensity float 8 0 64 "Core Intensity"
//@param HaloRadius float 0.22 0.001 2 "Halo Radius"
//@param HaloWidth float 0.025 0.001 0.5 "Halo Width"
//@param HaloFalloff float 2 0.1 16 "Halo Falloff"
//@param HaloIntensity float 1.5 0 32 "Halo Intensity"
//@param RayCount float 8 1 64 "Ray Count"
//@param RaySharpness float 12 1 64 "Ray Sharpness"
//@param RayLength float 0.45 0.001 2 "Ray Length"
//@param RayIntensity float 1.5 0 32 "Ray Intensity"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param GhostSpacing float 0.5 0 3 "Ghost Spacing"
//@param GhostRadius float 0.08 0.001 1 "Ghost Radius"
//@param GhostIntensity float 1 0 32 "Ghost Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.38 0.42 0 1 "Center"
//@param Direction float2 0.28 0.08 0 1 "Direction"
//@param PrimaryColor color 1 0.65 0.25 1 "Primary Color"
//@param SecondaryColor color 0.18 0.45 1 1 "Secondary Color"
// Imported preset (Draw / Light & Space): HDR cinematic lens flare with core, halo, rays, and colored ghosts.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float CoreRadius, float CoreFalloff, float CoreIntensity, float HaloRadius, float HaloWidth, float HaloFalloff, float HaloIntensity, float RayCount, float RaySharpness, float RayLength, float RayIntensity, float Rotation, float GhostSpacing, float GhostRadius, float GhostIntensity, float Opacity, float3 Center, float3 Direction, float3 PrimaryColor, float3 SecondaryColor)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float d = length(p);
    float core = pow(saturate(1.0 - d / max(CoreRadius, 0.0001)), max(CoreFalloff, 0.1)) * CoreIntensity;
    float halo = pow(saturate(1.0 - abs(d - HaloRadius) / max(HaloWidth, 0.0001)), max(HaloFalloff, 0.1)) * HaloIntensity;
    float angle = atan2(p.y, p.x) + Rotation;
    float rays = pow(abs(cos(angle * max(RayCount, 1.0))), max(RaySharpness, 1.0)) * pow(saturate(1.0 - d / max(RayLength, 0.0001)), 2.0) * RayIntensity;
    float2 axis = Direction.xy;
    float g1 = pow(saturate(1.0 - length(p - axis * GhostSpacing) / max(GhostRadius, 0.0001)), 2.0);
    float g2 = pow(saturate(1.0 - length(p + axis * GhostSpacing * 1.7) / max(GhostRadius * 0.7, 0.0001)), 2.0);
    float3 rgb = PrimaryColor.rgb * (core + halo + rays) + SecondaryColor.rgb * (g1 + g2) * GhostIntensity;
    float a = saturate(max(max(rgb.r, rgb.g), rgb.b) * Opacity);
    return float4(rgb * Opacity, a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, CoreRadius, CoreFalloff, CoreIntensity, HaloRadius, HaloWidth, HaloFalloff, HaloIntensity, RayCount, RaySharpness, RayLength, RayIntensity, Rotation, GhostSpacing, GhostRadius, GhostIntensity, Opacity, float3(Center, 0.0), float3(Direction, 0.0), PrimaryColor.rgb, SecondaryColor.rgb);
}
