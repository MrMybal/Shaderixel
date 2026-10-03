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
#include "common.slang"

cbuffer Root : register(b0)
{
    int2 gRectOrigin;
    uint2 gRectSize;
    uint2 gDocSize;
    uint gDabCount;
    uint gDabBase;      // index of the first dab of this batch in the stroke
    uint gMode;         // 0 paint, 1 erase
    uint gFlags;        // 1 selection, 2 preview (no selection, no pre layer)
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
};

float Hash1(float x) { return frac(sin(x * 12.9898 + 78.233) * 43758.5453); }
// Random numbers derived from a dab seed: Rand(d.seed, 1), Rand(d.seed, 2)... differ.
float Rand(float seed, float k) { return Hash1(seed * 1013.0 + k * 57.13); }
float2 Rand2(float seed) { return float2(Rand(seed, 11.0), Rand(seed, 23.0)); }
float2 Rotate(float2 v, float a) { float c = cos(a), s = sin(a); return float2(c * v.x - s * v.y, s * v.x + c * v.y); }
