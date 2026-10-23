#include "io/Lensfun.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace sx {

namespace fs = std::filesystem;

std::string ExeDir();  // ui/App.cpp

namespace {

// ---------------------------------------------------------------- minimal XML (lensfun files)
struct XmlNode {
    std::string name, text;
    std::map<std::string, std::string> attr;
    std::vector<XmlNode> children;
    const XmlNode* Child(const char* n) const {
        for (auto& c : children) if (c.name == n) return &c;
        return nullptr;
    }
    std::string ChildText(const char* n) const {
        // Without a lang attribute first (the English / main name).
        const XmlNode* best = nullptr;
        for (auto& c : children)
            if (c.name == n && (!best || (best->attr.count("lang") && !c.attr.count("lang")))) best = &c;
        return best ? best->text : std::string();
    }
    float Attr(const char* n, float def) const {
        auto it = attr.find(n);
        return it == attr.end() ? def : (float)std::atof(it->second.c_str());
    }
};

std::string Unescape(std::string s) {
    static const std::pair<const char*, const char*> e[] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
    for (auto& [a, b] : e)
        for (size_t p; (p = s.find(a)) != std::string::npos;) s.replace(p, std::strlen(a), b);
    return s;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Parses the document into a root node (its children: the top-level elements).
void ParseXml(const std::string& s, XmlNode& root) {
    std::vector<XmlNode*> stack = {&root};
    size_t i = 0;
    while (i < s.size()) {
        size_t lt = s.find('<', i);
        if (lt == std::string::npos) break;
        if (lt > i && stack.size() > 1) stack.back()->text += s.substr(i, lt - i);
        if (s.compare(lt, 4, "<!--") == 0) { size_t e = s.find("-->", lt); i = e == std::string::npos ? s.size() : e + 3; continue; }
        if (s.compare(lt, 2, "<?") == 0 || s.compare(lt, 2, "<!") == 0) { size_t e = s.find('>', lt); i = e == std::string::npos ? s.size() : e + 1; continue; }
        size_t gt = s.find('>', lt);
        if (gt == std::string::npos) break;
        std::string tag = s.substr(lt + 1, gt - lt - 1);
        i = gt + 1;
        if (!tag.empty() && tag[0] == '/') {
            if (stack.size() > 1) {
                stack.back()->text = Unescape(Trim(stack.back()->text));
                stack.pop_back();
            }
            continue;
        }
        const bool selfClose = !tag.empty() && tag.back() == '/';
        if (selfClose) tag.pop_back();
        XmlNode n;
        size_t p = 0;
        while (p < tag.size() && !isspace((unsigned char)tag[p])) ++p;
        n.name = tag.substr(0, p);
        while (p < tag.size()) {
            while (p < tag.size() && isspace((unsigned char)tag[p])) ++p;
            size_t eq = tag.find('=', p);
            if (eq == std::string::npos) break;
            std::string key = Trim(tag.substr(p, eq - p));
            size_t q1 = tag.find_first_of("\"'", eq);
            if (q1 == std::string::npos) break;
            size_t q2 = tag.find(tag[q1], q1 + 1);
            if (q2 == std::string::npos) break;
            n.attr[key] = Unescape(tag.substr(q1 + 1, q2 - q1 - 1));
            p = q2 + 1;
        }
        stack.back()->children.push_back(std::move(n));
        if (!selfClose) stack.push_back(&stack.back()->children.back());
    }
}

// Names compared without case, punctuation or spacing ("EF-S 18-55mm f/3.5-5.6" ~ "ef s 18 55mm f 3 5 5 6").
std::vector<std::string> Tokens(const std::string& s) {
    std::vector<std::string> t;
    std::string cur;
    for (unsigned char c : s) {
        if (isalnum(c)) cur.push_back((char)tolower(c));
        else if (!cur.empty()) { t.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
}

}  // namespace

bool LensfunDb::Load(const std::string& folder, std::string* error) {
    lenses.clear();
    cameras.clear();
    dir = folder;
    std::error_code ec;
    int files = 0;
    for (auto it = fs::recursive_directory_iterator(Utf8Path(folder), fs::directory_options::skip_permission_denied, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
        if (it->path().extension() != ".xml") continue;
        std::ifstream f(it->path(), std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        const std::string text = ss.str();
        if (text.find("<lensdatabase") == std::string::npos) continue;
        XmlNode root;
        ParseXml(text, root);
        const XmlNode* db = root.Child("lensdatabase");
        if (!db) continue;
        ++files;
        // Crop factor: 1 (full frame) when missing or invalid (empty, 0, NaN, absurd).
        auto cropOf = [](const std::string& s) {
            const float v = s.empty() ? 0.0f : (float)std::atof(s.c_str());
            return v > 0.0f && v < 100.0f ? std::max(0.1f, v) : 1.0f;
        };
        for (const XmlNode& n : db->children) {
            if (n.name == "camera") {
                LensfunCamera c;
                c.maker = n.ChildText("maker");
                c.model = n.ChildText("model");
                c.mount = n.ChildText("mount");
                c.crop = cropOf(n.ChildText("cropfactor"));
                cameras.push_back(c);
            } else if (n.name == "lens") {
                LensfunLens l;
                l.maker = n.ChildText("maker");
                l.model = n.ChildText("model");
                for (auto& c : n.children) if (c.name == "mount") l.mounts += (l.mounts.empty() ? "" : ", ") + c.text;
                l.crop = cropOf(n.ChildText("cropfactor"));
                if (const XmlNode* cal = n.Child("calibration")) {
                    for (const XmlNode& e : cal->children) {
                        auto modelOf = [&](const char* key) { auto it = e.attr.find(key); return it == e.attr.end() ? std::string() : it->second; };
                        const std::string m = modelOf("model");
                        // Lensfun v2 "real-focal" differs from the nominal focal of the EXIF: the nominal one is used.
                        const float focal = e.Attr("focal", 0);
                        if (e.name == "distortion") {
                            LensfunDistortion d;
                            d.focal = focal;
                            d.model = m == "poly3" ? 1 : m == "poly5" ? 2 : m == "ptlens" ? 3 : 0;
                            if (d.model == 1) d.a = e.Attr("k1", 0);
                            else if (d.model == 2) { d.a = e.Attr("k1", 0); d.b = e.Attr("k2", 0); }
                            else if (d.model == 3) { d.a = e.Attr("a", 0); d.b = e.Attr("b", 0); d.c = e.Attr("c", 0); }
                            if (d.model) l.distortion.push_back(d);
                        } else if (e.name == "tca") {
                            LensfunTca t;
                            t.focal = focal;
                            t.model = m == "linear" ? 1 : m == "poly3" ? 2 : 0;
                            if (t.model == 1) { t.vr = e.Attr("kr", 1); t.vb = e.Attr("kb", 1); }
                            else if (t.model == 2) {
                                t.vr = e.Attr("vr", 1); t.vb = e.Attr("vb", 1);
                                t.cr = e.Attr("cr", 0); t.cb = e.Attr("cb", 0);
                                t.br = e.Attr("br", 0); t.bb = e.Attr("bb", 0);
                            }
                            if (t.model) l.tca.push_back(t);
                        } else if (e.name == "vignetting" && m == "pa") {
                            LensfunVignetting v;
                            v.focal = focal;
                            v.aperture = e.Attr("aperture", 8);
                            v.distance = e.Attr("distance", 1000);
                            v.k1 = e.Attr("k1", 0); v.k2 = e.Attr("k2", 0); v.k3 = e.Attr("k3", 0);
                            l.vignetting.push_back(v);
                        }
                    }
                }
                float lo = 1e9f, hi = 0;
                for (auto& d : l.distortion) { lo = std::min(lo, d.focal); hi = std::max(hi, d.focal); }
                for (auto& d : l.tca) { lo = std::min(lo, d.focal); hi = std::max(hi, d.focal); }
                for (auto& d : l.vignetting) { lo = std::min(lo, d.focal); hi = std::max(hi, d.focal); }
                l.minFocal = hi > 0 ? lo : 0;
                l.maxFocal = hi;
                if (!l.model.empty()) lenses.push_back(std::move(l));
            }
        }
    }
    std::sort(lenses.begin(), lenses.end(), [](const LensfunLens& a, const LensfunLens& b) { return a.Name() < b.Name(); });
    if (lenses.empty() && error) *error = files ? "aucun objectif dans les fichiers lensfun" : "aucun fichier lensfun (*.xml) dans " + folder;
    return !lenses.empty();
}

const LensfunCamera* LensfunDb::FindCamera(const std::string& maker, const std::string& model) const {
    const auto want = Tokens(model);
    const LensfunCamera* best = nullptr;
    int bestScore = 0;
    for (auto& c : cameras) {
        const auto t = Tokens(c.model);
        int score = 0;
        for (auto& w : want) score += std::find(t.begin(), t.end(), w) != t.end() ? 2 : 0;
        if (!maker.empty() && Tokens(c.maker) == Tokens(maker)) score += 1;
        if (t == want) score += 10;
        if (score > bestScore) { bestScore = score; best = &c; }
    }
    return bestScore >= 4 ? best : nullptr;
}

const LensfunLens* LensfunDb::FindLens(const std::string& name, const std::string& maker) const {
    const auto want = Tokens(name);
    if (want.empty()) return nullptr;
    const LensfunLens* best = nullptr;
    float bestScore = 0;
    for (auto& l : lenses) {
        const auto t = Tokens(l.model);
        int common = 0;
        for (auto& w : want) common += std::find(t.begin(), t.end(), w) != t.end() ? 1 : 0;
        // Mostly how much of the query is found (EXIF names are often shorter), a little how much of the model.
        float score = 0.75f * common / (float)want.size() + 0.25f * common / (float)std::max<size_t>(t.size(), 1);
        if (!maker.empty() && Tokens(l.maker) == Tokens(maker)) score += 0.05f;
        if (score > bestScore) { bestScore = score; best = &l; }
    }
    return bestScore >= 0.6f ? best : nullptr;
}

LensProfile LensfunDb::Profile(const LensfunLens& lens, float focal, float aperture, float distance, float cameraCrop) {
    LensProfile p;
    p.lens = lens.Name();
    if (focal <= 0) focal = lens.minFocal > 0 ? lens.minFocal : 50;
    p.focal = focal;
    p.cropRatio = cameraCrop > 0 ? lens.crop / cameraCrop : 1.0f;
    // Calibrations around the focal length, linearly interpolated (same model on both sides).
    auto bracket = [&](auto& list, auto& lo, auto& hi, float& t) {
        lo = hi = nullptr;
        for (auto& e : list) {
            if (e.focal <= focal && (!lo || e.focal > lo->focal)) lo = &e;
            if (e.focal >= focal && (!hi || e.focal < hi->focal)) hi = &e;
        }
        if (!lo) lo = hi;
        if (!hi) hi = lo;
        t = (lo && hi && hi->focal > lo->focal) ? (focal - lo->focal) / (hi->focal - lo->focal) : 0.0f;
    };
    {
        const LensfunDistortion *lo, *hi;
        float t;
        bracket(lens.distortion, lo, hi, t);
        if (lo) {
            if (hi->model != lo->model) hi = lo;
            p.distModel = lo->model;
            p.a = lo->a + (hi->a - lo->a) * t;
            p.b = lo->b + (hi->b - lo->b) * t;
            p.c = lo->c + (hi->c - lo->c) * t;
        }
    }
    {
        const LensfunTca *lo, *hi;
        float t;
        bracket(lens.tca, lo, hi, t);
        if (lo) {
            if (hi->model != lo->model) hi = lo;
            p.tcaModel = lo->model;
            auto L = [&](float x, float y) { return x + (y - x) * t; };
            p.vr = L(lo->vr, hi->vr); p.vb = L(lo->vb, hi->vb);
            p.cr = L(lo->cr, hi->cr); p.cb = L(lo->cb, hi->cb);
            p.br = L(lo->br, hi->br); p.bb = L(lo->bb, hi->bb);
        }
    }
    if (!lens.vignetting.empty()) {
        // Nearest calibrations in (focal, aperture in stops, distance), inverse-distance weighted.
        if (aperture <= 0) aperture = 8;
        if (distance <= 0) distance = 1000;
        std::vector<std::pair<float, const LensfunVignetting*>> d;
        for (auto& v : lens.vignetting) {
            const float df = std::fabs(std::log(std::max(v.focal, 0.1f) / focal)) * 3.0f;
            const float da = std::fabs(std::log2(std::max(v.aperture, 0.5f) / aperture));
            const float dd = std::fabs(std::log(std::max(v.distance, 0.01f) / distance)) * 0.2f;
            d.push_back({df + da + dd, &v});
        }
        std::sort(d.begin(), d.end(), [](auto& x, auto& y) { return x.first < y.first; });
        float ws = 0;
        for (size_t i = 0; i < d.size() && i < 4; ++i) {
            const float w = 1.0f / (d[i].first + 1e-3f);
            p.k1 += d[i].second->k1 * w;
            p.k2 += d[i].second->k2 * w;
            p.k3 += d[i].second->k3 * w;
            ws += w;
        }
        p.k1 /= ws;
        p.k2 /= ws;
        p.k3 /= ws;
        p.vignetting = true;
    }
    return p;
}

std::string FindLensfunDir(const std::string& setting) {
    std::vector<std::string> dirs;
    if (!setting.empty()) dirs.push_back(setting);
    if (const char* la = getenv("LOCALAPPDATA")) dirs.push_back(std::string(la) + "\\Shaderixel\\lensfun");
    dirs.push_back(ExeDir() + "\\lensfun");
    for (const char* pf : {"ProgramFiles", "ProgramW6432", "ProgramFiles(x86)"})
        if (const char* p = getenv(pf)) {
            dirs.push_back(std::string(p) + "\\darktable\\share\\lensfun");
            dirs.push_back(std::string(p) + "\\lensfun\\share\\lensfun");
            dirs.push_back(std::string(p) + "\\RawTherapee\\share\\lensfun");
        }
    if (const char* la = getenv("LOCALAPPDATA")) dirs.push_back(std::string(la) + "\\lensfun");
    for (auto& d : dirs) {
        std::error_code ec;
        if (!fs::is_directory(Utf8Path(d), ec)) continue;
        for (auto it = fs::recursive_directory_iterator(Utf8Path(d), fs::directory_options::skip_permission_denied, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
            if (it->path().extension() == ".xml") return d;
        }
    }
    return {};
}

LensfunDb& Lensfun() {
    static LensfunDb db;
    return db;
}

}  // namespace sx
