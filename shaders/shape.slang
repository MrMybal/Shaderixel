// Vector shape layer rendered from signed distance fields (antialiased fill + stroke).
// Types: 0 rectangle (rounded), 1 ellipse, 2 line, 3 regular polygon, 4 star.
#include "common.slang"

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
};

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

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gSize)) return;
    float2 p = float2(id.xy) + 0.5;
    float2 c = (gRect.xy + gRect.zw) * 0.5;
    float2 ext = abs(gRect.zw - gRect.xy) * 0.5;
    float2 q = p - c;
    float cs = cos(-gRotation), sn = sin(-gRotation);
    q = float2(cs * q.x - sn * q.y, sn * q.x + cs * q.y);

    float d;
    if (gType == 0) d = SdBox(q, ext, gRadius);
    else if (gType == 1) d = SdEllipse(q, ext);
    else if (gType == 2) d = SdSegment(p, gRect.xy, gRect.zw) - max(gStrokeWidth, 1.0) * 0.5;
    else if (gType == 3)
    {
        float2 s = q / max(ext, 1e-3) * min(ext.x, ext.y);
        d = SdStar(s, min(ext.x, ext.y), gSides, cos(3.14159265 / max(gSides, 3)));
    }
    else
    {
        float2 s = q / max(ext, 1e-3) * min(ext.x, ext.y);
        d = SdStar(s, min(ext.x, ext.y), gSides, saturate(gRadius));
    }

    float fill = (gFlags & 1) ? saturate(0.5 - d) : 0.0;
    if (gType == 2) fill = saturate(0.5 - d);  // a line is drawn with the stroke color
    float stroke = 0.0;
    if ((gFlags & 2) && gType != 2) stroke = saturate(0.5 - (abs(d) - gStrokeWidth * 0.5));

    float4 f = gType == 2 ? gStroke : gFill;
    float4 col = Premultiply(float4(f.rgb, f.a * fill));
    float4 st = Premultiply(float4(gStroke.rgb, gStroke.a * stroke));
    gOut[id.xy] = st + col * (1.0 - st.a);
}
