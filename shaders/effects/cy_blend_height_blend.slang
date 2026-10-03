//@name Height Blend
//@category Fusions
//@graph {"name":"Height Blend","category":"Fusions","inputs":3,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyBackground"},{"id":2,"op":"texture","pos":[40,150],"in":[],"value":[0,0,0,0],"index":1,"name":"CyForeground"},{"id":3,"op":"texture","pos":[40,260],"in":[],"value":[0,0,0,0],"index":2,"name":"CyMask"},{"id":4,"op":"uv","pos":[40,370],"in":[],"value":[0,0,0,0]},{"id":5,"op":"canvassize","pos":[40,460],"in":[],"value":[0,0,0,0]},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":64.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Bias","text":"Bias","index":1,"min":-2.0,"max":2.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.150000006,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":2.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"BackgroundHeightChannel","text":"Background Height Channel","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ForegroundHeightChannel","text":"Foreground Height Channel","index":1},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Height Blend","text":"    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float hb=dot(bg,BackgroundHeightChannel);float hf=dot(fg,ForegroundHeightChannel);float balance=(hf-hb)*Contrast+Bias;float height=smoothstep(-Softness,Softness,balance);float a=saturate(height*fg.a*mask*Opacity);return float4(lerp(bg.rgb,fg.rgb,a),a+bg.a*(1.0-a));","pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["Contrast",1],["Bias",1],["Softness",1],["Opacity",1],["BackgroundHeightChannel",4],["ForegroundHeightChannel",4]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}],"kind":1}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Contrast float 4 0 64 "Contrast"
//@param Bias float 0 -2 2 "Bias"
//@param Softness float 0.15 0.0001 2 "Softness"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundHeightChannel color 0 0 0 1 "Background Height Channel"
//@param ForegroundHeightChannel color 0 0 0 1 "Foreground Height Channel"

// Imported preset (Blend / PBR): Height-aware material blend using configurable foreground/background channels, contrast, bias, and transition softness.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Contrast, float Bias, float Softness, float Opacity, float4 BackgroundHeightChannel, float4 ForegroundHeightChannel)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float hb=dot(bg,BackgroundHeightChannel);float hf=dot(fg,ForegroundHeightChannel);float balance=(hf-hb)*Contrast+Bias;float height=smoothstep(-Softness,Softness,balance);float a=saturate(height*fg.a*mask*Opacity);return float4(lerp(bg.rgb,fg.rgb,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Contrast, Bias, Softness, Opacity, BackgroundHeightChannel, ForegroundHeightChannel);
}
