//@name Pixelate
//@category Traitements
//@graph {"name":"Pixelate","category":"Traitements","inputs":1,"premultiplied":false,"nextId":8,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[128.0,0.0,0.0,0.0],"name":"BlocksX","text":"Blocks X","index":1,"min":1.0,"max":8192.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[128.0,0.0,0.0,0.0],"name":"BlocksY","text":"Blocks Y","index":1,"min":1.0,"max":8192.0},{"id":6,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0]],"value":[0,0,0,0],"name":"Pixelate","text":"    float2 count=max(float2(BlocksX,BlocksY),1.0); float2 q=(floor(UV*count)+0.5)/count; return Texture2DSample(CyInput,CyInputSampler,q);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["BlocksX",1],["BlocksY",1]],"outs":[["Resultat",4]]},{"id":7,"op":"output","pos":[780,40],"in":[[6,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param BlocksX float 128 1 8192 "Blocks X"
//@param BlocksY float 128 1 8192 "Blocks Y"

// Imported preset (Effect / Stylize): Pixelates the source into configurable blocks.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float BlocksX, float BlocksY)
{
    float2 count=max(float2(BlocksX,BlocksY),1.0); float2 q=(floor(UV*count)+0.5)/count; return Texture2DSample(CyInput,CyInputSampler,q);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), BlocksX, BlocksY);
}
