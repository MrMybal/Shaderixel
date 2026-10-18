//@name SDF Arc
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"SDF Arc","nextId":19,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":8,"min":0.100000001,"name":"Aspect","op":"param","pos":[40, 220],"text":"Aspect","value":[1, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[0.300000012, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"Thickness","op":"param","pos":[40, 440],"text":"Thickness","value":[0.0179999992, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.100000001,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.00400000019, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"MidAngle","op":"param","pos":[40, 660],"text":"Mid Angle","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":6.28299999,"min":0.00999999978,"name":"ArcAngle","op":"param","pos":[40, 770],"text":"Arc Angle","value":[3.4000001, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"AngularSoftness","op":"param","pos":[40, 880],"text":"Angular Softness","value":[0.00999999978, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"GlowRadius","op":"param","pos":[40, 990],"text":"Glow Radius","value":[0.0799999982, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":16,"min":0.100000001,"name":"GlowFalloff","op":"param","pos":[40, 1100],"text":"Glow Falloff","value":[2, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":16,"min":0,"name":"GlowIntensity","op":"param","pos":[40, 1210],"text":"Glow Intensity","value":[0.699999988, 0, 0, 0]},{"id":13,"in":[],"index":1,"max":64,"min":0,"name":"Intensity","op":"param","pos":[40, 1320],"text":"Intensity","value":[2, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1430],"text":"Opacity","value":[1, 0, 0, 0]},{"id":15,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1540],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":16,"in":[],"index":1,"name":"Color","op":"paramcolor","pos":[40, 1650],"text":"Color","value":[0.100000001, 1.10000002, 2.5, 1]},{"id":17,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0],[16, 0]],"name":"SDF Arc","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["Radius",1],["Thickness",1],["Softness",1],["MidAngle",1],["ArcAngle",1],["AngularSoftness",1],["GlowRadius",1],["GlowFalloff",1],["GlowIntensity",1],["Intensity",1],["Opacity",1],["Center",3],["Color",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float angle = atan2(p.y, p.x);\n    float radius = length(p);\n    float da = abs(atan2(sin(angle - MidAngle), cos(angle - MidAngle)));\n    float arcMask = 1.0 - smoothstep(ArcAngle * 0.5, ArcAngle * 0.5 + AngularSoftness, da);\n    float ring = 1.0 - smoothstep(Thickness, Thickness + Softness, abs(radius - Radius));\n    float capAngle = ArcAngle * 0.5;\n    float2 c0 = float2(cos(MidAngle - capAngle), sin(MidAngle - capAngle)) * Radius;\n    float2 c1 = float2(cos(MidAngle + capAngle), sin(MidAngle + capAngle)) * Radius;\n    float caps = max(1.0 - smoothstep(Thickness, Thickness + Softness, length(p - c0)), 1.0 - smoothstep(Thickness, Thickness + Softness, length(p - c1)));\n    float m = max(ring * arcMask, caps);\n    float glow = pow(saturate(1.0 - abs(radius - Radius) / max(GlowRadius, 0.0001)), GlowFalloff) * arcMask;\n    float3 rgb = Color.rgb * (m + glow * GlowIntensity) * Intensity;\n    return float4(rgb, saturate(max(m, glow) * Opacity));","value":[0, 0, 0, 0]},{"id":18,"in":[[17, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param Radius float 0.3 0.001 2 "Radius"
//@param Thickness float 0.018 0.001 0.5 "Thickness"
//@param Softness float 0.004 0.0001 0.1 "Softness"
//@param MidAngle float 0 -6.283 6.283 "Mid Angle"
//@param ArcAngle float 3.4 0.01 6.283 "Arc Angle"
//@param AngularSoftness float 0.01 0.0001 0.5 "Angular Softness"
//@param GlowRadius float 0.08 0.001 1 "Glow Radius"
//@param GlowFalloff float 2 0.1 16 "Glow Falloff"
//@param GlowIntensity float 0.7 0 16 "Glow Intensity"
//@param Intensity float 2 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Color color 0.1 1.1 2.5 1 "Color"
// Imported preset (Draw / Shape & Gradient): Antialiased signed-distance arc with round caps, glow, and HDR output.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float Thickness, float Softness, float MidAngle, float ArcAngle, float AngularSoftness, float GlowRadius, float GlowFalloff, float GlowIntensity, float Intensity, float Opacity, float3 Center, float3 Color)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float angle = atan2(p.y, p.x);
    float radius = length(p);
    float da = abs(atan2(sin(angle - MidAngle), cos(angle - MidAngle)));
    float arcMask = 1.0 - smoothstep(ArcAngle * 0.5, ArcAngle * 0.5 + AngularSoftness, da);
    float ring = 1.0 - smoothstep(Thickness, Thickness + Softness, abs(radius - Radius));
    float capAngle = ArcAngle * 0.5;
    float2 c0 = float2(cos(MidAngle - capAngle), sin(MidAngle - capAngle)) * Radius;
    float2 c1 = float2(cos(MidAngle + capAngle), sin(MidAngle + capAngle)) * Radius;
    float caps = max(1.0 - smoothstep(Thickness, Thickness + Softness, length(p - c0)), 1.0 - smoothstep(Thickness, Thickness + Softness, length(p - c1)));
    float m = max(ring * arcMask, caps);
    float glow = pow(saturate(1.0 - abs(radius - Radius) / max(GlowRadius, 0.0001)), GlowFalloff) * arcMask;
    float3 rgb = Color.rgb * (m + glow * GlowIntensity) * Intensity;
    return float4(rgb, saturate(max(m, glow) * Opacity));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, Thickness, Softness, MidAngle, ArcAngle, AngularSoftness, GlowRadius, GlowFalloff, GlowIntensity, Intensity, Opacity, float3(Center, 0.0), Color.rgb);
}
