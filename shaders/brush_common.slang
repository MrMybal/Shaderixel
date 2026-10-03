// Programmable brushes (shaders/brushes/*.slang): prelude.
//
// A brush file defines
//   float4 Tip(Brush b)            color (rgb, straight) and coverage (a) of a dab pixel
//   void Dynamics(inout Dab d)     optional: how each dab is placed (angle, offset, size, opacity, color)
// plus //@param parameters (same syntax as effects) and brush settings:
//   //@spacing 0.1     default spacing (fraction of the diameter)
//   //@airbrush 30     dabs per second while the button is held (time-based painting)
//   //@extent 2        dabs may reach this many radii from the stroke (scatter / scale)
//   //@selfflow        Tip() applies flow and pressure itself (CyTexture brushes)
//   //@texture Name "Label" textures/file.png   an image of the brush (up to 4), read in
//                      Tip() with BrushTex(Name, uv) or Texture2DSample(Name, S, uv);
//                      the user can pick another image in the brush settings
#include "common.slang"

cbuffer Root : register(b0)
{
    int2 gRectOrigin;
    uint2 gRectSize;
    uint2 gDocSize;
    uint gDabCount;
    uint gDabBase;      // index of the first dab of this batch in the stroke
    uint gMode;         // 0 paint, 1 erase
    uint gFlags;        // 1 selection, 2 preview (no selection, no pre layer), 4 << i: brush texture i bound
};
// The brush color and settings come first in the BrushData cbuffer (generated).
#define gOpacity gBrushSettings.x
#define gHardness gBrushSettings.y
#define gFlow gBrushSettings.z

Texture2D<float4> gPre : register(t0);           // layer before the stroke
Texture2D<float> gSel : register(t1);            // selection (optional)
RWTexture2D<float4> gLayer : register(u0);
RWTexture2D<float> gStroke : register(u1);       // stroke coverage
RWTexture2D<float4> gStrokeColor : register(u2); // stroke color, premultiplied by coverage
Texture2D<float4> gBrushTex0 : register(t2);     // brush textures (//@texture)
Texture2D<float4> gBrushTex1 : register(t3);
Texture2D<float4> gBrushTex2 : register(t4);
Texture2D<float4> gBrushTex3 : register(t5);

float4 SXBrushTexLoad(Texture2D<float4> t, float2 uv)
{
    // Bilinear, wrapped; sRGB images -> linear color (like a texture sampled in Unreal).
    uint w, h;
    t.GetDimensions(w, h);
    float2 p = uv * float2(w, h) - 0.5;
    int2 i = int2(floor(p));
    float2 f = p - float2(i);
    int2 s = int2(w, h);
    float4 a = t.Load(int3((i % s + s) % s, 0)), b = t.Load(int3(((i + int2(1, 0)) % s + s) % s, 0));
    float4 c = t.Load(int3(((i + int2(0, 1)) % s + s) % s, 0)), d = t.Load(int3(((i + int2(1, 1)) % s + s) % s, 0));
    float4 r = lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
    r.rgb = pow(max(r.rgb, 0.0), 2.2);
    return r;
}

// Brush texture i at uv (0..1, wrapped); white when no image is set.
float4 BrushTex(uint i, float2 uv)
{
    if ((gFlags & (4u << i)) == 0) return float4(1.0, 1.0, 1.0, 1.0);
    switch (i)
    {
    case 0: return SXBrushTexLoad(gBrushTex0, uv);
    case 1: return SXBrushTexLoad(gBrushTex1, uv);
    case 2: return SXBrushTexLoad(gBrushTex2, uv);
    default: return SXBrushTexLoad(gBrushTex3, uv);
    }
}
#define Texture2DSample(T, S, UVS) BrushTex(T, UVS)
#define Texture2DSampleLevel(T, S, UVS, LOD) BrushTex(T, UVS)

// What Tip() sees for one pixel of one dab.
struct Brush
{
    float2 uv;          // 0..1 inside the dab square (rotated with the dab)
    float2 local;       // -1..1, same frame
    float2 px;          // document pixel
    float2 docSize;
    float pressure;     // 0..1
    float3 color;       // brush color (linear)
    float hardness;     // 0..1
    float flow;         // brush flow
    float radius;       // dab radius in pixels
    float angle;        // dab rotation (radians)
    float seed;         // random value of the dab, 0..1
    uint index;         // dab number in the stroke
    float time;         // seconds since the stroke started
    float distance;     // pixels painted since the stroke started
    float2 direction;   // stroke direction (unit)
    float2 tilt;        // pen tilt in degrees (x, y), -90..90, 0 = upright (0 with a mouse)
    float twist;        // pen barrel rotation, radians (0 with a mouse)
    float speed;        // stroke speed, document pixels per second
};

// One dab, as Dynamics() may change it.
struct Dab
{
    float2 pos;         // center, document pixels
    float radius;
    float angle;        // radians
    float opacity;      // flow x pressure (from the brush settings)
    float3 color;
    float pressure;
    float2 direction;   // stroke direction (unit)
    float time;
    float distance;
    float seed;         // 0..1
    uint index;
    float2 tilt;        // pen tilt (degrees)
    float twist;        // pen rotation (radians)
    float speed;        // pixels / second
};

// Pen helpers for Dynamics(): tilt amount 0..1 and tilt direction (radians).
float TiltAmount(Dab d) { return saturate(length(d.tilt) / 60.0); }
float TiltAngle(Dab d) { return atan2(d.tilt.y, d.tilt.x); }

float Hash1(float x) { return frac(sin(x * 12.9898 + 78.233) * 43758.5453); }
// Random numbers derived from a dab seed: Rand(d.seed, 1), Rand(d.seed, 2)... differ.
float Rand(float seed, float k) { return Hash1(seed * 1013.0 + k * 57.13); }
float2 Rand2(float seed) { return float2(Rand(seed, 11.0), Rand(seed, 23.0)); }
float2 Rotate(float2 v, float a) { float c = cos(a), s = sin(a); return float2(c * v.x - s * v.y, s * v.x + c * v.y); }
