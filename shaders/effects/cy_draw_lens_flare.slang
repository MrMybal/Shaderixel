//@name Lens Flare
//@category Dessin
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param CoreRadius float 0.06 0.001 1 "Core Radius"
//@param CoreFalloff float 2 0.1 16 "Core Falloff"
//@param CoreIntensity float 8 0 64 "Core Intensity"
//@param HaloRadius float 0.22 0.001 2 "Halo Radius"
//@param HaloWidth float 0.025 0.001 0.5 "Halo Width"
//@param HaloFalloff float 2 0.1 16 "Halo Falloff"
//@param HaloIntensity float 1.5 0 32 "Halo Intensity"
//@param RayCount float 8 1 64 "Ray Count"
//@param RaySharpness float 12 1 64 "Ray Sharpness"
//@param RayLength float 0.45 0.001 2 "Ray Length"
//@param RayIntensity float 1.5 0 32 "Ray Intensity"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param GhostSpacing float 0.5 0 3 "Ghost Spacing"
//@param GhostRadius float 0.08 0.001 1 "Ghost Radius"
//@param GhostIntensity float 1 0 32 "Ghost Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.38 0.42 0 1 "Center"
//@param Direction float2 0.28 0.08 0 1 "Direction"
//@param PrimaryColor color 1 0.65 0.25 1 "Primary Color"
//@param SecondaryColor color 0.18 0.45 1 1 "Secondary Color"

// Imported preset (Draw / Light & Space): HDR cinematic lens flare with core, halo, rays, and colored ghosts.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float CoreRadius, float CoreFalloff, float CoreIntensity, float HaloRadius, float HaloWidth, float HaloFalloff, float HaloIntensity, float RayCount, float RaySharpness, float RayLength, float RayIntensity, float Rotation, float GhostSpacing, float GhostRadius, float GhostIntensity, float Opacity, float3 Center, float3 Direction, float3 PrimaryColor, float3 SecondaryColor)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float core=pow(saturate(1.0-d/max(CoreRadius,0.0001)),max(CoreFalloff,0.1))*CoreIntensity; float halo=pow(saturate(1.0-abs(d-HaloRadius)/max(HaloWidth,0.0001)),max(HaloFalloff,0.1))*HaloIntensity; float angle=atan2(p.y,p.x)+Rotation; float rays=pow(abs(cos(angle*max(RayCount,1.0))),max(RaySharpness,1.0))*pow(saturate(1.0-d/max(RayLength,0.0001)),2.0)*RayIntensity; float2 axis=Direction.xy; float g1=pow(saturate(1.0-length(p-axis*GhostSpacing)/max(GhostRadius,0.0001)),2.0); float g2=pow(saturate(1.0-length(p+axis*GhostSpacing*1.7)/max(GhostRadius*0.7,0.0001)),2.0); float3 rgb=PrimaryColor.rgb*(core+halo+rays)+SecondaryColor.rgb*(g1+g2)*GhostIntensity; float a=saturate(max(max(rgb.r,rgb.g),rgb.b)*Opacity); return float4(rgb*Opacity,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, CoreRadius, CoreFalloff, CoreIntensity, HaloRadius, HaloWidth, HaloFalloff, HaloIntensity, RayCount, RaySharpness, RayLength, RayIntensity, Rotation, GhostSpacing, GhostRadius, GhostIntensity, Opacity, float3(Center, 0.0), float3(Direction, 0.0), PrimaryColor.rgb, SecondaryColor.rgb);
}
