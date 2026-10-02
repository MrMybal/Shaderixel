#include "io/TextRaster.h"

#define STB_TRUETYPE_IMPLEMENTATION
#pragma warning(push, 0)
#include <stb_truetype.h>
#pragma warning(pop)

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
        int offset = stbtt_GetFontOffsetForIndex(f->data.data(), 0);
        f->ok = offset >= 0 && stbtt_InitFont(&f->info, f->data.data(), offset);
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

bool RasterizeText(const TextStyle& t, int docW, int docH, std::vector<float>& rgba, Rect& rect) {
    LoadedFont* f = GetFont(t.font.empty() ? DefaultFontPath() : t.font);
    if (!f) return false;
    const stbtt_fontinfo& info = f->info;
    float scale = stbtt_ScaleForMappingEmToPixels(&info, std::max(t.size, 1.0f));
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    float lineH = (ascent - descent) * scale * t.lineSpacing;
    float track = t.tracking / 1000.0f * t.size;

    // Split into lines, measure.
    std::vector<std::vector<u32>> lines(1);
    for (u32 cp : DecodeUtf8(t.content)) {
        if (cp == '\r') continue;
        if (cp == '\n') lines.emplace_back();
        else lines.back().push_back(cp);
    }
    struct Glyph { u32 cp; float x, y; };
    std::vector<Glyph> glyphs;
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (size_t li = 0; li < lines.size(); ++li) {
        float width = 0;
        for (size_t i = 0; i < lines[li].size(); ++i) {
            int adv, lsb;
            stbtt_GetCodepointHMetrics(&info, lines[li][i], &adv, &lsb);
            width += adv * scale + track;
            if (i + 1 < lines[li].size()) width += stbtt_GetCodepointKernAdvance(&info, lines[li][i], lines[li][i + 1]) * scale;
        }
        float x = t.pos.x - (t.align == 1 ? width * 0.5f : t.align == 2 ? width : 0.0f);
        float baseline = t.pos.y + ascent * scale + li * lineH;
        for (size_t i = 0; i < lines[li].size(); ++i) {
            u32 cp = lines[li][i];
            glyphs.push_back({cp, x, baseline});
            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBoxSubpixel(&info, cp, scale, scale, x - std::floor(x), 0, &x0, &y0, &x1, &y1);
            if (x1 > x0 && y1 > y0) {
                minX = std::min(minX, std::floor(x) + x0);
                maxX = std::max(maxX, std::floor(x) + x1);
                minY = std::min(minY, std::floor(baseline) + y0);
                maxY = std::max(maxY, std::floor(baseline) + y1);
            }
            int adv, lsb;
            stbtt_GetCodepointHMetrics(&info, cp, &adv, &lsb);
            x += adv * scale + track;
            if (i + 1 < lines[li].size()) x += stbtt_GetCodepointKernAdvance(&info, cp, lines[li][i + 1]) * scale;
        }
    }
    int pad = t.faux ? 2 : 1;
    rect = Rect{(int)minX - pad, (int)minY - pad, (int)maxX + pad, (int)maxY + pad}.Clipped(docW, docH);
    rgba.clear();
    if (glyphs.empty() || rect.Empty()) return true;
    const int w = rect.Width(), h = rect.Height();
    std::vector<float> cov((size_t)w * h, 0.0f);
    std::vector<unsigned char> bmp;
    for (auto& g : glyphs) {
        int x0, y0, x1, y1;
        float fx = g.x - std::floor(g.x);
        stbtt_GetCodepointBitmapBoxSubpixel(&info, g.cp, scale, scale, fx, 0, &x0, &y0, &x1, &y1);
        int bw = x1 - x0, bh = y1 - y0;
        if (bw <= 0 || bh <= 0) continue;
        bmp.assign((size_t)bw * bh, 0);
        stbtt_MakeCodepointBitmapSubpixel(&info, bmp.data(), bw, bh, bw, scale, scale, fx, 0, g.cp);
        int ox = (int)std::floor(g.x) + x0 - rect.x0, oy = (int)std::floor(g.y) + y0 - rect.y0;
        for (int y = 0; y < bh; ++y) {
            int ty = oy + y;
            if (ty < 0 || ty >= h) continue;
            for (int x = 0; x < bw; ++x) {
                int tx = ox + x;
                if (tx < 0 || tx >= w) continue;
                float& c = cov[(size_t)ty * w + tx];
                float v = bmp[(size_t)y * bw + x] / 255.0f;
                c = c + v - c * v;  // union of overlapping glyphs
            }
        }
    }
    if (t.faux) {  // 1px dilation for a faux-bold look
        std::vector<float> d(cov);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                float m = cov[(size_t)y * w + x];
                if (x > 0) m = std::max(m, cov[(size_t)y * w + x - 1] * 0.8f);
                if (y > 0) m = std::max(m, cov[(size_t)(y - 1) * w + x] * 0.8f);
                d[(size_t)y * w + x] = m;
            }
        cov.swap(d);
    }
    float r = SrgbToLinear(t.color.x), gg = SrgbToLinear(t.color.y), b = SrgbToLinear(t.color.z);
    rgba.resize((size_t)w * h * 4);
    for (size_t i = 0; i < cov.size(); ++i) {
        rgba[i * 4] = r;
        rgba[i * 4 + 1] = gg;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = cov[i] * t.color.w;
    }
    return true;
}

}  // namespace sx
