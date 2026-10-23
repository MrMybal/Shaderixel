#include "io/PsdImport.h"
#include "render/Renderer.h"

#include <stb_image.h>

#include <cstring>
#include <fstream>

namespace sx {

namespace {
struct Reader {
    const std::vector<u8>& b;
    size_t pos = 0;
    bool ok = true;
    bool Can(size_t n) const { return ok && pos + n <= b.size(); }
    u8 U8() { if (!Can(1)) { ok = false; return 0; } return b[pos++]; }
    u16 U16() { u16 a = U8(); return (u16)((a << 8) | U8()); }
    i16 S16() { return (i16)U16(); }
    u32 U32() { u32 a = U16(); return (a << 16) | U16(); }
    i32 S32() { return (i32)U32(); }
    u64 U64() { u64 a = U32(); return (a << 32) | U32(); }
    float F32() { u32 v = U32(); float f; memcpy(&f, &v, 4); return f; }
    std::string Tag() {
        if (!Can(4)) { ok = false; return {}; }
        std::string t((const char*)&b[pos], 4);
        pos += 4;
        return t;
    }
    void Skip(size_t n) { if (!Can(n)) { ok = false; pos = b.size(); } else pos += n; }
    void Seek(size_t p) { if (p > b.size()) { ok = false; pos = b.size(); } else pos = p; }
};

struct Channel {
    i16 id = 0;
    u64 length = 0;
    std::vector<u8> plane;
};

struct PsdLayer {
    i32 top = 0, left = 0, bottom = 0, right = 0;
    i32 mtop = 0, mleft = 0, mbottom = 0, mright = 0;
    u8 maskDefault = 255, maskFlags = 0;
    // Pixel mask of a layer that also has a vector mask (channel -3): its own rectangle and default.
    i32 rtop = 0, rleft = 0, rbottom = 0, rright = 0;
    u8 realDefault = 255, realFlags = 0;
    bool real = false;
    std::string name;
    std::string blend = "norm";
    u8 opacity = 255, clipping = 0, flags = 0;
    u8 blendSrc[4][4] = {{0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}};  // gray, then per channel
    u8 blendDst[4][4] = {{0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}, {0, 0, 255, 255}};
    u32 section = 0;
    std::vector<Channel> channels;
    std::string adjKey;
    std::vector<u8> adjData;
    bool styles = false;
    // Damaged rectangles (overflowing or huge) count as empty.
    int W() const { const i64 v = (i64)right - left; return v > 0 && v <= 300000 ? (int)v : 0; }
    int H() const { const i64 v = (i64)bottom - top; return v > 0 && v <= 300000 ? (int)v : 0; }
};

std::string Utf16ToUtf8(const std::u16string& s) {
    std::wstring w(s.begin(), s.end());
    return Narrow(w);
}

std::string ReadPascal(Reader& r) {
    size_t start = r.pos;
    u8 n = r.U8();
    std::string s;
    for (int i = 0; i < n; ++i) s += (char)r.U8();
    size_t used = r.pos - start;
    r.Skip((4 - (used & 3)) & 3);
    // Pascal names are in the system code page; convert Latin-1 to UTF-8.
    std::wstring w(s.begin(), s.end());
    for (auto& c : w) c = (wchar_t)(unsigned char)c;
    return Narrow(w);
}

bool IsAdjustment(const std::string& k) {
    static const char* keys[] = {"SoCo", "GdFl", "PtFl", "brit", "levl", "curv", "expA", "vibA", "hue ", "hue2", "blnc",
                                 "blwh", "phfl", "mixr", "clrL", "nvrt", "post", "thrs", "grdm", "selc"};
    for (auto* x : keys) if (k == x) return true;
    return false;
}

int BlendFromKey(const std::string& k, bool& ok) {
    static const std::pair<const char*, BlendMode> map[] = {
        {"norm", BlendMode::Normal}, {"pass", BlendMode::Normal}, {"mul ", BlendMode::Multiply}, {"scrn", BlendMode::Screen},
        {"over", BlendMode::Overlay}, {"lddg", BlendMode::LinearDodge}, {"fsub", BlendMode::Subtract}, {"dark", BlendMode::Darken},
        {"lite", BlendMode::Lighten}, {"diff", BlendMode::Difference}, {"div ", BlendMode::ColorDodge}, {"idiv", BlendMode::ColorBurn},
        {"hLit", BlendMode::HardLight}, {"sLit", BlendMode::SoftLight}, {"diss", BlendMode::Dissolve}, {"lbrn", BlendMode::LinearBurn},
        {"vLit", BlendMode::VividLight}, {"lLit", BlendMode::LinearLight}, {"pLit", BlendMode::PinLight}, {"hMix", BlendMode::HardMix},
        {"smud", BlendMode::Exclusion}, {"fdiv", BlendMode::Divide}, {"hue ", BlendMode::Hue}, {"sat ", BlendMode::Saturation},
        {"colr", BlendMode::Color}, {"lum ", BlendMode::Luminosity}, {"dkCl", BlendMode::DarkerColor}, {"lgCl", BlendMode::LighterColor}};
    ok = true;
    for (auto& [key, m] : map) if (k == key) return (int)m;
    ok = false;
    return 0;
}

bool DecodeRle(Reader& r, size_t rowEnd, size_t expected, std::vector<u8>& out) {
    size_t start = out.size();
    while (r.ok && r.pos < rowEnd && out.size() - start < expected) {
        i8 h = (i8)r.U8();
        if (h >= 0) {
            for (int i = 0; i <= h; ++i) out.push_back(r.U8());
        } else if (h != -128) {
            u8 v = r.U8();
            for (int i = 0; i < 1 - h; ++i) out.push_back(v);
        }
    }
    r.Seek(rowEnd);
    out.resize(start + expected, 0);
    return r.ok;
}

bool DecodePlane(Reader& r, Channel& c, int w, int h, int depth, bool psb) {
    size_t start = r.pos;
    if (start > r.b.size() || c.length > r.b.size() - start) { r.Seek(r.b.size()); return w <= 0 || h <= 0; }
    size_t end = start + (size_t)c.length;
    if (c.length < 2) { r.Seek(end); return w <= 0 || h <= 0; }
    if (w <= 0 || h <= 0) { r.Seek(end); return true; }
    u16 comp = r.U16();
    size_t rowBytes = (size_t)w * (depth / 8);
    size_t expected = rowBytes * h;
    // RLE row lengths must fit in the channel data before anything is allocated.
    if (comp == 1 && (size_t)h * (psb ? 4 : 2) > end - r.pos) { r.Seek(end); return false; }
    c.plane.clear();
    c.plane.reserve(expected);
    bool ok = true;
    if (comp == 0) {
        if (r.pos + expected > end) ok = false;
        else c.plane.assign(r.b.begin() + r.pos, r.b.begin() + r.pos + expected);
    } else if (comp == 1) {
        std::vector<u32> lens(h);
        for (auto& l : lens) l = psb ? r.U32() : r.U16();
        for (int y = 0; ok && y < h; ++y) ok = DecodeRle(r, r.pos + lens[y], rowBytes, c.plane);
    } else if (comp == 2 || comp == 3) {
        c.plane.resize(expected);
        int got = stbi_zlib_decode_buffer((char*)c.plane.data(), (int)expected, (const char*)&r.b[r.pos], (int)(end - r.pos));
        ok = got == (int)expected;
        if (ok && comp == 3) {
            for (int y = 0; y < h; ++y) {
                u8* row = &c.plane[(size_t)y * rowBytes];
                if (depth == 8) {
                    for (int x = 1; x < w; ++x) row[x] = (u8)(row[x] + row[x - 1]);
                } else if (depth == 16) {
                    for (int x = 1; x < w; ++x) {
                        u16 prev = (u16)((row[(x - 1) * 2] << 8) | row[(x - 1) * 2 + 1]);
                        u16 cur = (u16)((row[x * 2] << 8) | row[x * 2 + 1]);
                        cur = (u16)(cur + prev);
                        row[x * 2] = (u8)(cur >> 8);
                        row[x * 2 + 1] = (u8)cur;
                    }
                } else {
                    for (size_t i = 1; i < rowBytes; ++i) row[i] = (u8)(row[i] + row[i - 1]);
                    // Bytes are stored as planes (all MSBs, then next...): interleave back to big-endian floats.
                    std::vector<u8> tmp(row, row + rowBytes);
                    for (int x = 0; x < w; ++x)
                        for (int k = 0; k < 4; ++k) row[x * 4 + k] = tmp[(size_t)k * w + x];
                }
            }
        }
    } else {
        ok = false;
    }
    r.Seek(end);
    return ok;
}

const std::vector<u8>* Plane(const PsdLayer& l, i16 id) {
    for (auto& c : l.channels) if (c.id == id && !c.plane.empty()) return &c.plane;
    return nullptr;
}

// Channel value at pixel i as linear float (sRGB decoded for 8/16-bit color channels).
float Sample(const std::vector<u8>* p, size_t i, int depth, float def) {
    if (!p) return def;
    if (depth == 8) return i < p->size() ? (*p)[i] / 255.0f : def;
    if (depth == 16) return i * 2 + 1 < p->size() ? (((*p)[i * 2] << 8) | (*p)[i * 2 + 1]) / 65535.0f : def;
    if (i * 4 + 3 >= p->size()) return def;
    u32 v = ((u32)(*p)[i * 4] << 24) | ((u32)(*p)[i * 4 + 1] << 16) | ((u32)(*p)[i * 4 + 2] << 8) | (*p)[i * 4 + 3];
    float f;
    memcpy(&f, &v, 4);
    return f;
}

double FindDescriptorDouble(const std::vector<u8>& d, const char* key) {
    size_t n = strlen(key);
    for (size_t i = 0; i + n + 12 <= d.size(); ++i) {
        if (memcmp(&d[i], key, n) != 0) continue;
        for (size_t j = i + n; j + 12 <= d.size() && j < i + n + 8; ++j) {
            if (memcmp(&d[j], "doub", 4) != 0) continue;
            u64 v = 0;
            for (int k = 0; k < 8; ++k) v = (v << 8) | d[j + 4 + k];
            double out;
            memcpy(&out, &v, 8);
            return out;
        }
    }
    return -1;
}

// Configure a Shaderixel effect from a Photoshop adjustment record. Returns false if unsupported.
bool ConvertAdjustment(const PsdLayer& s, Layer& l) {
    Reader r{s.adjData};
    const std::string& k = s.adjKey;
    auto setEffect = [&](const char* id) {
        l.effect.effectId = id;
        if (auto def = Effects().Find(id)) l.effect.params = def->DefaultParams();
        return !l.effect.params.empty() || Effects().Find(id) != nullptr;
    };
    if (k == "brit") {
        setEffect("brightness_contrast");
        float b = r.S16(), c = r.S16();
        l.effect.params[0].x = b / 250.0f;
        l.effect.params[1].x = std::clamp(c / 100.0f, -1.0f, 1.0f);
    } else if (k == "levl") {
        setEffect("levels");
        r.U16();
        float inB = r.U16(), inW = r.U16(), outB = r.U16(), outW = r.U16(), gamma = r.U16();
        if (r.ok) {
            l.effect.params[0].x = inB / 255.0f;
            l.effect.params[1].x = inW / 255.0f;
            l.effect.params[2].x = gamma / 100.0f;
            l.effect.params[3].x = outB / 255.0f;
            l.effect.params[4].x = outW / 255.0f;
        }
    } else if (k == "hue " || k == "hue2") {
        setEffect("hue_saturation");
        r.U16();
        u8 colorize = r.U8();
        r.U8();
        i16 ch = r.S16(), cs = r.S16(), cl = r.S16();
        i16 mh = r.S16(), ms = r.S16(), ml = r.S16();
        if (colorize) { l.effect.params[0].x = ch - 180.0f; l.effect.params[1].x = cs - 25.0f; l.effect.params[2].x = cl; l.effect.params[3].x = 1; }
        else { l.effect.params[0].x = mh; l.effect.params[1].x = ms; l.effect.params[2].x = ml; }
    } else if (k == "curv") {
        // Map flag, version, channel bitmap (0 composite, 1 R, 2 G, 3 B), point lists (output, input).
        setEffect("curves");
        const u8 isMap = r.U8();
        r.U16();
        const u32 bits = r.U32();
        l.effect.curves.resize(std::max<size_t>(l.effect.curves.size(), 4));
        for (int ch = 0; ch < 32 && r.ok; ++ch) {
            if (!(bits & (1u << ch))) continue;
            CurvePoints c;
            if (isMap) {
                // 256 output values: kept as 17 points.
                u8 m[256];
                for (auto& x : m) x = r.U8();
                for (int i = 0; i <= 256; i += 16) {
                    const int k2 = std::min(i, 255);
                    c.push_back({k2 / 255.0f, m[k2] / 255.0f});
                }
            } else {
                const int n = r.U16();
                for (int i = 0; i < n && r.ok; ++i) {  // every pair read (the next channel follows), 64 kept
                    const float out = r.U16() / 255.0f, in = r.U16() / 255.0f;
                    if (i < 64) c.push_back({in, out});
                }
            }
            if (!r.ok) break;  // truncated: the partial list is not kept
            if (ch < 4) l.effect.curves[ch] = c;
        }
    } else if (k == "nvrt") {
        setEffect("invert");
    } else if (k == "post") {
        setEffect("posterize");
        l.effect.params[0].x = (float)std::clamp<int>(r.U16(), 2, 64);
    } else if (k == "thrs") {
        setEffect("threshold");
        l.effect.params[0].x = std::clamp<int>(r.U16(), 0, 255) / 255.0f;
    } else if (k == "expA") {
        setEffect("exposure");
        r.U16();
        l.effect.params[0].x = r.F32();
        l.effect.params[1].x = r.F32();
        float g = r.F32();
        l.effect.params[2].x = g > 0 ? g : 1.0f;
    } else if (k == "vibA") {
        setEffect("vibrance");
    } else if (k == "blwh") {
        setEffect("black_white");
    } else if (k == "mixr") {
        setEffect("channel_mixer");
    } else if (k == "grdm") {
        setEffect("gradient_map");
    } else if (k == "blnc") {
        setEffect("color_balance");
    } else if (k == "SoCo") {
        l.kind = LayerKind::Fill;
        setEffect("solid_color");
        double rr = FindDescriptorDouble(s.adjData, "Rd  "), gg = FindDescriptorDouble(s.adjData, "Grn "), bb = FindDescriptorDouble(s.adjData, "Bl  ");
        if (rr >= 0 && gg >= 0 && bb >= 0)
            l.effect.params[0] = {SrgbToLinear((float)rr / 255.0f), SrgbToLinear((float)gg / 255.0f), SrgbToLinear((float)bb / 255.0f), 1};
    } else {
        return false;
    }
    return true;
}
}  // namespace

std::unique_ptr<Document> ImportPsd(const std::string& path, Renderer& renderer, gpu::CommandContext& ctx, PsdImportReport& report, std::string* error) {
    auto fail = [&](const std::string& m) -> std::unique_ptr<Document> { if (error) *error = m; return nullptr; };
    std::ifstream f(Utf8Path(path), std::ios::binary);
    if (!f) return fail("fichier introuvable");
    std::vector<u8> data((std::istreambuf_iterator<char>(f)), {});
    Reader r{data};
    if (r.Tag() != "8BPS") return fail("fichier PSD invalide");
    const u16 version = r.U16();
    if (version != 1 && version != 2) return fail("version de PSD inconnue");
    const bool psb = version == 2;  // large document format: 64-bit section lengths
    r.Skip(6);
    int channels = r.U16();
    int H = r.S32(), W = r.S32();
    int depth = r.U16(), mode = r.U16();
    if (!r.ok || W <= 0 || H <= 0 || W > 300000 || H > 300000 || (depth != 8 && depth != 16 && depth != 32) || (mode != 1 && mode != 3) || channels <= 0)
        return fail("PSD RVB ou niveaux de gris 8/16/32 bits uniquement");
    r.Skip(r.U32());  // color mode data
    // Image resources: keep saved paths (ids 2000-2997).
    std::vector<VectorPath> paths;
    {
        u32 resLen = r.U32();
        size_t resEnd = r.pos + resLen;
        if (resEnd > data.size()) return fail("fichier PSD tronqué");
        while (r.ok && r.pos + 12 <= resEnd) {
            if (r.Tag() != "8BIM") break;
            u16 id = r.U16();
            u8 nlen = r.U8();
            std::string name;
            for (u8 k = 0; k < nlen; ++k) {
                u8 c = r.U8();
                if (c < 0x80) name.push_back((char)c);
                else { name.push_back((char)(0xC0 | (c >> 6))); name.push_back((char)(0x80 | (c & 0x3F))); }  // Latin-1 -> UTF-8
            }
            if ((nlen + 1) % 2) r.Skip(1);
            u32 size = r.U32();
            size_t start = r.pos;
            if (!r.Can(size)) break;
            if (id >= 2000 && id <= 2997) {
                VectorPath p;
                p.name = name;
                if (DecodePsdPath(&data[start], size, W, H, p)) paths.push_back(std::move(p));
            }
            r.Seek(start + size + (size & 1));
        }
        r.Seek(resEnd);
    }
    u64 lmLen = psb ? r.U64() : r.U32();
    if (!r.ok || lmLen < 4 || lmLen > data.size() - r.pos) return fail("aucun calque");  // (no wrap-around)
    size_t lmEnd = r.pos + (size_t)lmLen;
    u64 liLen = psb ? r.U64() : r.U32();
    if (!r.ok || liLen > lmEnd - r.pos) return fail("aucun calque");
    size_t liEnd = r.pos + (size_t)liLen;
    int count = liLen >= 2 ? std::abs((int)r.S16()) : 0;
    if (count == 0 && (depth == 16 || depth == 32)) {
        // 16 / 32-bit documents: an empty layer info, the layers are in a top-level Lr16 / Lr32 block after the global mask.
        r.Seek(liEnd);
        const u32 gmLen = r.U32();
        if (r.ok && gmLen <= lmEnd - r.pos) r.Seek(r.pos + gmLen);
        bool found = false;
        while (r.ok && !found && r.pos + 12 <= lmEnd) {
            while (r.pos + 4 <= lmEnd && memcmp(&data[r.pos], "8BIM", 4) != 0 && memcmp(&data[r.pos], "8B64", 4) != 0 && r.pos % 4 != 0) r.Seek(r.pos + 1);  // padding
            const std::string sig = r.Tag(), key = r.Tag();
            if (sig != "8BIM" && sig != "8B64") break;
            const bool longLen = psb && (key == "Lr16" || key == "Lr32" || key == "Layr" || key == "LMsk" || key == "Mt16" || key == "Mt32" || key == "Mtrn" || key == "Alph" || key == "FMsk" || key == "lnk2" || key == "FEid" || key == "FXid" || key == "PxSD");
            const u64 len = longLen ? r.U64() : r.U32();
            if (!r.ok || len > lmEnd - r.pos) break;
            const size_t ds = r.pos;
            if (key == "Lr16" || key == "Lr32" || key == "Layr") {
                liEnd = ds + (size_t)len;
                count = len >= 2 ? std::abs((int)r.S16()) : 0;
                found = true;
            } else {
                r.Seek(ds + (size_t)len);
            }
        }
    }
    if (count <= 0 || count > 32768) return fail("aucun calque");

    std::vector<PsdLayer> layers(count);
    for (auto& L : layers) {
        L.top = r.S32(); L.left = r.S32(); L.bottom = r.S32(); L.right = r.S32();
        int nc = r.U16();
        if (nc > 64) { r.ok = false; break; }
        L.channels.resize(nc);
        for (auto& c : L.channels) { c.id = r.S16(); c.length = psb ? r.U64() : r.U32(); }
        if (r.Tag() != "8BIM") { r.ok = false; break; }
        L.blend = r.Tag();
        L.opacity = r.U8(); L.clipping = r.U8(); L.flags = r.U8(); r.Skip(1);
        u32 extraLen = r.U32();
        size_t extraEnd = r.pos + extraLen;
        if (extraEnd > liEnd) { r.ok = false; break; }
        u32 maskLen = r.U32();
        size_t maskEnd = r.pos + maskLen;
        if (maskLen >= 18 && maskEnd <= extraEnd) {
            L.mtop = r.S32(); L.mleft = r.S32(); L.mbottom = r.S32(); L.mright = r.S32();
            L.maskDefault = r.U8(); L.maskFlags = r.U8();
            if (maskLen >= 36) {
                // Real flags, real background, real rectangle: the last 18 bytes (after optional mask parameters).
                r.Seek(maskEnd - 18);
                L.realFlags = r.U8();
                L.realDefault = r.U8();
                L.rtop = r.S32(); L.rleft = r.S32(); L.rbottom = r.S32(); L.rright = r.S32();
                L.real = r.ok && L.rright > L.rleft && L.rbottom > L.rtop;
            }
        }
        r.Seek(std::min(maskEnd, extraEnd));
        {
            u32 rangesLen = r.U32();
            size_t rangesEnd = r.pos + rangesLen;
            // Gray pair, then one pair per channel (RGB: red, green, blue).
            for (u32 c = 0; c < 4 && (c + 1) * 8 <= rangesLen; ++c) {
                for (auto& x : L.blendSrc[c]) x = r.U8();
                for (auto& x : L.blendDst[c]) x = r.U8();
            }
            r.Seek(std::min(rangesEnd, extraEnd));
        }
        L.name = ReadPascal(r);
        while (r.ok && r.pos + 12 <= extraEnd) {
            std::string sig = r.Tag(), key = r.Tag();
            // PSB: a few keys carry 64-bit lengths.
            static const char* longKeys[] = {"LMsk", "Lr16", "Lr32", "Layr", "Mt16", "Mt32", "Mtrn", "Alph", "FMsk", "lnk2", "FEid", "FXid", "PxSD"};
            bool longLen = sig == "8B64";
            if (psb) for (auto* k : longKeys) if (key == k) longLen = true;
            u64 len = longLen ? r.U64() : r.U32();
            if (!r.ok || r.pos > extraEnd || len > extraEnd - r.pos) { r.ok = false; break; }  // (no wrap-around)
            size_t ds = r.pos, de = ds + (size_t)len;
            if (sig != "8BIM" && sig != "8B64") { r.ok = false; break; }
            if (key == "luni") {
                u32 n = r.U32();
                std::u16string s;
                for (u32 i = 0; i < n && r.pos + 2 <= de; ++i) s += (char16_t)r.U16();
                if (!s.empty()) L.name = Utf16ToUtf8(s);
            } else if (key == "lsct" || key == "lsdk") {
                if (len >= 4) L.section = r.U32();
                if (len >= 12) { r.Tag(); L.blend = r.Tag(); }
            } else if (IsAdjustment(key)) {
                L.adjKey = key;
                L.adjData.assign(data.begin() + ds, data.begin() + de);
            } else if (key == "lfx2" || key == "lrFX") {
                L.styles = true;
            }
            r.Seek(de + (len & 1));
        }
        r.Seek(extraEnd);
        if (L.name.empty()) L.name = "Calque PSD";
    }
    if (!r.ok) return fail("enregistrements de calques invalides");
    for (auto& L : layers)
        for (auto& c : L.channels) {
            bool isMask = c.id == -2 || c.id == -3;
            int cw = isMask && L.mright > L.mleft ? L.mright - L.mleft : L.W();
            int chh = isMask && L.mbottom > L.mtop ? L.mbottom - L.mtop : L.H();
            if (c.id == -3 && L.real) { cw = L.rright - L.rleft; chh = L.rbottom - L.rtop; }
            if (!DecodePlane(r, c, cw, chh, depth, psb)) report.warnings.push_back("canal non décodé : " + L.name);
        }
    if (!r.ok) return fail("données de canaux invalides");

    auto doc = std::make_unique<Document>((u32)W, (u32)H, depth == 32 ? Precision::Float32 : Precision::Half16, DocMode::Layers,
                                          Narrow(Utf8Path(path).filename().wstring()));
    DocState& st = doc->S();
    std::vector<Id> groupStack;
    struct PixelJob { Id surface; const PsdLayer* src; };
    struct MaskJob { Id surface; const PsdLayer* src; };
    std::vector<PixelJob> pixelJobs;
    std::vector<MaskJob> maskJobs;

    auto common = [&](Layer& l, const PsdLayer& s) {
        bool ok;
        l.blend = (BlendMode)BlendFromKey(s.blend, ok);
        if (!ok) report.warnings.push_back("mode de fusion non supporté (" + s.blend + ") : " + s.name);
        l.opacity = s.opacity / 255.0f;
        l.visible = (s.flags & 0x02) == 0;
        l.clipped = s.clipping != 0;
        memcpy(l.blendIf.self, s.blendSrc, sizeof(l.blendIf.self));
        memcpy(l.blendIf.under, s.blendDst, sizeof(l.blendIf.under));
        l.parent = groupStack.empty() ? 0 : groupStack.back();
        if (s.styles) report.warnings.push_back("styles de calque ignorés : " + s.name);
        if ((Plane(s, -2) || Plane(s, -3)) && s.mright > s.mleft && s.mbottom > s.mtop) {
            float def = s.maskDefault / 255.0f;
            l.mask = doc->CreateSurface({def, def, def, 1});
            l.maskEnabled = (s.maskFlags & 0x02) == 0;
            maskJobs.push_back({l.mask, &s});
            report.masks++;
        }
    };

    for (const PsdLayer& s : layers) {
        if (s.section == 3) {  // group start (bottom of group)
            groupStack.push_back(doc->NewId());
            continue;
        }
        if (s.section == 1 || s.section == 2) {  // group end: the group record itself
            if (groupStack.empty()) continue;
            Layer g;
            g.id = groupStack.back();
            groupStack.pop_back();
            g.kind = LayerKind::Group;
            g.name = s.name;
            common(g, s);
            g.passThrough = s.blend == "pass";
            g.collapsed = s.section == 2;
            st.layers.push_back(g);
            report.groups++;
            continue;
        }
        Layer l;
        l.id = doc->NewId();
        l.name = s.name;
        if (!s.adjKey.empty()) {
            l.kind = LayerKind::Adjustment;
            if (!ConvertAdjustment(s, l)) {
                report.warnings.push_back("calque de réglage non supporté (" + s.adjKey + ") : " + s.name);
                continue;
            }
            common(l, s);
            st.layers.push_back(l);
            report.adjustments++;
            continue;
        }
        l.kind = LayerKind::Pixel;
        common(l, s);
        l.surface = doc->CreateSurface({0, 0, 0, 0});
        if (s.W() > 0 && s.H() > 0 && Plane(s, 0)) pixelJobs.push_back({l.surface, &s});
        st.layers.push_back(l);
        report.layers++;
    }
    // Unclosed groups: drop their ids from children.
    for (Id g : groupStack)
        for (auto& l : st.layers) if (l.parent == g) l.parent = 0;
    if (st.layers.empty()) return fail("aucun calque décodé");
    st.activeLayer = st.layers.back().id;

    // Clear surfaces, then upload layer rectangles.
    renderer.Prepare(*doc, ctx);
    for (auto& job : pixelJobs) {
        const PsdLayer& s = *job.src;
        int w = s.W(), h = s.H();
        const auto *c0 = Plane(s, 0), *c1 = Plane(s, 1), *c2 = Plane(s, 2), *ca = Plane(s, -1);
        std::vector<float> px((size_t)w * h * 4);
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            float rr = Sample(c0, i, depth, 0);
            float gg = mode == 1 ? rr : Sample(c1, i, depth, rr);
            float bb = mode == 1 ? rr : Sample(c2, i, depth, rr);
            float aa = Sample(ca, i, depth, 1.0f);
            if (depth != 32) { rr = SrgbToLinear(rr); gg = SrgbToLinear(gg); bb = SrgbToLinear(bb); }
            px[i * 4] = rr; px[i * 4 + 1] = gg; px[i * 4 + 2] = bb; px[i * 4 + 3] = aa;
        }
        renderer.Import(*doc, ctx, job.surface, px.data(), (u32)w, (u32)h, true, {s.left, s.top});
    }
    for (auto& job : maskJobs) {
        const PsdLayer& s = *job.src;
        // With a vector mask too, the pixel mask is channel -3 with its own rectangle.
        const bool useReal = Plane(s, -3) && s.real;
        int w = useReal ? s.rright - s.rleft : s.mright - s.mleft, h = useReal ? s.rbottom - s.rtop : s.mbottom - s.mtop;
        const int ox = useReal ? s.rleft : s.mleft, oy = useReal ? s.rtop : s.mtop;
        const auto* p = useReal ? Plane(s, -3) : Plane(s, -2) ? Plane(s, -2) : Plane(s, -3);
        std::vector<float> px((size_t)w * h * 4);
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            float v = std::clamp(Sample(p, i, depth, 1.0f), 0.0f, 1.0f);
            px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = v;
            px[i * 4 + 3] = 1;
        }
        renderer.Import(*doc, ctx, job.surface, px.data(), (u32)w, (u32)h, true, {ox, oy});
    }
    for (auto& p : paths) {
        p.id = doc->NewId();
        st.paths.push_back(std::move(p));
    }
    doc->SyncGraph();
    doc->dirty = false;
    return doc;
}

}  // namespace sx
