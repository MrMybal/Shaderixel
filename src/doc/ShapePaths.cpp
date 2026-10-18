#include "doc/ShapePaths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace sx {

std::vector<SubPath> ParseSvgPath(const std::string& d) {
    std::vector<SubPath> out;
    SubPath cur;
    float2 pos{}, start{}, lastCubic{}, lastQuad{};
    char cmd = 0, prev = 0;
    size_t i = 0;
    auto skip = [&] {
        while (i < d.size() && (std::isspace((unsigned char)d[i]) || d[i] == ',')) ++i;
    };
    auto number = [&](float& v) {
        skip();
        if (i >= d.size()) return false;
        const char* s = d.c_str() + i;
        char* e = nullptr;
        v = std::strtof(s, &e);
        if (e == s) return false;
        i += (size_t)(e - s);
        return true;
    };
    auto flush = [&] {
        if (cur.pts.size() >= 2) out.push_back(cur);
        cur = SubPath{};
    };
    auto begin = [&] {
        if (cur.pts.empty()) cur.pts.push_back({pos, pos, pos});
    };
    auto lineTo = [&](float2 p) {
        begin();
        cur.pts.push_back({p, p, p});
        pos = p;
    };
    auto cubicTo = [&](float2 c1, float2 c2, float2 p) {
        begin();
        cur.pts.back().out = c1;
        cur.pts.push_back({p, c2, p});
        pos = p;
        lastCubic = c2;
    };
    auto quadTo = [&](float2 q, float2 p) {
        cubicTo(pos + (q - pos) * (2.0f / 3.0f), p + (q - p) * (2.0f / 3.0f), p);
        lastQuad = q;
    };
    while (true) {
        skip();
        if (i >= d.size()) break;
        if (std::isalpha((unsigned char)d[i])) cmd = d[i++];
        else if (!cmd) break;
        const bool rel = std::islower((unsigned char)cmd) != 0;
        const char C = (char)std::toupper((unsigned char)cmd);
        const float2 base = rel ? pos : float2{0, 0};
        float a[7];
        auto read = [&](int n) {
            for (int k = 0; k < n; ++k)
                if (!number(a[k])) return false;
            return true;
        };
        bool ok = true;
        switch (C) {
            case 'M':
                if (!(ok = read(2))) break;
                flush();
                pos = base + float2{a[0], a[1]};
                start = pos;
                cur.pts.push_back({pos, pos, pos});
                cmd = rel ? 'l' : 'L';  // following pairs are lines
                break;
            case 'L':
                if ((ok = read(2))) lineTo(base + float2{a[0], a[1]});
                break;
            case 'H':
                if ((ok = read(1))) lineTo({rel ? pos.x + a[0] : a[0], pos.y});
                break;
            case 'V':
                if ((ok = read(1))) lineTo({pos.x, rel ? pos.y + a[0] : a[0]});
                break;
            case 'C':
                if ((ok = read(6))) cubicTo(base + float2{a[0], a[1]}, base + float2{a[2], a[3]}, base + float2{a[4], a[5]});
                break;
            case 'S':
                if ((ok = read(4))) {
                    const float2 c1 = (prev == 'C' || prev == 'S') ? pos * 2.0f - lastCubic : pos;
                    cubicTo(c1, base + float2{a[0], a[1]}, base + float2{a[2], a[3]});
                }
                break;
            case 'Q':
                if ((ok = read(4))) quadTo(base + float2{a[0], a[1]}, base + float2{a[2], a[3]});
                break;
            case 'T':
                if ((ok = read(2))) quadTo((prev == 'Q' || prev == 'T') ? pos * 2.0f - lastQuad : pos, base + float2{a[0], a[1]});
                break;
            case 'A':
                if ((ok = read(7))) lineTo(base + float2{a[5], a[6]});  // arcs: straight segments
                break;
            case 'Z':
                if (cur.pts.size() >= 2) {
                    // The closing point lands on the first one: merged.
                    const float2 dd = cur.pts.back().p - cur.pts.front().p;
                    if (std::abs(dd.x) < 1e-3f && std::abs(dd.y) < 1e-3f) {
                        cur.pts.front().in = cur.pts.back().in;
                        cur.pts.pop_back();
                    }
                    cur.closed = true;
                }
                flush();
                pos = start;
                cmd = 0;
                break;
            default: ok = false;
        }
        if (!ok) break;
        prev = C;
    }
    flush();
    return out;
}

std::vector<SubPath> ParseSvgFile(const std::string& svg) {
    std::vector<SubPath> all;
    for (size_t at = svg.find("<path"); at != std::string::npos; at = svg.find("<path", at + 5)) {
        const size_t end = svg.find('>', at);
        if (end == std::string::npos) break;
        const std::string tag = svg.substr(at, end - at);
        size_t d = tag.find(" d=");
        if (d == std::string::npos) d = tag.find("\td=");
        if (d == std::string::npos) d = tag.find("\nd=");
        if (d == std::string::npos || d + 4 >= tag.size()) continue;
        const char q = tag[d + 3];
        const size_t close = tag.find(q, d + 4);
        if (close == std::string::npos) continue;
        auto sub = ParseSvgPath(tag.substr(d + 4, close - d - 4));
        all.insert(all.end(), sub.begin(), sub.end());
    }
    // Polygons too.
    for (size_t at = svg.find("<polygon"); at != std::string::npos; at = svg.find("<polygon", at + 8)) {
        const size_t end = svg.find('>', at);
        const std::string tag = svg.substr(at, end == std::string::npos ? std::string::npos : end - at);
        const size_t p = tag.find("points=");
        if (p == std::string::npos || p + 8 >= tag.size()) continue;
        const char q = tag[p + 7];
        const size_t close = tag.find(q, p + 8);
        if (close == std::string::npos) continue;
        auto sub = ParseSvgPath("M " + tag.substr(p + 8, close - p - 8) + " Z");
        all.insert(all.end(), sub.begin(), sub.end());
    }
    return all;
}

void NormalizeShapePath(std::vector<SubPath>& path) {
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (auto& s : path)
        for (auto& a : s.pts)
            for (const float2& p : {a.p, a.in, a.out}) {
                x0 = std::min(x0, p.x);
                y0 = std::min(y0, p.y);
                x1 = std::max(x1, p.x);
                y1 = std::max(y1, p.y);
            }
    if (x1 <= x0 && y1 <= y0) return;
    const float sz = std::max(x1 - x0, y1 - y0);
    const float k = sz > 0 ? 1.0f / sz : 1.0f;
    const float2 off{(1.0f - (x1 - x0) * k) * 0.5f, (1.0f - (y1 - y0) * k) * 0.5f};
    auto map = [&](float2 p) { return float2{(p.x - x0) * k + off.x, (p.y - y0) * k + off.y}; };
    for (auto& s : path)
        for (auto& a : s.pts) {
            a.p = map(a.p);
            a.in = map(a.in);
            a.out = map(a.out);
        }
}

const std::vector<CustomShape>& BuiltinShapes() {
    static const std::vector<CustomShape> shapes = [] {
        struct Def {
            const char* name;
            const char* d;
        };
        static const Def defs[] = {
            {"Cœur", "M50 90 C22 68 4 50 4 30 C4 14 16 4 30 4 C40 4 47 10 50 17 C53 10 60 4 70 4 C84 4 96 14 96 30 C96 50 78 68 50 90 Z"},
            {"Flèche", "M4 38 L58 38 L58 14 L96 50 L58 86 L58 62 L4 62 Z"},
            {"Double flèche", "M4 50 L28 22 L28 38 L72 38 L72 22 L96 50 L72 78 L72 62 L28 62 L28 78 Z"},
            {"Bulle", "M12 8 L88 8 Q96 8 96 16 L96 58 Q96 66 88 66 L46 66 L24 90 L30 66 L12 66 Q4 66 4 58 L4 16 Q4 8 12 8 Z"},
            {"Bulle ronde", "M50 6 C76 6 96 21 96 40 C96 59 76 74 50 74 C45 74 40 73 35 72 L14 92 L20 66 C10 59 4 50 4 40 C4 21 24 6 50 6 Z"},
            {"Éclair", "M58 4 L18 56 L44 56 L34 96 L82 38 L56 38 L68 4 Z"},
            {"Coche", "M6 52 L22 36 L40 54 L78 14 L94 30 L40 86 Z"},
            {"Croix", "M20 4 L50 34 L80 4 L96 20 L66 50 L96 80 L80 96 L50 66 L20 96 L4 80 L34 50 L4 20 Z"},
            {"Plus", "M36 4 L64 4 L64 36 L96 36 L96 64 L64 64 L64 96 L36 96 L36 64 L4 64 L4 36 L36 36 Z"},
            {"Nuage", "M26 82 C10 82 4 70 8 60 C0 52 6 36 22 38 C22 20 40 12 52 22 C60 6 84 8 84 30 C98 32 100 52 90 60 C100 72 88 84 74 82 Z"},
            {"Goutte", "M50 4 C50 4 14 48 14 64 C14 84 30 96 50 96 C70 96 86 84 86 64 C86 48 50 4 50 4 Z"},
            {"Anneau", "M50 4 C76 4 96 24 96 50 C96 76 76 96 50 96 C24 96 4 76 4 50 C4 24 24 4 50 4 Z "
                       "M50 28 C38 28 28 38 28 50 C28 62 38 72 50 72 C62 72 72 62 72 50 C72 38 62 28 50 28 Z"},
            {"Lune", "M62 4 C34 8 16 28 16 52 C16 76 36 96 62 96 C70 96 78 94 84 90 C62 86 46 70 46 50 C46 30 62 14 84 10 C78 6 70 4 62 4 Z"},
            {"Bouclier", "M50 4 L92 18 C92 56 76 82 50 96 C24 82 8 56 8 18 Z"},
            {"Étiquette", "M4 50 L30 16 L96 16 L96 84 L30 84 Z M28 44 C24 44 22 47 22 50 C22 53 24 56 28 56 C32 56 34 53 34 50 C34 47 32 44 28 44 Z"},
            {"Pastille", "M50 2 L60 14 L75 8 L78 23 L94 26 L88 40 L98 52 L86 62 L92 77 L76 80 L72 95 L58 89 L48 99 L38 88 L24 94 L21 79 L6 75 "
                         "L12 61 L2 50 L13 39 L8 24 L23 21 L27 6 L41 12 Z"},
            {"Maison", "M50 6 L96 46 L84 46 L84 94 L60 94 L60 66 L40 66 L40 94 L16 94 L16 46 L4 46 Z"},
            {"Note de musique", "M38 74 C38 86 26 94 16 92 C6 90 4 80 12 74 C20 68 30 68 34 70 L34 10 L90 2 L90 64 C90 76 78 84 68 82 C58 80 56 70 64 64 "
                                "C72 58 82 58 86 60 L86 22 L38 28 Z"},
            {"Ruban", "M4 30 L22 30 L22 18 L78 18 L78 30 L96 30 L86 46 L96 62 L70 62 L70 74 L30 74 L30 62 L4 62 L14 46 Z"},
            {"Hexagone arrondi", "M40 6 Q50 0 60 6 L86 21 Q96 27 96 39 L96 61 Q96 73 86 79 L60 94 Q50 100 40 94 L14 79 Q4 73 4 61 L4 39 Q4 27 14 21 Z"},
        };
        std::vector<CustomShape> v;
        for (const Def& d : defs) {
            CustomShape s;
            s.name = d.name;
            s.path = ParseSvgPath(d.d);
            NormalizeShapePath(s.path);
            v.push_back(std::move(s));
        }
        return v;
    }();
    return shapes;
}

namespace {
// Squared distance transform of a 1D function (Felzenszwalb & Huttenlocher).
void Dt1(const float* f, int n, float* d, int* v, float* z) {
    const float inf = 1e20f;
    int k = 0;
    v[0] = 0;
    z[0] = -inf;
    z[1] = inf;
    for (int q = 1; q < n; ++q) {
        float s = ((f[q] + (float)q * q) - (f[v[k]] + (float)v[k] * v[k])) / (2.0f * q - 2.0f * v[k]);
        while (s <= z[k]) {
            --k;
            s = ((f[q] + (float)q * q) - (f[v[k]] + (float)v[k] * v[k])) / (2.0f * q - 2.0f * v[k]);
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = inf;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < q) ++k;
        d[q] = (float)(q - v[k]) * (q - v[k]) + f[v[k]];
    }
}

// Squared distance to the nearest site (site: 0, elsewhere: large).
void Dt2(std::vector<float>& g, int w, int h) {
    const int n = std::max(w, h);
    std::vector<float> f(n), d(n), z(n + 1);
    std::vector<int> v(n);
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) f[y] = g[(size_t)y * w + x];
        Dt1(f.data(), h, d.data(), v.data(), z.data());
        for (int y = 0; y < h; ++y) g[(size_t)y * w + x] = d[y];
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) f[x] = g[(size_t)y * w + x];
        Dt1(f.data(), w, d.data(), v.data(), z.data());
        for (int x = 0; x < w; ++x) g[(size_t)y * w + x] = d[x];
    }
}
}  // namespace

void ShapeDistanceField(const std::vector<SubPath>& path, float boxW, float boxH, float margin, int maxSide, std::vector<float>& out, int& tw,
                        int& th, float& scale) {
    const float lx = boxW + margin * 2, ly = boxH + margin * 2;
    scale = std::min(1.0f, (float)maxSide / std::max(1.0f, std::max(lx, ly)));
    tw = std::max(4, (int)std::ceil(lx * scale));
    th = std::max(4, (int)std::ceil(ly * scale));
    // The outline in texels.
    VectorPath vp;
    vp.subpaths = path;
    for (auto& s : vp.subpaths)
        for (auto& a : s.pts)
            for (float2* p : {&a.p, &a.in, &a.out}) *p = {(p->x * boxW + margin) * scale, (p->y * boxH + margin) * scale};
    auto contours = FlattenPath(vp, 0.2f);
    Rect r;
    std::vector<u8> cov;
    RasterizeContours(contours, tw, th, false, r, cov);
    std::vector<u8> inside((size_t)tw * th, 0);
    for (int y = std::max(0, r.y0); y < std::min(th, r.y1); ++y)
        for (int x = std::max(0, r.x0); x < std::min(tw, r.x1); ++x)
            inside[(size_t)y * tw + x] = cov[(size_t)(y - r.y0) * r.Width() + (x - r.x0)] >= 128;
    const float big = 1e20f;
    std::vector<float> toIn((size_t)tw * th), toOut((size_t)tw * th);
    for (size_t i = 0; i < inside.size(); ++i) {
        toIn[i] = inside[i] ? 0.0f : big;
        toOut[i] = inside[i] ? big : 0.0f;
    }
    Dt2(toIn, tw, th);
    Dt2(toOut, tw, th);
    out.resize(inside.size());
    for (size_t i = 0; i < inside.size(); ++i) {
        const float dTexels = inside[i] ? -(std::sqrt(toOut[i]) - 0.5f) : (std::sqrt(toIn[i]) - 0.5f);
        out[i] = dTexels / scale;
    }
}

}  // namespace sx
