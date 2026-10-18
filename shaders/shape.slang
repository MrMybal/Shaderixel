// Vector shape layer rendered from signed distance fields (antialiased fill + stroke).
// Types: 0 rectangle (rounded), 1 ellipse, 2 line, 3 regular polygon, 4 star, 5 custom outline (distance field texture).
// Extra components combine with the main shape in order (boolean operations on the
// distances): 0 unite (min), 1 subtract (max(a, -b)), 2 intersect (max), 3 exclude (xor).
#include "common.slang"

#define MAX_PARTS 8

cbuffer Shape : register(b1)
{
    float4 gRect;       // x0 y0 x1 y1
    float4 gFill;       // linear straight
    float4 gStroke;     // linear straight
    uint2 gSize;
    uint gType;
    uint gFlags;        // 1 fill, 2 stroke
    float gRadius;      // corner radius / star inner ratio
    float gStrokeWidth;
    float gRotation;    // radians
    int gSides;
    uint gPartCount;
    uint3 gPad;
    float4 gPartRect[MAX_PARTS];
    float4 gPartParam[MAX_PARTS];   // type, radius, sides, rotation (radians)
    float4 gPartOp[MAX_PARTS];      // x: operation
    float4 gSdfBox;                 // custom shape: area of the distance field, frame coordinates around the center
};

Texture2D<float> gSdf : register(t0);  // custom shape: signed distance (pixels)

RWTexture2D<float4> gOut : register(u0);

float SdBox(float2 p, float2 b, float r)
{
    r = min(r, min(b.x, b.y));
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float SdEllipse(float2 p, float2 ab)
{
    // Approximate distance: scaled circle distance corrected by the gradient length.
    ab = max(ab, 0.5);
    float k0 = length(p / ab);
    float k1 = length(p / (ab * ab));
    return k0 < 1e-5 ? -min(ab.x, ab.y) : k0 * (k0 - 1.0) / k1;
}

float SdSegment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a, ba = b - a;
    float h = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-6));
    return length(pa - ba * h);
}

float SdStar(float2 p, float r, int n, float inner)
{
    // Star as the polygon through alternating outer/inner vertices (exact segment distances).
    int m = clamp(n, 3, 64) * 2;
    float d = 1e9;
    bool inside = false;
    float2 prev = float2(0, -r);
    for (int i = 1; i <= m; ++i)
    {
        float ang = 6.2831853 * i / m;
        float rr = (i & 1) ? r * inner : r;
        float2 cur = float2(sin(ang), -cos(ang)) * rr;
        d = min(d, SdSegment(p, prev, cur));
        if (((prev.y > p.y) != (cur.y > p.y)) && (p.x < (cur.x - prev.x) * (p.y - prev.y) / (cur.y - prev.y) + prev.x)) inside = !inside;
        prev = cur;
    }
    return inside ? -d : d;
}

float ShapeSd(float2 p, uint type, float4 rect, float radius, int sides, float rotation)
{
    float2 c = (rect.xy + rect.zw) * 0.5;
    float2 ext = abs(rect.zw - rect.xy) * 0.5;
    float2 q = p - c;
    float cs = cos(-rotation), sn = sin(-rotation);
    q = float2(cs * q.x - sn * q.y, sn * q.x + cs * q.y);
    if (type == 0) return SdBox(q, ext, radius);
    if (type == 1) return SdEllipse(q, ext);
    if (type == 2) return SdSegment(p, rect.xy, rect.zw) - max(gStrokeWidth, 1.0) * 0.5;
    float2 s = q / max(ext, 1e-3) * min(ext.x, ext.y);
    if (type == 3) return SdStar(s, min(ext.x, ext.y), sides, cos(3.14159265 / max(sides, 3)));
    return SdStar(s, min(ext.x, ext.y), sides, saturate(radius));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float2 p = float2(id.xy) + 0.5;
    float d;
    if (gType == 5)
    {
        // Custom outline: the distance field around the frame center (rotated), extended outside its area.
        float2 c = (gRect.xy + gRect.zw) * 0.5;
        float2 q = p - c;
        float cs = cos(-gRotation), sn = sin(-gRotation);
        q = float2(cs * q.x - sn * q.y, sn * q.x + cs * q.y);
        float2 uv = (q - gSdfBox.xy) / max(gSdfBox.zw, 1e-3);
        float2 cuv = saturate(uv);
        d = gSdf.SampleLevel(sLinearClamp, cuv, 0) + length((uv - cuv) * gSdfBox.zw);
    }
    else d = ShapeSd(p, gType, gRect, gRadius, gSides, gRotation);
    for (uint i = 0; i < min(gPartCount, (uint)MAX_PARTS); ++i)
    {
        float4 pp = gPartParam[i];
        float b = ShapeSd(p, (uint)pp.x, gPartRect[i], pp.y, (int)pp.z, pp.w);
        uint op = (uint)gPartOp[i].x;
        if (op == 0) d = min(d, b);
        else if (op == 1) d = max(d, -b);
        else if (op == 2) d = max(d, b);
        else d = max(min(d, b), -max(d, b));
    }
    const bool line = gType == 2 && gPartCount == 0;
    float fill = (gFlags & 1) ? saturate(0.5 - d) : 0.0;
    if (line) fill = saturate(0.5 - d);  // a lone line is drawn with the stroke color
    float stroke = 0.0;
    if ((gFlags & 2) && !line) stroke = saturate(0.5 - (abs(d) - gStrokeWidth * 0.5));

    float4 f = line ? gStroke : gFill;
    float4 col = Premultiply(float4(f.rgb, f.a * fill));
    float4 st = Premultiply(float4(gStroke.rgb, gStroke.a * stroke));
    gOut[id.xy] = st + col * (1.0 - st.a);
}
