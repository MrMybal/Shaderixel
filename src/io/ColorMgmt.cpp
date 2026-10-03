// Color management through the Windows color system (ICM / mscms).
#include "io/ColorMgmt.h"

#include <windows.h>
#include <icm.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>

#pragma comment(lib, "mscms.lib")

namespace sx::color {

namespace {

std::string ExeDirectory() {
    wchar_t buf[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return Narrow(std::filesystem::path(buf).parent_path().wstring());
}

std::string ColorDir() {
    wchar_t buf[MAX_PATH] = L"";
    DWORD size = sizeof(buf);
    if (!GetColorDirectoryW(nullptr, buf, &size)) return "";
    return Narrow(buf);
}

u32 BE32(const u8* p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

// Profile class / color space read from the file header (bytes 12..19).
bool ReadHeader(const std::string& path, u32& cls, u32& space) {
    std::ifstream f(Utf8Path(path), std::ios::binary);
    u8 h[24] = {};
    if (!f.read((char*)h, sizeof(h))) return false;
    cls = BE32(h + 12);
    space = BE32(h + 16);
    return BE32(h + 0) > 128;
}

struct Profile {
    HPROFILE h = nullptr;
    ~Profile() { if (h) CloseColorProfile(h); }
};

HPROFILE OpenProfile(const std::string& path) {
    std::wstring w = Widen(path.empty() ? SrgbPath() : path);
    PROFILE p = {};
    p.dwType = PROFILE_FILENAME;
    p.pProfileData = (PVOID)w.c_str();
    p.cbDataSize = (DWORD)((w.size() + 1) * sizeof(wchar_t));
    return OpenColorProfileW(&p, PROFILE_READ, FILE_SHARE_READ, OPEN_EXISTING);
}

// Transforms are cached by chain + intent + flags (creation is the expensive part).
std::mutex gMutex;
std::map<std::string, HTRANSFORM> gTransforms;

HTRANSFORM GetTransform(const std::vector<std::string>& chain, int intent, DWORD flags) {
    std::string key;
    for (auto& c : chain) key += (c.empty() ? SrgbPath() : c) + "|";
    key += std::to_string(intent) + "|" + std::to_string(flags);
    auto it = gTransforms.find(key);
    if (it != gTransforms.end()) return it->second;
    std::vector<Profile> profs(chain.size());
    std::vector<HPROFILE> hs;
    for (size_t i = 0; i < chain.size(); ++i) {
        profs[i].h = OpenProfile(chain[i]);
        if (!profs[i].h) {
            Log("ICC: cannot open %s", chain[i].c_str());
            return nullptr;
        }
        hs.push_back(profs[i].h);
    }
    // One intent per profile (the call rejects nProfiles - 1 for longer chains).
    std::vector<DWORD> intents(chain.size(), (DWORD)intent);
    HTRANSFORM t = CreateMultiProfileTransform(hs.data(), (DWORD)hs.size(), intents.data(), (DWORD)intents.size(), flags, INDEX_DONT_CARE);
    if (!t) t = CreateMultiProfileTransform(hs.data(), (DWORD)hs.size(), intents.data(), 1, flags, INDEX_DONT_CARE);
    if (!t) Log("ICC: transform failed (%lu) for %s", GetLastError(), key.c_str());
    gTransforms[key] = t;
    return t;
}

}  // namespace

std::string SrgbPath() {
    static std::string p = [] {
        wchar_t buf[MAX_PATH] = L"";
        DWORD size = sizeof(buf);
        if (GetStandardColorSpaceProfileW(nullptr, LCS_sRGB, buf, &size)) return Narrow(buf);
        return ColorDir() + "\\sRGB Color Space Profile.icm";
    }();
    return p;
}

std::string UserProfileDir() { return ExeDirectory() + "/profiles"; }

const std::vector<ProfileInfo>& Profiles(bool rescan) {
    static std::vector<ProfileInfo> list;
    static bool scanned = false;
    if (scanned && !rescan) return list;
    scanned = true;
    list.clear();
    std::error_code ec;
    for (const std::string& dir : {ColorDir(), UserProfileDir()}) {
        if (dir.empty()) continue;
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(dir), ec)) {
            std::string ext = Narrow(e.path().extension().wstring());
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            if (ext != ".icc" && ext != ".icm") continue;
            ProfileInfo pi;
            pi.path = Narrow(e.path().wstring());
            pi.name = Narrow(e.path().stem().wstring());
            u32 cls = 0, space = 0;
            if (!ReadHeader(pi.path, cls, space)) continue;
            pi.rgb = space == 'RGB ';
            pi.cmyk = space == 'CMYK';
            pi.gray = space == 'GRAY';
            if (cls == 'link' || cls == 'abst' || cls == 'nmcl') continue;
            if (pi.rgb || pi.cmyk || pi.gray) list.push_back(pi);
        }
    }
    std::sort(list.begin(), list.end(), [](auto& a, auto& b) { return a.name < b.name; });
    return list;
}

std::string DefaultCmykPath() {
    std::string first;
    for (auto& p : Profiles()) {
        if (!p.cmyk) continue;
        if (first.empty()) first = p.path;
        std::string n = p.name;
        for (auto& c : n) c = (char)tolower((unsigned char)c);
        if (n.find("swop") != std::string::npos || n.find("fogra") != std::string::npos) return p.path;
    }
    return first;
}

std::string MonitorPath(void* hwnd) {
    HDC dc = GetDC((HWND)hwnd);
    wchar_t buf[MAX_PATH] = L"";
    DWORD size = MAX_PATH;
    BOOL ok = GetICMProfileW(dc, &size, buf);
    ReleaseDC((HWND)hwnd, dc);
    if (!ok) return "";
    std::string p = Narrow(buf);
    return std::filesystem::path(buf).filename() == std::filesystem::path(SrgbPath()).filename() ? "" : p;
}

std::string ProfileName(const std::string& path) {
    if (path.empty()) return "sRGB IEC61966-2.1";
    return Narrow(Utf8Path(path).stem().wstring());
}

bool TransformRgb(const std::vector<std::string>& chain, int intent, const float* rgb, size_t count, float* out, std::vector<u8>* gamut) {
    std::lock_guard lock(gMutex);
    if (chain.size() < 2) return false;
    HTRANSFORM t = GetTransform(chain, intent, BEST_MODE);
    if (!t) return false;
    // 16 bits per channel, R G B order (BM_16b_RGB).
    std::vector<u16> src(count * 3), dst(count * 3);
    // The ICM triplet formats are stored B, G, R in memory: swap on the way in and out.
    for (size_t i = 0; i < count; ++i)
        for (int k = 0; k < 3; ++k) src[i * 3 + 2 - k] = (u16)std::lround(std::clamp(rgb[i * 3 + k], 0.0f, 1.0f) * 65535.0f);
    if (!TranslateBitmapBits(t, src.data(), BM_16b_RGB, (DWORD)count, 1, 0, dst.data(), BM_16b_RGB, 0, nullptr, 0)) {
        Log("ICC: TranslateBitmapBits failed (%lu)", GetLastError());
        return false;
    }
    for (size_t i = 0; i < count; ++i)
        for (int k = 0; k < 3; ++k) out[i * 3 + k] = dst[i * 3 + 2 - k] / 65535.0f;
    if (gamut) {
        gamut->assign(count, 0);
        HTRANSFORM g = GetTransform({chain[0], chain[1]}, intent, BEST_MODE | ENABLE_GAMUT_CHECKING);
        if (g) {
            // CheckBitmapBits works on 8-bit RGB triplets.
            std::vector<u8> s8(count * 3);
            for (size_t i = 0; i < count * 3; ++i) s8[i] = (u8)(src[i] >> 8);
            CheckBitmapBits(g, s8.data(), BM_RGBTRIPLETS, (DWORD)count, 1, 0, gamut->data(), nullptr, 0);
        }
    }
    return true;
}

bool RgbToCmyk(const std::string& rgbProfile, const std::string& cmykProfile, int intent, const u8* rgb, size_t count, u8* cmyk) {
    std::lock_guard lock(gMutex);
    HTRANSFORM t = GetTransform({rgbProfile, cmykProfile}, intent, BEST_MODE);
    if (!t) return false;
    // Memory order: triplets B G R, CMYK quads K Y M C.
    std::vector<u8> bgr(count * 3), kymc(count * 4);
    for (size_t i = 0; i < count; ++i) for (int k = 0; k < 3; ++k) bgr[i * 3 + 2 - k] = rgb[i * 3 + k];
    if (!TranslateBitmapBits(t, bgr.data(), BM_RGBTRIPLETS, (DWORD)count, 1, 0, kymc.data(), BM_CMYKQUADS, 0, nullptr, 0)) return false;
    for (size_t i = 0; i < count; ++i) for (int k = 0; k < 4; ++k) cmyk[i * 4 + k] = kymc[i * 4 + 3 - k];
    return true;
}

bool CmykToRgb(const std::string& cmykProfile, const std::string& rgbProfile, int intent, const u8* cmyk, size_t count, u8* rgb) {
    std::lock_guard lock(gMutex);
    HTRANSFORM t = GetTransform({cmykProfile, rgbProfile}, intent, BEST_MODE);
    if (!t) return false;
    std::vector<u8> bgr(count * 3), kymc(count * 4);
    for (size_t i = 0; i < count; ++i) for (int k = 0; k < 4; ++k) kymc[i * 4 + 3 - k] = cmyk[i * 4 + k];
    if (!TranslateBitmapBits(t, kymc.data(), BM_CMYKQUADS, (DWORD)count, 1, 0, bgr.data(), BM_RGBTRIPLETS, 0, nullptr, 0)) return false;
    for (size_t i = 0; i < count; ++i) for (int k = 0; k < 3; ++k) rgb[i * 3 + k] = bgr[i * 3 + 2 - k];
    return true;
}

bool BuildLut(const std::vector<std::string>& chain, int intent, int n, bool gamutCheck, std::vector<float>& rgba) {
    const size_t count = (size_t)n * n * n;
    std::vector<float> in(count * 3), out(count * 3);
    for (int b = 0; b < n; ++b)
        for (int g = 0; g < n; ++g)
            for (int r = 0; r < n; ++r) {
                size_t i = ((size_t)b * n + g) * n + r;
                in[i * 3] = r / float(n - 1);
                in[i * 3 + 1] = g / float(n - 1);
                in[i * 3 + 2] = b / float(n - 1);
            }
    std::vector<u8> gamut;
    if (!TransformRgb(chain, intent, in.data(), count, out.data())) return false;
    if (gamutCheck && chain.size() >= 2) {
        // Out of gamut: the colorimetric round trip through the proofing space moves the color
        // (Delta E > 12 once both whites are matched; generic profiles such as RSWOP have no gamut
        // tag, and their round trips drift by a few units on reproducible colors).
        std::vector<float> rt(count * 3);
        if (TransformRgb({chain[0], chain[1], chain[0]}, Relative, in.data(), count, rt.data())) {
            const size_t w = count - 1;  // grid corner (1, 1, 1)
            float wr = std::max(rt[w * 3], 0.01f), wg = std::max(rt[w * 3 + 1], 0.01f), wb = std::max(rt[w * 3 + 2], 0.01f);
            gamut.resize(count);
            for (size_t i = 0; i < count; ++i) {
                float4 a = SrgbToLab({in[i * 3], in[i * 3 + 1], in[i * 3 + 2], 1});
                float4 b = SrgbToLab({std::min(rt[i * 3] / wr, 1.0f), std::min(rt[i * 3 + 1] / wg, 1.0f), std::min(rt[i * 3 + 2] / wb, 1.0f), 1});
                float de = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
                gamut[i] = de > 12.0f ? 255 : 0;
            }
        }
    }
    rgba.resize(count * 4);
    for (size_t i = 0; i < count; ++i) {
        rgba[i * 4] = out[i * 3];
        rgba[i * 4 + 1] = out[i * 3 + 1];
        rgba[i * 4 + 2] = out[i * 3 + 2];
        rgba[i * 4 + 3] = gamut.empty() || gamut[i] < 8 ? 1.0f : 0.0f;
    }
    return true;
}

// ------------------------------------------------------------------ Lab

namespace {
float ToLin(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
float ToSrgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(std::max(c, 0.0f), 1 / 2.4f) - 0.055f; }
// sRGB (D65) -> XYZ D50 (Bradford), and the inverse.
const float kToXyz[9] = {0.4360747f, 0.3850649f, 0.1430804f, 0.2225045f, 0.7168786f, 0.0606169f, 0.0139322f, 0.0971045f, 0.7141733f};
const float kToRgb[9] = {3.1338561f, -1.6168667f, -0.4906146f, -0.9787684f, 1.9161415f, 0.0334540f, 0.0719453f, -0.2289914f, 1.4052427f};
const float kWhite[3] = {0.9642f, 1.0f, 0.8251f};
float F(float t) { return t > 216.0f / 24389.0f ? std::cbrt(t) : (24389.0f / 27.0f * t + 16) / 116.0f; }
float Finv(float f) { float t = f * f * f; return t > 216.0f / 24389.0f ? t : (116 * f - 16) / (24389.0f / 27.0f); }
}  // namespace

float4 SrgbToLab(float4 s) {
    float l[3] = {ToLin(s.x), ToLin(s.y), ToLin(s.z)};
    float xyz[3];
    for (int i = 0; i < 3; ++i) xyz[i] = (kToXyz[i * 3] * l[0] + kToXyz[i * 3 + 1] * l[1] + kToXyz[i * 3 + 2] * l[2]) / kWhite[i];
    float fx = F(xyz[0]), fy = F(xyz[1]), fz = F(xyz[2]);
    return {116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz), 1};
}

float4 LabToSrgb(float4 lab) {
    float fy = (lab.x + 16) / 116, fx = fy + lab.y / 500, fz = fy - lab.z / 200;
    float xyz[3] = {Finv(fx) * kWhite[0], Finv(fy) * kWhite[1], Finv(fz) * kWhite[2]};
    float r[3];
    for (int i = 0; i < 3; ++i) r[i] = std::clamp(ToSrgb(kToRgb[i * 3] * xyz[0] + kToRgb[i * 3 + 1] * xyz[1] + kToRgb[i * 3 + 2] * xyz[2]), 0.0f, 1.0f);
    return {r[0], r[1], r[2], 1};
}

// ------------------------------------------------------------------ embedded profiles

std::vector<u8> ReadEmbeddedIcc(const std::string& path) {
    std::ifstream f(Utf8Path(path), std::ios::binary);
    std::vector<u8> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<u8> icc;
    if (d.size() > 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') {
        // PNG: iCCP = name \0 method(0) zlib data.
        size_t p = 8;
        while (p + 12 <= d.size()) {
            u32 len = BE32(&d[p]);
            if (p + 12 + len > d.size()) break;
            if (!memcmp(&d[p + 4], "iCCP", 4)) {
                const u8* c = &d[p + 8];
                size_t nameLen = strnlen((const char*)c, std::min<size_t>(len, 80));
                if (nameLen + 2 <= len) {
                    int outLen = 0;
                    char* out = stbi_zlib_decode_malloc((const char*)c + nameLen + 2, (int)(len - nameLen - 2), &outLen);
                    if (out) {
                        icc.assign(out, out + outLen);
                        free(out);
                    }
                }
                break;
            }
            if (!memcmp(&d[p + 4], "IDAT", 4)) break;
            p += 12 + len;
        }
    } else if (d.size() > 4 && d[0] == 0xFF && d[1] == 0xD8) {
        // JPEG: APP2 "ICC_PROFILE\0" seq count data, possibly split.
        std::map<int, std::vector<u8>> parts;
        size_t p = 2;
        while (p + 4 <= d.size() && d[p] == 0xFF) {
            u8 m = d[p + 1];
            if (m == 0xDA || m == 0xD9) break;
            u32 len = (u32)d[p + 2] << 8 | d[p + 3];
            if (m == 0xE2 && len > 16 && p + 2 + len <= d.size() && !memcmp(&d[p + 4], "ICC_PROFILE", 12)) {
                int seq = d[p + 16];
                parts[seq].assign(d.begin() + p + 18, d.begin() + p + 2 + len);
            }
            p += 2 + len;
        }
        for (auto& [k, v] : parts) icc.insert(icc.end(), v.begin(), v.end());
    }
    return icc;
}

std::string StoreEmbeddedIcc(const std::vector<u8>& icc) {
    if (icc.size() < 132) return "";
    // sRGB-like profiles: keep the document in sRGB (description tag mentions sRGB).
    std::string text(icc.begin(), icc.end());
    if (text.find("sRGB") != std::string::npos || text.find("s\0R\0G\0B", 0, 7) != std::string::npos) return "";
    if (BE32(&icc[16]) != 'RGB ') return "";
    Hasher h;
    h.Bytes(icc.data(), icc.size());
    std::filesystem::create_directories(Utf8Path(UserProfileDir()));
    char name[64];
    snprintf(name, sizeof(name), "/embedded_%016llx.icc", (unsigned long long)h.h);
    std::string path = UserProfileDir() + name;
    if (!std::filesystem::exists(Utf8Path(path))) {
        std::ofstream o(Utf8Path(path), std::ios::binary);
        o.write((const char*)icc.data(), (std::streamsize)icc.size());
        Profiles(true);
    }
    return path;
}

// ------------------------------------------------------------------ TIFF

bool WriteCmykTiff(const std::string& path, const u8* cmyk, u32 w, u32 h, const std::string& iccPath) {
    std::vector<u8> icc;
    if (!iccPath.empty()) {
        std::ifstream f(Utf8Path(iccPath), std::ios::binary);
        icc.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    std::ofstream o(Utf8Path(path), std::ios::binary);
    if (!o) return false;
    auto u16le = [&](u16 v) { o.put((char)(v & 255)); o.put((char)(v >> 8)); };
    auto u32le = [&](u32 v) { for (int i = 0; i < 4; ++i) o.put((char)((v >> (i * 8)) & 255)); };
    const u32 dataSize = w * h * 4;
    // Layout: header (8), pixel data, bits per sample (4 x u16), ICC, IFD.
    const u32 dataOff = 8, bpsOff = dataOff + dataSize, iccOff = bpsOff + 8, ifdOff = (iccOff + (u32)icc.size() + 1) & ~1u;
    o.write("II", 2);
    u16le(42);
    u32le(ifdOff);
    o.write((const char*)cmyk, dataSize);
    for (int i = 0; i < 4; ++i) u16le(8);
    o.write((const char*)icc.data(), (std::streamsize)icc.size());
    if (o.tellp() & 1) o.put(0);
    struct Tag { u16 tag, type; u32 count, value; };
    std::vector<Tag> tags = {
        {256, 4, 1, w}, {257, 4, 1, h}, {258, 3, 4, bpsOff}, {259, 3, 1, 1}, {262, 3, 1, 5},  // photometric: separated (CMYK)
        {273, 4, 1, dataOff}, {277, 3, 1, 4}, {278, 4, 1, h}, {279, 4, 1, dataSize}, {284, 3, 1, 1}, {332, 3, 1, 1},  // InkSet CMYK
    };
    if (!icc.empty()) tags.push_back({34675, 7, (u32)icc.size(), iccOff});
    u16le((u16)tags.size());
    for (auto& t : tags) {
        u16le(t.tag);
        u16le(t.type);
        u32le(t.count);
        if (t.type == 3 && t.count == 1) { u16le((u16)t.value); u16le(0); }
        else u32le(t.value);
    }
    u32le(0);
    return (bool)o;
}

}  // namespace sx::color
