#include "io/TextRaster.h"

#define STB_TRUETYPE_IMPLEMENTATION
#pragma warning(push, 0)
#include <stb_truetype.h>
#pragma warning(pop)

#include <windows.h>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>

namespace fs = std::filesystem;

namespace sx {

namespace {
struct LoadedFont {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    bool ok = false;
};

std::mutex g_fontMutex;
std::map<std::string, std::unique_ptr<LoadedFont>> g_fonts;

LoadedFont* GetFont(const std::string& path) {
    std::lock_guard lock(g_fontMutex);
    auto it = g_fonts.find(path);
    if (it != g_fonts.end()) return it->second->ok ? it->second.get() : nullptr;
    auto f = std::make_unique<LoadedFont>();
    std::ifstream in(Utf8Path(path), std::ios::binary);
    if (in) {
        f->data.assign((std::istreambuf_iterator<char>(in)), {});
        // stb_truetype does no bounds checks: reject empty / truncated files before it reads them
        // (header and collection index, then the offset table and its records inside the data).
        const auto& d = f->data;
        const size_t n = d.size();
        auto be16 = [&](size_t o) { return (u32)(d[o] << 8 | d[o + 1]); };
        auto be32 = [&](size_t o) { return (u32)d[o] << 24 | (u32)d[o + 1] << 16 | (u32)d[o + 2] << 8 | (u32)d[o + 3]; };
        int offset = n >= 16 ? stbtt_GetFontOffsetForIndex(d.data(), 0) : -1;
        bool valid = offset >= 0 && (size_t)offset + 12 <= n;
        if (valid) {
            const size_t tables = be16((size_t)offset + 4);
            valid = tables > 0 && (size_t)offset + 12 + 16 * tables <= n;
            for (size_t k = 0; valid && k < tables; ++k) {
                const size_t rec = (size_t)offset + 12 + 16 * k;
                valid = (u64)be32(rec + 8) + be32(rec + 12) <= n;
            }
        }
        f->ok = valid && stbtt_InitFont(&f->info, f->data.data(), offset);
    }
    LoadedFont* p = f->ok ? f.get() : nullptr;
    g_fonts[path] = std::move(f);
    return p;
}

std::vector<u32> DecodeUtf8(const std::string& s) {
    std::vector<u32> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        u32 cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
        else { cp = c & 0x07; n = 4; }
        for (int k = 1; k < n && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
        out.push_back(cp);
        i += n;
    }
    return out;
}
}  // namespace

int Utf8Length(const std::string& s) { return (int)DecodeUtf8(s).size(); }

int Utf8CodepointIndex(const std::string& s, int byteOffset) {
    int n = 0;
    for (int i = 0; i < (int)s.size() && i < byteOffset; ++i)
        if (((unsigned char)s[i] & 0xC0) != 0x80) ++n;
    return n;
}

u64 CharStyle::Hash() const {
    Hasher h;
    h.Pod(id);
    h.Str(font);
    h.Pod(size);
    h.Pod(color);
    h.Pod(useColor);
    h.Pod(tracking);
    h.Pod(faux);
    h.Pod(baseline);
    h.Pod(underline);
    h.Pod(allCaps);
    return h.h;
}

u64 ParaStyle::Hash() const {
    Hasher h;
    h.Pod(id);
    h.Pod(align);
    h.Pod(lineSpacing);
    h.Pod(spaceBefore);
    h.Pod(indentLeft);
    h.Pod(indentFirst);
    h.Pod(charStyle);
    return h.h;
}

const CharStyle* TextStyleTable::Char(Id id) const {
    if (!chars || !id) return nullptr;
    for (auto& c : *chars) if (c.id == id) return &c;
    return nullptr;
}

const ParaStyle* TextStyleTable::Para(Id id) const {
    if (!paras || !id) return nullptr;
    for (auto& p : *paras) if (p.id == id) return &p;
    return nullptr;
}

u64 TextStyleTable::Hash() const {
    Hasher h;
    if (chars) for (auto& c : *chars) h.Pod(c.Hash());
    if (paras) for (auto& p : *paras) h.Pod(p.Hash());
    return h.h;
}

u64 TextStyle::Hash() const {
    Hasher h;
    h.Str(content);
    h.Str(font);
    h.Pod(size);
    h.Pod(color);
    h.Pod(pos);
    h.Pod(align);
    h.Pod(lineSpacing);
    h.Pod(tracking);
    h.Pod(faux);
    for (auto& r : runs) { h.Pod(r.start); h.Pod(r.end); h.Pod(r.style); }
    for (Id p : paraStyles) h.Pod(p);
    for (auto& p : path) h.Pod(p);
    h.Pod(pathOffset);
    h.Pod(pathFlip);
    return h.h;
}

const std::vector<FontEntry>& SystemFonts() {
    static std::vector<FontEntry> fonts = [] {
        std::vector<FontEntry> out;
        wchar_t* windir = nullptr;
        size_t len = 0;
        _wdupenv_s(&windir, &len, L"WINDIR");
        std::error_code ec;
        fs::path dir = fs::path(windir ? windir : L"C:\\Windows") / "Fonts";
        free(windir);
        for (auto& e : fs::directory_iterator(dir, ec)) {
            auto ext = e.path().extension().wstring();
            for (auto& c : ext) c = (wchar_t)towlower(c);
            if (ext != L".ttf" && ext != L".otf") continue;
            out.push_back({Narrow(e.path().stem().wstring()), Narrow(e.path().wstring())});
        }
        std::sort(out.begin(), out.end(), [](const FontEntry& a, const FontEntry& b) { return a.name < b.name; });
        return out;
    }();
    return fonts;
}

std::string DefaultFontPath() {
    for (auto& f : SystemFonts()) if (f.name == "segoeui") return f.path;
    for (auto& f : SystemFonts()) if (f.name == "arial") return f.path;
    return SystemFonts().empty() ? std::string() : SystemFonts().front().path;
}

namespace {

// Resolved attributes of one character.
struct Attr {
    LoadedFont* font = nullptr;
    float size = 72, scale = 1, tracking = 0, baseline = 0;
    float4 color;
    bool faux = false, underline = false;
};

struct Glyph {
    u32 cp;
    Attr a;
    float x = 0, y = 0;     // pen position (baseline, document pixels)
    float adv = 0;          // advance (with tracking / kerning)
    float angle = 0;        // text on path: rotation of the glyph around its baseline center
    float cx = 0, cy = 0;   // text on path: baseline center
    bool onPath = false, hidden = false;
};

// Polyline with cumulative lengths, sampled by arc length.
struct PathSampler {
    std::vector<float2> p;
    std::vector<float> s;
    explicit PathSampler(const std::vector<float2>& pts) : p(pts) {
        s.resize(p.size(), 0.0f);
        for (size_t i = 1; i < p.size(); ++i) s[i] = s[i - 1] + Length(p[i] - p[i - 1]);
    }
    float Total() const { return s.empty() ? 0 : s.back(); }
    bool At(float d, float2& pos, float2& tan) const {
        if (p.size() < 2 || d < 0 || d > Total()) return false;
        size_t i = std::upper_bound(s.begin(), s.end(), d) - s.begin();
        i = std::clamp<size_t>(i, 1, p.size() - 1);
        float seg = std::max(s[i] - s[i - 1], 1e-6f);
        float t = (d - s[i - 1]) / seg;
        pos = p[i - 1] + (p[i] - p[i - 1]) * t;
        // Smoothed tangent: chord over a small window.
        float2 a, b, ta, tb;
        float w = 3.0f;
        auto raw = [&](float dd, float2& q) {
            dd = std::clamp(dd, 0.0f, Total());
            size_t j = std::clamp<size_t>(std::upper_bound(s.begin(), s.end(), dd) - s.begin(), 1, p.size() - 1);
            float sg = std::max(s[j] - s[j - 1], 1e-6f);
            q = p[j - 1] + (p[j] - p[j - 1]) * ((dd - s[j - 1]) / sg);
        };
        raw(d - w, a);
        raw(d + w, b);
        (void)ta;
        (void)tb;
        float2 dv = b - a;
        float l = Length(dv);
        tan = l > 1e-6f ? dv * (1.0f / l) : float2{1, 0};
        return true;
    }
};

}  // namespace

bool RasterizeText(const TextStyle& t, int docW, int docH, std::vector<float>& rgba, Rect& rect, const TextStyleTable* styles) {
    LoadedFont* baseFont = GetFont(t.font.empty() ? DefaultFontPath() : t.font);
    if (!baseFont && !t.font.empty()) baseFont = GetFont(DefaultFontPath());  // font file missing / unreadable
    if (!baseFont) return false;
    TextStyleTable empty;
    const TextStyleTable& tab = styles ? *styles : empty;

    // Paragraphs of codepoints (global indices kept for the runs).
    std::vector<u32> cps = DecodeUtf8(t.content);
    struct Para { int start, end; };
    std::vector<Para> paras(1, {0, 0});
    for (int i = 0; i < (int)cps.size(); ++i) {
        if (cps[i] == '\n') {
            paras.back().end = i;
            paras.push_back({i + 1, i + 1});
        }
    }
    paras.back().end = (int)cps.size();

    auto resolve = [&](int idx, const ParaStyle* ps) {
        Attr a;
        std::string font = t.font;
        a.size = t.size;
        a.color = t.color;
        a.tracking = t.tracking;
        a.faux = t.faux;
        auto applyChar = [&](const CharStyle* c) {
            if (!c) return;
            if (!c->font.empty()) font = c->font;
            if (c->size > 0) a.size = c->size;
            if (c->useColor) a.color = c->color;
            a.tracking = c->tracking;
            a.faux = c->faux;
            a.baseline = c->baseline;
            a.underline = c->underline;
        };
        if (ps) applyChar(tab.Char(ps->charStyle));
        for (auto& r : t.runs)
            if (idx >= r.start && idx < r.end) applyChar(tab.Char(r.style));
        a.font = font.empty() ? baseFont : GetFont(font);
        if (!a.font) a.font = baseFont;
        a.scale = stbtt_ScaleForMappingEmToPixels(&a.font->info, std::max(a.size, 1.0f));
        return a;
    };
    auto caps = [&](int idx, const ParaStyle* ps, u32 cp) {
        bool up = false;
        if (ps) if (auto* c = tab.Char(ps->charStyle)) up = c->allCaps;
        for (auto& r : t.runs)
            if (idx >= r.start && idx < r.end) if (auto* c = tab.Char(r.style)) up = c->allCaps;
        if (!up || cp > 0xFFFF) return cp;
        // Locale independent (accents included).
        wchar_t wc = (wchar_t)cp;
        CharUpperBuffW(&wc, 1);
        return (u32)wc;
    };

    std::vector<Glyph> glyphs;
    float y = t.pos.y;
    const bool onPath = t.path.size() >= 2;
    for (size_t pi = 0; pi < paras.size(); ++pi) {
        const ParaStyle* ps = pi < t.paraStyles.size() ? tab.Para(t.paraStyles[pi]) : nullptr;
        int align = ps ? ps->align : t.align;
        float spacing = ps ? ps->lineSpacing : t.lineSpacing;
        if (ps && pi > 0) y += ps->spaceBefore;
        // One line per paragraph (explicit line breaks only): measure, then place.
        std::vector<Glyph> line;
        float width = 0, maxAsc = 0, maxH = 0;
        for (int i = paras[pi].start; i < paras[pi].end; ++i) {
            Glyph g;
            g.a = resolve(i, ps);
            g.cp = caps(i, ps, cps[i]);
            int asc, desc, gap, adv, lsb;
            stbtt_GetFontVMetrics(&g.a.font->info, &asc, &desc, &gap);
            maxAsc = std::max(maxAsc, asc * g.a.scale);
            maxH = std::max(maxH, (asc - desc) * g.a.scale);
            stbtt_GetCodepointHMetrics(&g.a.font->info, g.cp, &adv, &lsb);
            g.adv = adv * g.a.scale + g.a.tracking / 1000.0f * g.a.size;
            if (i + 1 < paras[pi].end && !t.runs.size())
                g.adv += stbtt_GetCodepointKernAdvance(&g.a.font->info, g.cp, cps[i + 1]) * g.a.scale;
            width += g.adv;
            line.push_back(g);
        }
        if (line.empty()) {
            int asc, desc, gap;
            stbtt_GetFontVMetrics(&baseFont->info, &asc, &desc, &gap);
            float sc = stbtt_ScaleForMappingEmToPixels(&baseFont->info, std::max(t.size, 1.0f));
            maxAsc = asc * sc;
            maxH = (asc - desc) * sc;
        }
        float indent = ps ? ps->indentLeft + ps->indentFirst : 0.0f;
        float x = t.pos.x + indent - (align == 1 ? width * 0.5f : align == 2 ? width : 0.0f);
        float baseline = y + maxAsc;
        for (auto& g : line) {
            g.x = x;
            g.y = baseline - g.a.baseline;
            x += g.adv;
            glyphs.push_back(g);
        }
        if (onPath && pi + 1 < paras.size()) {
            // On a path, paragraphs continue on the same baseline, separated by a space.
            Glyph sp;
            sp.a = resolve(paras[pi].end, ps);
            sp.cp = ' ';
            int adv, lsb;
            stbtt_GetCodepointHMetrics(&sp.a.font->info, ' ', &adv, &lsb);
            sp.adv = adv * sp.a.scale;
            sp.hidden = true;
            glyphs.push_back(sp);
        }
        y += maxH * spacing;
    }

    if (onPath) {
        // Lay the glyphs along the path: their baseline centers at successive arc lengths.
        std::vector<float2> pts = t.path;
        if (t.pathFlip) std::reverse(pts.begin(), pts.end());
        PathSampler ps(pts);
        float total = 0;
        for (auto& g : glyphs) total += g.adv;
        float s = t.pathOffset - (t.align == 1 ? total * 0.5f : t.align == 2 ? total : 0.0f);
        for (auto& g : glyphs) {
            float2 pos, tan;
            float mid = s + g.adv * 0.5f;
            if (!ps.At(mid, pos, tan)) g.hidden = true;
            g.onPath = true;
            g.angle = std::atan2(tan.y, tan.x);
            float2 nrm = {tan.y, -tan.x};  // left of the direction (up for left-to-right)
            g.cx = pos.x + nrm.x * g.a.baseline;
            g.cy = pos.y + nrm.y * g.a.baseline;
            s += g.adv;
        }
    }

    // Bounds.
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    auto grow = [&](float x, float yy) { minX = std::min(minX, x); minY = std::min(minY, yy); maxX = std::max(maxX, x); maxY = std::max(maxY, yy); };
    for (auto& g : glyphs) {
        if (g.hidden) continue;
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&g.a.font->info, g.cp, g.a.scale, g.a.scale, &x0, &y0, &x1, &y1);
        if (x1 <= x0 || y1 <= y0) continue;
        if (g.onPath) {
            float r = std::max({std::abs((float)x0 - g.adv * 0.5f), std::abs((float)x1 - g.adv * 0.5f), std::abs((float)y0), std::abs((float)y1)}) + 2;
            grow(g.cx - r, g.cy - r);
            grow(g.cx + r, g.cy + r);
        } else {
            grow(std::floor(g.x) + x0, std::floor(g.y) + y0);
            grow(std::floor(g.x) + x1 + 1, std::floor(g.y) + y1);
            if (g.a.underline) grow(g.x + g.adv, g.y + g.a.size * 0.15f);
        }
    }
    rect = Rect{(int)minX - 2, (int)minY - 2, (int)maxX + 2, (int)maxY + 2}.Clipped(docW, docH);
    rgba.clear();
    if (glyphs.empty() || rect.Empty() || minX > maxX) return true;
    const int w = rect.Width(), h = rect.Height();
    // Premultiplied linear accumulation (over), straight at the end.
    std::vector<float> acc((size_t)w * h * 4, 0.0f);
    auto over = [&](int tx, int ty, float v, const float4& lin) {
        if (tx < 0 || ty < 0 || tx >= w || ty >= h || v <= 0) return;
        float* d = &acc[((size_t)ty * w + tx) * 4];
        float a = std::min(v, 1.0f) * lin.w;
        d[0] = lin.x * a + d[0] * (1 - a);
        d[1] = lin.y * a + d[1] * (1 - a);
        d[2] = lin.z * a + d[2] * (1 - a);
        d[3] = a + d[3] * (1 - a);
    };
    std::vector<unsigned char> bmp;
    for (auto& g : glyphs) {
        if (g.hidden) continue;
        const float4 lin = {SrgbToLinear(g.a.color.x), SrgbToLinear(g.a.color.y), SrgbToLinear(g.a.color.z), g.a.color.w};
        const stbtt_fontinfo& info = g.a.font->info;
        int x0, y0, x1, y1;
        float fx = g.onPath ? 0.0f : g.x - std::floor(g.x);
        stbtt_GetCodepointBitmapBoxSubpixel(&info, g.cp, g.a.scale, g.a.scale, fx, 0, &x0, &y0, &x1, &y1);
        int bw = x1 - x0, bh = y1 - y0;
        if (bw > 0 && bh > 0) {
            bmp.assign((size_t)bw * bh, 0);
            stbtt_MakeCodepointBitmapSubpixel(&info, bmp.data(), bw, bh, bw, g.a.scale, g.a.scale, fx, 0, g.cp);
            if (g.a.faux) {
                std::vector<unsigned char> d(bmp);
                for (int yy = 0; yy < bh; ++yy)
                    for (int xx = 0; xx < bw; ++xx) {
                        int m = bmp[(size_t)yy * bw + xx];
                        if (xx > 0) m = std::max(m, (int)(bmp[(size_t)yy * bw + xx - 1] * 0.8f));
                        if (yy > 0) m = std::max(m, (int)(bmp[(size_t)(yy - 1) * bw + xx] * 0.8f));
                        d[(size_t)yy * bw + xx] = (unsigned char)m;
                    }
                bmp.swap(d);
            }
            if (!g.onPath) {
                int ox = (int)std::floor(g.x) + x0 - rect.x0, oy = (int)std::floor(g.y) + y0 - rect.y0;
                for (int yy = 0; yy < bh; ++yy)
                    for (int xx = 0; xx < bw; ++xx) over(ox + xx, oy + yy, bmp[(size_t)yy * bw + xx] / 255.0f, lin);
            } else {
                // Rotated: inverse map each target pixel into the glyph bitmap (bilinear).
                float c = std::cos(g.angle), s = std::sin(g.angle);
                float r = std::max({std::abs((float)x0 - g.adv * 0.5f), std::abs((float)x1 - g.adv * 0.5f), std::abs((float)y0), std::abs((float)y1)}) + 2;
                int tx0 = (int)std::floor(g.cx - r) - rect.x0, ty0 = (int)std::floor(g.cy - r) - rect.y0;
                int tx1 = (int)std::ceil(g.cx + r) - rect.x0, ty1 = (int)std::ceil(g.cy + r) - rect.y0;
                for (int ty = std::max(ty0, 0); ty <= std::min(ty1, h - 1); ++ty)
                    for (int tx = std::max(tx0, 0); tx <= std::min(tx1, w - 1); ++tx) {
                        float dx = tx + rect.x0 + 0.5f - g.cx, dy = ty + rect.y0 + 0.5f - g.cy;
                        // Glyph space: x along the baseline from the pen, y down from the baseline.
                        float lx = c * dx + s * dy + g.adv * 0.5f, ly = -s * dx + c * dy;
                        float bx = lx - x0 - 0.5f, by = ly - y0 - 0.5f;
                        int ix = (int)std::floor(bx), iy = (int)std::floor(by);
                        if (ix < -1 || iy < -1 || ix >= bw || iy >= bh) continue;
                        float ax = bx - ix, ay = by - iy;
                        auto B = [&](int xx, int yy) { return xx < 0 || yy < 0 || xx >= bw || yy >= bh ? 0.0f : bmp[(size_t)yy * bw + xx] / 255.0f; };
                        float v = (B(ix, iy) * (1 - ax) + B(ix + 1, iy) * ax) * (1 - ay) + (B(ix, iy + 1) * (1 - ax) + B(ix + 1, iy + 1) * ax) * ay;
                        over(tx, ty, v, lin);
                    }
            }
        }
        if (g.a.underline && !g.onPath) {
            int uy = (int)std::floor(g.y + g.a.size * 0.1f) - rect.y0;
            int th = std::max(1, (int)std::lround(g.a.size * 0.05f));
            for (int yy = uy; yy < uy + th; ++yy)
                for (int xx = (int)std::floor(g.x) - rect.x0; xx < (int)std::ceil(g.x + g.adv) - rect.x0; ++xx) over(xx, yy, 1.0f, lin);
        }
    }
    rgba.resize(acc.size());
    for (size_t i = 0; i < acc.size(); i += 4) {
        float a = acc[i + 3];
        float inv = a > 1e-6f ? 1.0f / a : 0.0f;
        rgba[i] = acc[i] * inv;
        rgba[i + 1] = acc[i + 1] * inv;
        rgba[i + 2] = acc[i + 2] * inv;
        rgba[i + 3] = a;
    }
    return true;
}

}  // namespace sx
