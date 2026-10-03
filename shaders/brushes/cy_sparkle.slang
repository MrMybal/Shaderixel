//@name Sparkle
//@category Particules
//@selfflow
//@spacing 0.3
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param CoreFalloff float 5 0.1 32 "Core Falloff"
//@param Rays float 8 2 32 "Rays"
//@param RaySharpness float 24 1 128 "Ray Sharpness"
//@param RayFalloff float 2 0.1 16 "Ray Falloff"
//@param RayIntensity float 0.65 0 8 "Ray Intensity"
//@param HaloRadius float 0.45 0.01 1.5 "Halo Radius"
//@param HaloWidth float 0.05 0.001 0.5 "Halo Width"
//@param HaloFalloff float 2 0.1 16 "Halo Falloff"
//@param HaloIntensity float 0.2 0 8 "Halo Intensity"
//@param Intensity float 2 0 32 "Intensity"
//@param FollowStroke float 0 0 1 "Suivre le trait"

// Imported preset (Brush / Particles): HDR-compatible sparkle stamp with star rays, halo, rotation, and falloff controls.
// Unreal Custom-node code kept as written (UV = tip space, CyBrushPosition.z = pressure).
float4 CyPreset(float2 UV, float CyBrushHardness, float CyBrushFlow, float4 CyBrushPosition, float3 CyBrushColor, float4 CyCanvasSize, float Rotation, float CoreFalloff, float Rays, float RaySharpness, float RayFalloff, float RayIntensity, float HaloRadius, float HaloWidth, float HaloFalloff, float HaloIntensity, float Intensity)
{
    float3 CyWriteChannels = float3(1.0, 1.0, 1.0);
    float2 p=UV-0.5; float c=cos(Rotation),s=sin(Rotation); p=float2(c*p.x-s*p.y,s*p.x+c*p.y); float d=length(p)*2.0; float core=pow(saturate(1.0-d),CoreFalloff); float ang=atan2(p.y,p.x); float rays=pow(abs(cos(ang*max(Rays,2.0)*0.5)),RaySharpness)*pow(saturate(1.0-d),RayFalloff); float halo=pow(saturate(1.0-abs(d-HaloRadius)/max(HaloWidth,0.0001)),HaloFalloff); float v=core+rays*RayIntensity+halo*HaloIntensity; float a=saturate(v*CyBrushFlow*saturate(CyBrushPosition.z)); return float4(CyBrushColor*Intensity,a);
}

float4 Tip(Brush b)
{
    return CyPreset(b.uv, b.hardness, b.flow, float4(b.px, b.pressure, 0.0), b.color, float4(b.docSize, 1.0 / b.docSize), Rotation, CoreFalloff, Rays, RaySharpness, RayFalloff, RayIntensity, HaloRadius, HaloWidth, HaloFalloff, HaloIntensity, Intensity);
}

void Dynamics(inout Dab d)
{
    d.angle = FollowStroke * atan2(d.direction.y, d.direction.x);
}
