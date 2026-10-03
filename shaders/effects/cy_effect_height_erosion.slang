//@name Height Erosion
//@category Traitements
//@graph {"name":"Height Erosion","category":"Traitements","inputs":1,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":64.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0149999997,0.0,0.0,0.0],"name":"Talus","text":"Talus","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[12.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":128.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.649999976,0.0,0.0,0.0],"name":"Erosion","text":"Erosion","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.180000007,0.0,0.0,0.0],"name":"Deposition","text":"Deposition","index":1},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[128.0,0.0,0.0,0.0],"name":"NoiseScale","text":"Noise Scale","index":1,"min":1.0,"max":4096.0},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"NoiseAmount","text":"Noise Amount","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"HeightChannel","text":"Height Channel","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"ChannelMask","text":"Channel Mask","index":1},{"id":14,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0]],"value":[0,0,0,0],"name":"Height Erosion","text":"    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 src=Texture2DSample(CyInput,CyInputSampler,UV);float h=dot(src.rgb,HeightChannel.rgb);float lower=h,average=0;[unroll]for(int i=0;i<16;i++){float a=i*6.283185/16.0;float sampleH=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(cos(a),sin(a))*t).rgb,HeightChannel.rgb);lower=min(lower,sampleH);average+=sampleH;}average/=16;float slope=saturate((h-lower-Talus)*Strength);float noise=frac(sin(dot(floor(UV*NoiseScale)+Seed,float2(12.9898,78.233)))*43758.5453);float eroded=lerp(h,lower,slope*Erosion*lerp(1.0-NoiseAmount,1.0,noise));eroded=lerp(eroded,average,Deposition*(1.0-slope));float3 outColor=lerp(src.rgb,eroded.xxx,ChannelMask.rgb);return float4(outColor,src.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Talus",1],["Strength",1],["Erosion",1],["Deposition",1],["NoiseScale",1],["Seed",1],["NoiseAmount",1],["HeightChannel",3],["ChannelMask",3]],"outs":[["Resultat",4]]},{"id":15,"op":"output","pos":[780,40],"in":[[14,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 2 0.25 64 "Radius"
//@param Talus float 0.015 0 1 "Talus"
//@param Strength float 12 0 128 "Strength"
//@param Erosion float 0.65 0 1 "Erosion"
//@param Deposition float 0.18 0 1 "Deposition"
//@param NoiseScale float 128 1 4096 "Noise Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param NoiseAmount float 0.35 0 1 "Noise Amount"
//@param HeightChannel color 1 0 0 0 "Height Channel"
//@param ChannelMask color 1 1 1 0 "Channel Mask"

// Imported preset (Effect / PBR): Iterative-looking height erosion approximation with talus, deposition, and noise-driven weathering.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Talus, float Strength, float Erosion, float Deposition, float NoiseScale, float Seed, float NoiseAmount, float3 HeightChannel, float3 ChannelMask)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 src=Texture2DSample(CyInput,CyInputSampler,UV);float h=dot(src.rgb,HeightChannel.rgb);float lower=h,average=0;[unroll]for(int i=0;i<16;i++){float a=i*6.283185/16.0;float sampleH=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(cos(a),sin(a))*t).rgb,HeightChannel.rgb);lower=min(lower,sampleH);average+=sampleH;}average/=16;float slope=saturate((h-lower-Talus)*Strength);float noise=frac(sin(dot(floor(UV*NoiseScale)+Seed,float2(12.9898,78.233)))*43758.5453);float eroded=lerp(h,lower,slope*Erosion*lerp(1.0-NoiseAmount,1.0,noise));eroded=lerp(eroded,average,Deposition*(1.0-slope));float3 outColor=lerp(src.rgb,eroded.xxx,ChannelMask.rgb);return float4(outColor,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Talus, Strength, Erosion, Deposition, NoiseScale, Seed, NoiseAmount, HeightChannel.rgb, ChannelMask.rgb);
}
