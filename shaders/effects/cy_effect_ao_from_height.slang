//@name AO From Height
//@category Traitements
//@graph {"name":"AO From Height","category":"Traitements","inputs":1,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":1.0,"max":512.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[24.0,0.0,0.0,0.0],"name":"Steps","text":"Steps","index":1,"min":4.0,"max":48.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.00499999989,0.0,0.0,0.0],"name":"Bias","text":"Bias","index":1,"min":-1.0,"max":1.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[5.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":64.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.20000005,0.0,0.0,0.0],"name":"Power","text":"Power","index":1,"min":0.0500000007,"max":8.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Invert","text":"Invert","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"HeightChannel","text":"Height Channel","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"AO From Height","text":"    float2 texel=1.0/max(CyCanvasSize.xy,1.0);float center=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,HeightChannel.rgb);int count=clamp((int)round(Steps),4,48);float occ=0;[loop]for(int i=0;i<48;i++){if(i>=count)break;float f=(i+.5)/count;float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*Radius*sqrt(f)*texel;float h=dot(Texture2DSample(CyInput,CyInputSampler,UV+o).rgb,HeightChannel.rgb);occ+=saturate((h-center-Bias)*Strength/(sqrt(f)+.05));}occ=pow(saturate(occ/count),Power);float ao=lerp(1.0-occ,occ,Invert);return float4(ao.xxx,Texture2DSample(CyInput,CyInputSampler,UV).a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Steps",1],["Bias",1],["Strength",1],["Power",1],["Invert",1],["HeightChannel",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 24 1 512 "Radius"
//@param Steps float 24 4 48 "Steps"
//@param Bias float 0.005 -1 1 "Bias"
//@param Strength float 5 0 64 "Strength"
//@param Power float 1.2 0.05 8 "Power"
//@param Invert float 0 0 1 "Invert"
//@param HeightChannel color 1 0 0 0 "Height Channel"

// Imported preset (Effect / PBR): Generates a horizon-style ambient-occlusion mask from a linear height texture with adjustable sample quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Steps, float Bias, float Strength, float Power, float Invert, float3 HeightChannel)
{
    float2 texel=1.0/max(CyCanvasSize.xy,1.0);float center=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,HeightChannel.rgb);int count=clamp((int)round(Steps),4,48);float occ=0;[loop]for(int i=0;i<48;i++){if(i>=count)break;float f=(i+.5)/count;float a=i*2.39996323;float2 o=float2(cos(a),sin(a))*Radius*sqrt(f)*texel;float h=dot(Texture2DSample(CyInput,CyInputSampler,UV+o).rgb,HeightChannel.rgb);occ+=saturate((h-center-Bias)*Strength/(sqrt(f)+.05));}occ=pow(saturate(occ/count),Power);float ao=lerp(1.0-occ,occ,Invert);return float4(ao.xxx,Texture2DSample(CyInput,CyInputSampler,UV).a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Steps, Bias, Strength, Power, Invert, HeightChannel.rgb);
}
