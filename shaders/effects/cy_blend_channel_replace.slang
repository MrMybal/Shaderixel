//@name Channel Replace
//@category Fusions
//@graph {"name":"Channel Replace","category":"Fusions","inputs":3,"premultiplied":false,"nextId":14,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyBackground"},{"id":2,"op":"texture","pos":[40,150],"in":[],"value":[0,0,0,0],"index":1,"name":"CyForeground"},{"id":3,"op":"texture","pos":[40,260],"in":[],"value":[0,0,0,0],"index":2,"name":"CyMask"},{"id":4,"op":"uv","pos":[40,370],"in":[],"value":[0,0,0,0]},{"id":5,"op":"canvassize","pos":[40,460],"in":[],"value":[0,0,0,0]},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"ReplaceAlpha","text":"Replace Alpha","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"SourceForRed","text":"Source For Red","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0,1.0,0.0,0.0],"name":"SourceForGreen","text":"Source For Green","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0,0.0,1.0,0.0],"name":"SourceForBlue","text":"Source For Blue","index":1},{"id":11,"op":"paramcolor","pos":[40,1100],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"ReplaceChannels","text":"Replace Channels","index":1},{"id":12,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0]],"value":[0,0,0,0],"name":"Channel Replace","text":"    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float3 mapped=float3(dot(fg,SourceForRed),dot(fg,SourceForGreen),dot(fg,SourceForBlue));float3 weights=saturate(ReplaceChannels.rgb);float a=saturate(mask*fg.a*Opacity);float3 blend=lerp(bg.rgb,mapped,weights);return float4(lerp(bg.rgb,blend,a),lerp(bg.a,fg.a,saturate(ReplaceAlpha*mask*Opacity)));","pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["ReplaceAlpha",1],["Opacity",1],["SourceForRed",4],["SourceForGreen",4],["SourceForBlue",4],["ReplaceChannels",3]],"outs":[["Resultat",4]]},{"id":13,"op":"output","pos":[780,40],"in":[[12,0],[0,0],[0,0]],"value":[0,0,0,0]}],"kind":1}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param ReplaceAlpha float 0 0 1 "Replace Alpha"
//@param Opacity float 1 0 1 "Opacity"
//@param SourceForRed color 1 0 0 0 "Source For Red"
//@param SourceForGreen color 0 1 0 0 "Source For Green"
//@param SourceForBlue color 0 0 1 0 "Source For Blue"
//@param ReplaceChannels color 1 1 1 0 "Replace Channels"

// Imported preset (Blend / Utility): Replaces any weighted combination of background RGB channels with foreground channels.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float ReplaceAlpha, float Opacity, float4 SourceForRed, float4 SourceForGreen, float4 SourceForBlue, float3 ReplaceChannels)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float3 mapped=float3(dot(fg,SourceForRed),dot(fg,SourceForGreen),dot(fg,SourceForBlue));float3 weights=saturate(ReplaceChannels.rgb);float a=saturate(mask*fg.a*Opacity);float3 blend=lerp(bg.rgb,mapped,weights);return float4(lerp(bg.rgb,blend,a),lerp(bg.a,fg.a,saturate(ReplaceAlpha*mask*Opacity)));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ReplaceAlpha, Opacity, SourceForRed, SourceForGreen, SourceForBlue, ReplaceChannels.rgb);
}
