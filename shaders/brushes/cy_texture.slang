//@name Texture
//@category Formes
//@selfflow
//@spacing 0.35
//@texture BrushTexture "Brush Texture" textures/splatter.png
//@param UseLuminance float 0 0 1 "Use Luminance"
//@param UseTextureColor float 0 0 1 "Use Texture Color"
//@param MaskGamma float 1 0.01 8 "Mask Gamma"
//@param RadialFalloff float 0 0 1 "Radial Falloff"
//@param RandomAngle float 1 0 1 "Rotation aléatoire"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported preset (Brush / Shape Tips): Texture-driven brush mask. Optionally preserves the texture's own RGB color.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float UseLuminance, float UseTextureColor, float MaskGamma, float RadialFalloff)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float4 t=Texture2DSample(BrushTexture,BrushTextureSampler,UV); float mask=lerp(t.a,dot(t.rgb,float3(0.2126,0.7152,0.0722)),saturate(UseLuminance)); mask=pow(saturate(mask),max(MaskGamma,0.01)); float radial=lerp(1.0,1.0-smoothstep(saturate(CyBrushHardness),1.0,length(UV-0.5)*2.0),saturate(RadialFalloff)); float3 rgb=lerp(CyBrushColor,t.rgb,saturate(UseTextureColor)); float a=mask*radial*CyBrushFlow*saturate(CyBrushPosition.z); return float4(rgb,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), UseLuminance, UseTextureColor, MaskGamma, RadialFalloff);
}

void Dynamics(inout Dab d)
{
    d.angle = Rand(d.seed, 4.0) * 6.2831853 * RandomAngle + FollowStroke * atan2(d.direction.y, d.direction.x);
}
