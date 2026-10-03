//@name Micro Detail Overlay
//@category Fusions
//@graph {"name":"Micro Detail Overlay","category":"Fusions","inputs":3,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyBackground"},{"id":2,"op":"texture","pos":[40,150],"in":[],"value":[0,0,0,0],"index":1,"name":"CyForeground"},{"id":3,"op":"texture","pos":[40,260],"in":[],"value":[0,0,0,0],"index":2,"name":"CyMask"},{"id":4,"op":"uv","pos":[40,370],"in":[],"value":[0,0,0,0]},{"id":5,"op":"canvassize","pos":[40,460],"in":[],"value":[0,0,0,0]},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Neutral","text":"Neutral","index":1,"min":-8.0,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":16.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"DetailChannel","text":"Detail Channel","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"TargetChannels","text":"Target Channels","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Micro Detail Overlay","text":"    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float detail=dot(fg,DetailChannel)-Neutral;float a=saturate(fg.a*mask*Opacity);float3 headroom=min(bg.rgb,1.0-bg.rgb)*2.0;float3 modified=bg.rgb+detail*Strength*headroom*TargetChannels.rgb;float3 rgb=lerp(bg.rgb,modified,a);return float4(rgb,bg.a);","pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["Neutral",1],["Strength",1],["Opacity",1],["DetailChannel",4],["TargetChannels",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}],"kind":1}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Neutral float 0.5 -8 8 "Neutral"
//@param Strength float 1 0 16 "Strength"
//@param Opacity float 1 0 1 "Opacity"
//@param DetailChannel color 1 0 0 0 "Detail Channel"
//@param TargetChannels color 1 1 1 0 "Target Channels"

// Imported preset (Blend / PBR): Adds signed micro-detail to selected PBR channels with range-safe attenuation near black and white.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Neutral, float Strength, float Opacity, float4 DetailChannel, float3 TargetChannels)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float detail=dot(fg,DetailChannel)-Neutral;float a=saturate(fg.a*mask*Opacity);float3 headroom=min(bg.rgb,1.0-bg.rgb)*2.0;float3 modified=bg.rgb+detail*Strength*headroom*TargetChannels.rgb;float3 rgb=lerp(bg.rgb,modified,a);return float4(rgb,bg.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Neutral, Strength, Opacity, DetailChannel, TargetChannels.rgb);
}
