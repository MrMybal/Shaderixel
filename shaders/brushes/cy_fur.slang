//@name Fur
//@category CyTexture Particules
//@selfflow
//@spacing 0.3
//@param Length float 0.43 0.02 0.7 "Length"
//@param Width float 0.7 0.05 1.2 "Width"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param FiberWidth float 0.012 0.001 0.2 "Fiber Width"
//@param Softness float 0.006 0.0001 0.1 "Softness"
//@param Bend float 0.22 0 1 "Bend"
//@param Seed float 0 -1000 1000 "Seed"
//@param RandomSeed float 1 0 1 "Graine par touche"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Particles): Clustered tapered fibers with seeded direction and length variation.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Length, float Width, float Rotation, float FiberWidth, float Softness, float Bend, float Seed)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation),s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float field=0.0; [unroll]for(int i=0;i<12;i++){float h=frac(sin((i+Seed)*17.31)*43758.5453);float y=(i/11.0-0.5)*Width;float len=Length*lerp(0.55,1.0,h);float bend=(h-0.5)*Bend;float x=saturate((p.x+len)/(2.0*len));float center=y+bend*x*x;float fiber=(1.0-smoothstep(FiberWidth,FiberWidth+Softness,abs(p.y-center)))*step(-len,p.x)*step(p.x,len)*(1.0-x);field=max(field,fiber);} float radial=1.0-smoothstep(.75,1.0,length(p)*2.0); float a=field*radial*CyBrushFlow*saturate(CyBrushPosition.z); return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Length, Width, Rotation, FiberWidth, Softness, Bend, Seed + floor(b.seed * 997.0) * RandomSeed);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
