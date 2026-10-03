//@name Chain
//@category CyTexture Détails
//@selfflow
//@spacing 0.45
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Links float 5 1 24 "Links"
//@param LinkLength float 0.42 0.05 1 "Link Length"
//@param LinkWidth float 0.18 0.02 0.7 "Link Width"
//@param Thickness float 0.08 0.005 0.5 "Thickness"
//@param Softness float 0.015 0.0001 0.2 "Softness"
//@param Perspective float 2.2 0.2 8 "Perspective"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported from CyTexture (Brush / Surface Detail): Interlocking chain-link stamp for trim, ornaments, cables, and mechanical masks.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Rotation, float Links, float LinkLength, float LinkWidth, float Thickness, float Softness, float Perspective)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-.5;float c=cos(Rotation),s=sin(Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float u=(p.x+.5)*max(Links,1.0);float id=floor(u);float2 q=float2(frac(u)-.5,p.y);float alternate=fmod(id,2.0);q.y*=lerp(1.0,Perspective,alternate);float ellipse=abs(length(float2(q.x/max(LinkLength,.001),q.y/max(LinkWidth,.001)))-1.0);float link=1.0-smoothstep(Thickness,Thickness+Softness,ellipse);float a=link*step(abs(p.x),.5)*CyBrushFlow*saturate(CyBrushPosition.z);return float4(CyBrushColor,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Rotation, Links, LinkLength, LinkWidth, Thickness, Softness, Perspective);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
