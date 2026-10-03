// Adobe brushes / patterns / swatches readers (big-endian binary formats).
#include "io/AdobeFormats.h"
#include "io/ColorMgmt.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace sx {

namespace {

struct Reader {
    const u8* p = nullptr;
    size_t n = 0, pos = 0;
    bool fail = false;
    bool Has(size_t k) const { return !fail && pos + k <= n; }
    u8 U8() { if (!Has(1)) { fail = true; return 0; } return p[pos++]; }
    u16 U16() { if (!Has(2)) { fail = true; return 0; } u16 v = (u16)(p[pos] << 8 | p[pos + 1]); pos += 2; return v; }
    u32 U32() { if (!Has(4)) { fail = true; return 0; } u32 v = (u32)p[pos] << 24 | (u32)p[pos + 1] << 16 | (u32)p[pos + 2] << 8 | p[pos + 3]; pos += 4; return v; }
    float F32() { u32 v = U32(); float f; memcpy(&f, &v, 4); return f; }
    void Skip(size_t k) { if (!Has(k)) { fail = true; pos = n; } else pos += k; }
    bool Tag(const char* t) { if (!Has(4) || memcmp(p + pos, t, 4)) return false; pos += 4; return true; }
    // UTF-16BE string of `count` code units.
    std::string Utf16(u32 count) {
        std::wstring w;
        for (u32 i = 0; i < count && !fail; ++i) {
            u16 c = U16();
            if (c) w.push_back((wchar_t)c);
        }
        return Narrow(w);
    }
};

std::vector<u8> ReadFile(const std::string& path) {
    std::ifstream f(Utf8Path(path), std::ios::binary);
    return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// PackBits rows (row byte counts first) or raw, `bytes` per sample (high byte kept).
bool ReadChannel(Reader& r, int w, int h, int depth, bool rle, std::vector<u8>& out) {
    const int bps = std::max(1, depth / 8);
    const size_t rowBytes = (size_t)w * bps;
    std::vector<u8> raw(rowBytes * h);
    if (!rle) {
        if (!r.Has(raw.size())) return false;
        memcpy(raw.data(), r.p + r.pos, raw.size());
        r.pos += raw.size();
    } else {
        std::vector<u16> counts(h);
        for (int y = 0; y < h; ++y) counts[y] = r.U16();
        for (int y = 0; y < h && !r.fail; ++y) {
            size_t end = r.pos + counts[y], x = 0;
            u8* row = &raw[(size_t)y * rowBytes];
            while (r.pos < end && x < rowBytes && !r.fail) {
                int c = (int8_t)r.U8();
                if (c >= 0) {
                    for (int k = 0; k <= c && x < rowBytes; ++k) row[x++] = r.U8();
                } else if (c != -128) {
                    u8 v = r.U8();
                    for (int k = 0; k < 1 - c && x < rowBytes; ++k) row[x++] = v;
                }
            }
            r.pos = end;
        }
        if (r.fail) return false;
    }
    out.resize((size_t)w * h);
    for (size_t i = 0; i < out.size(); ++i) out[i] = raw[i * bps];  // big-endian: high byte first
    return true;
}

}  // namespace

// ------------------------------------------------------------------ .abr

bool ReadAbr(const std::string& path, std::vector<AbrTip>& tips, std::string* error) {
    std::vector<u8> data = ReadFile(path);
    Reader r{data.data(), data.size()};
    auto err = [&](const char* m) { if (error) *error = m; return false; };
    if (data.size() < 4) return err("fichier vide");
    u16 version = r.U16(), sub = r.U16();
    std::string stem = Narrow(Utf8Path(path).stem().wstring());
    if (version == 1 || version == 2) {
        for (u16 i = 0; i < sub && !r.fail; ++i) {
            u16 type = r.U16();
            u32 size = r.U32();
            size_t next = r.pos + size;
            if (type == 2) {
                r.U32();
                u16 spacing = r.U16();
                std::string name;
                if (version == 2) name = r.Utf16(r.U32());
                r.U8();
                r.Skip(8);
                int top = (int)r.U32(), left = (int)r.U32(), bottom = (int)r.U32(), right = (int)r.U32();
                int depth = r.U16();
                bool rle = r.U8() != 0;
                AbrTip t;
                t.w = right - left;
                t.h = bottom - top;
                if (t.w > 0 && t.h > 0 && t.w <= 5000 && t.h <= 5000 && ReadChannel(r, t.w, t.h, depth, rle, t.gray)) {
                    t.name = name.empty() ? stem + " " + std::to_string(tips.size() + 1) : name;
                    t.spacing = spacing ? spacing / 100.0f : 0.25f;
                    tips.push_back(std::move(t));
                }
            }
            r.pos = next;
            r.fail = r.pos > r.n;
        }
        return !tips.empty() || err("aucune pointe échantillonnée dans ce fichier");
    }
    if (version != 6 && version != 10) return err("version .abr non prise en charge");
    // Sections "8BIM" + key + size; sampled tips are in "samp".
    while (r.Has(12) && !r.fail) {
        if (!r.Tag("8BIM")) return err("structure .abr inattendue");
        char key[5] = {};
        memcpy(key, r.p + r.pos, 4);
        r.pos += 4;
        u32 size = r.U32();
        size_t sectionEnd = r.pos + size;
        if (!strcmp(key, "samp")) {
            while (r.pos + 4 < sectionEnd && !r.fail) {
                u32 brushSize = r.U32();
                size_t brushEnd = r.pos + ((brushSize + 3) & ~3u);
                r.Skip(sub == 1 ? 47 : 301);
                int top = (int)r.U32(), left = (int)r.U32(), bottom = (int)r.U32(), right = (int)r.U32();
                int depth = r.U16();
                bool rle = r.U8() != 0;
                AbrTip t;
                t.w = right - left;
                t.h = bottom - top;
                if (!r.fail && t.w > 0 && t.h > 0 && t.w <= 5000 && t.h <= 5000 && ReadChannel(r, t.w, t.h, depth, rle, t.gray)) {
                    t.name = stem + " " + std::to_string(tips.size() + 1);
                    tips.push_back(std::move(t));
                }
                r.pos = brushEnd;
                r.fail = r.pos > r.n;
            }
        }
        r.pos = sectionEnd;
        r.fail = r.pos > r.n;
    }
    return !tips.empty() || err("aucune pointe échantillonnée dans ce fichier");
}

// ------------------------------------------------------------------ .pat

bool ReadPat(const std::string& path, std::vector<PatternImage>& out, std::string* error) {
    std::vector<u8> data = ReadFile(path);
    Reader r{data.data(), data.size()};
    auto err = [&](const char* m) { if (error) *error = m; return false; };
    if (!r.Tag("8BPT")) return err("ce n'est pas un fichier de motifs Photoshop");
    r.U16();  // version
    u32 count = r.U32();
    for (u32 i = 0; i < count && !r.fail; ++i) {
        r.U32();  // version
        u32 mode = r.U32();
        int h = r.U16(), w = r.U16();
        std::string name = r.Utf16(r.U32());
        r.Skip(r.U8());  // unique id (Pascal string)
        std::vector<u8> palette;
        if (mode == 2) {
            palette.assign(r.p + std::min(r.pos, r.n), r.p + std::min(r.pos + 768, r.n));
            r.Skip(768);
            if (r.Has(4)) r.Skip(4);  // index count / transparent index
        }
        // Virtual memory array list.
        r.U32();
        u32 vmaLen = r.U32();
        size_t vmaEnd = r.pos + vmaLen;
        int top = (int)r.U32(), left = (int)r.U32(), bottom = (int)r.U32(), right = (int)r.U32();
        u32 channels = r.U32();
        int cw = right - left, ch = bottom - top;
        if (cw <= 0 || ch <= 0) { cw = w; ch = h; }
        std::vector<std::vector<u8>> planes;
        for (u32 c = 0; c < channels + 2 && !r.fail && r.pos < vmaEnd; ++c) {
            u32 written = r.U32();
            if (!written) continue;
            u32 len = r.U32();
            size_t chEnd = r.pos + len;
            r.U32();  // depth
            int t = (int)r.U32(), l = (int)r.U32(), b = (int)r.U32(), rr = (int)r.U32();
            int depth = r.U16();
            bool rle = r.U8() != 0;
            std::vector<u8> plane;
            if (rr - l == cw && b - t == ch && ReadChannel(r, cw, ch, depth, rle, plane)) planes.push_back(std::move(plane));
            r.pos = chEnd;
        }
        r.pos = vmaEnd;
        if (r.fail) break;
        PatternImage pi;
        pi.name = name.empty() ? "Motif " + std::to_string(out.size() + 1) : name;
        pi.w = cw;
        pi.h = ch;
        pi.rgba.assign((size_t)cw * ch * 4, 255);
        if (mode == 3 && planes.size() >= 3) {
            for (size_t p = 0; p < (size_t)cw * ch; ++p)
                for (int k = 0; k < 3; ++k) pi.rgba[p * 4 + k] = planes[k][p];
            if (planes.size() >= 4) for (size_t p = 0; p < (size_t)cw * ch; ++p) pi.rgba[p * 4 + 3] = planes[3][p];
        } else if ((mode == 1 || mode == 8) && !planes.empty()) {
            for (size_t p = 0; p < (size_t)cw * ch; ++p) for (int k = 0; k < 3; ++k) pi.rgba[p * 4 + k] = planes[0][p];
            if (planes.size() >= 2) for (size_t p = 0; p < (size_t)cw * ch; ++p) pi.rgba[p * 4 + 3] = planes[1][p];
        } else if (mode == 2 && !planes.empty() && palette.size() >= 768) {
            for (size_t p = 0; p < (size_t)cw * ch; ++p) for (int k = 0; k < 3; ++k) pi.rgba[p * 4 + k] = palette[planes[0][p] * 3 + k];
        } else {
            continue;  // CMYK / Lab patterns: skipped
        }
        out.push_back(std::move(pi));
    }
    return !out.empty() || err("aucun motif lisible (modes RVB, niveaux de gris et couleurs indexées)");
}

// ------------------------------------------------------------------ swatches

bool ReadSwatches(const std::string& path, std::vector<Swatch>& out, std::string* error) {
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    std::vector<u8> data = ReadFile(path);
    auto err = [&](const char* m) { if (error) *error = m; return false; };
    if (ext == ".gpl") {
        std::istringstream in(std::string(data.begin(), data.end()));
        std::string line;
        while (std::getline(in, line)) {
            int r, g, b;
            char name[256] = "";
            if (line.empty() || line[0] == '#' || !isdigit((unsigned char)line[0]) && line[0] != ' ') continue;
            int n = sscanf_s(line.c_str(), "%d %d %d %255[^\r\n]", &r, &g, &b, name, (unsigned)sizeof(name));
            if (n >= 3) out.push_back({n >= 4 ? std::string(name) : "", {r / 255.0f, g / 255.0f, b / 255.0f, 1}});
        }
        return !out.empty() || err("palette vide");
    }
    Reader r{data.data(), data.size()};
    auto cmyk = [](float c, float m, float y, float k) {
        u8 in[4] = {(u8)std::lround(c * 255), (u8)std::lround(m * 255), (u8)std::lround(y * 255), (u8)std::lround(k * 255)}, rgb[3];
        std::string prof = color::DefaultCmykPath();
        if (!prof.empty() && color::CmykToRgb(prof, "", 1, in, 1, rgb)) return float4{rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, 1};
        return float4{(1 - c) * (1 - k), (1 - m) * (1 - k), (1 - y) * (1 - k), 1};
    };
    if (ext == ".ase") {
        if (!r.Tag("ASEF")) return err("ce n'est pas un fichier .ase");
        r.U16();
        r.U16();
        u32 blocks = r.U32();
        for (u32 i = 0; i < blocks && !r.fail; ++i) {
            u16 type = r.U16();
            u32 len = r.U32();
            size_t end = r.pos + len;
            if (type == 0x0001) {
                std::string name = r.Utf16(r.U16());
                char model[5] = {};
                if (r.Has(4)) memcpy(model, r.p + r.pos, 4);
                r.Skip(4);
                Swatch s;
                s.name = name;
                if (!strcmp(model, "RGB ")) { float a = r.F32(), b = r.F32(), c = r.F32(); s.srgb = {a, b, c, 1}; }
                else if (!strcmp(model, "CMYK")) { float a = r.F32(), b = r.F32(), c = r.F32(), d = r.F32(); s.srgb = cmyk(a, b, c, d); }
                else if (!strcmp(model, "LAB ")) { float l = r.F32(), a = r.F32(), b = r.F32(); s.srgb = color::LabToSrgb({l * 100, a, b, 1}); }
                else if (!strcmp(model, "Gray")) { float g = r.F32(); s.srgb = {g, g, g, 1}; }
                out.push_back(s);
            }
            r.pos = end;
        }
        return !out.empty() || err("aucune couleur");
    }
    // .aco: version 1 block, optionally followed by version 2 (with names).
    std::vector<Swatch> v1;
    for (int block = 0; block < 2 && r.Has(4); ++block) {
        u16 ver = r.U16(), count = r.U16();
        std::vector<Swatch> list;
        for (u16 i = 0; i < count && !r.fail; ++i) {
            u16 space = r.U16(), w = r.U16(), x = r.U16(), y = r.U16(), z = r.U16();
            Swatch s;
            if (space == 0) s.srgb = {w / 65535.0f, x / 65535.0f, y / 65535.0f, 1};
            else if (space == 1) {
                float rr, gg, bb;
                float hh = w / 65535.0f, ss = x / 65535.0f, vv = y / 65535.0f;
                int k = (int)(hh * 6) % 6;
                float f = hh * 6 - std::floor(hh * 6), p = vv * (1 - ss), q = vv * (1 - f * ss), t = vv * (1 - (1 - f) * ss);
                switch (k) { case 0: rr = vv; gg = t; bb = p; break; case 1: rr = q; gg = vv; bb = p; break; case 2: rr = p; gg = vv; bb = t; break;
                             case 3: rr = p; gg = q; bb = vv; break; case 4: rr = t; gg = p; bb = vv; break; default: rr = vv; gg = p; bb = q; }
                s.srgb = {rr, gg, bb, 1};
            } else if (space == 2) s.srgb = cmyk(1 - w / 65535.0f, 1 - x / 65535.0f, 1 - y / 65535.0f, 1 - z / 65535.0f);
            else if (space == 7) s.srgb = color::LabToSrgb({w / 100.0f, (int16_t)x / 100.0f, (int16_t)y / 100.0f, 1});
            else if (space == 8) { float g = 1 - w / 10000.0f; s.srgb = {g, g, g, 1}; }
            else continue;
            if (ver == 2) {
                u32 len = r.U32();
                s.name = r.Utf16(len);
            }
            list.push_back(s);
        }
        if (r.fail) break;
        if (ver == 2) { out = list; return !out.empty() || err("aucune couleur"); }
        v1 = list;
    }
    out = v1;
    return !out.empty() || err("aucune couleur lisible");
}

bool WriteAco(const std::string& path, const std::vector<Swatch>& sw) {
    std::ofstream o(Utf8Path(path), std::ios::binary);
    if (!o) return false;
    auto u16 = [&](unsigned v) { o.put((char)((v >> 8) & 255)); o.put((char)(v & 255)); };
    auto u32 = [&](unsigned v) { u16(v >> 16); u16(v & 0xFFFF); };
    for (int ver = 1; ver <= 2; ++ver) {
        u16(ver);
        u16((unsigned)sw.size());
        for (auto& s : sw) {
            u16(0);
            u16((unsigned)std::lround(std::clamp(s.srgb.x, 0.0f, 1.0f) * 65535));
            u16((unsigned)std::lround(std::clamp(s.srgb.y, 0.0f, 1.0f) * 65535));
            u16((unsigned)std::lround(std::clamp(s.srgb.z, 0.0f, 1.0f) * 65535));
            u16(0);
            if (ver == 2) {
                std::wstring w = Widen(s.name);
                u32((unsigned)w.size() + 1);
                for (wchar_t c : w) u16((unsigned)c);
                u16(0);
            }
        }
    }
    return (bool)o;
}

}  // namespace sx
