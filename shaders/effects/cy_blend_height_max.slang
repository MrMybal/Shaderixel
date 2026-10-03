//@name Height Max
//@category Fusions
//@graph {"name":"Height Max","category":"Fusions","inputs":3,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyBackground"},{"id":2,"op":"texture","pos":[40,150],"in":[],"value":[0,0,0,0],"index":1,"name":"CyForeground"},{"id":3,"op":"texture","pos":[40,260],"in":[],"value":[0,0,0,0],"index":2,"name":"CyMask"},{"id":4,"op":"uv","pos":[40,370],"in":[],"value":[0,0,0,0]},{"id":5,"op":"canvassize","pos":[40,460],"in":[],"value":[0,0,0,0]},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"HeightBias","text":"Height Bias","index":1,"min":-2.0,"max":2.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0500000007,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":1.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"BackgroundHeightChannel","text":"Background Height Channel","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ForegroundHeightChannel","text":"Foreground Height Channel","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Height Max","text":"    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float hb=dot(bg,BackgroundHeightChannel);float hf=dot(fg,ForegroundHeightChannel)+HeightBias;float selector=smoothstep(-Softness,Softness,hf-hb);float a=saturate(selector*mask*Opacity);float4 combined=lerp(bg,fg,a);return float4(combined.rgb,lerp(bg.a,max(bg.a,fg.a),a));","pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["HeightBias",1],["Softness",1],["Opacity",1],["BackgroundHeightChannel",4],["ForegroundHeightChannel",4]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}],"kind":1}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param HeightBias float 0 -2 2 "Height Bias"
//@param Softness float 0.05 0.0001 1 "Softness"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundHeightChannel color 0 0 0 1 "Background Height Channel"
//@param ForegroundHeightChannel color 0 0 0 1 "Foreground Height Channel"

// Imported preset (Blend / PBR): Selects the material with the greater packed height while preserving a controllable soft transition.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float HeightBias, float Softness, float Opacity, float4 BackgroundHeightChannel, float4 ForegroundHeightChannel)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float hb=dot(bg,BackgroundHeightChannel);float hf=dot(fg,ForegroundHeightChannel)+HeightBias;float selector=smoothstep(-Softness,Softness,hf-hb);float a=saturate(selector*mask*Opacity);float4 combined=lerp(bg,fg,a);return float4(combined.rgb,lerp(bg.a,max(bg.a,fg.a),a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), HeightBias, Softness, Opacity, BackgroundHeightChannel, ForegroundHeightChannel);
}
