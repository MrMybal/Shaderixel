#include "ui/Lang.h"
#include "core/Common.h"
#include "core/Json.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

// Hook of the patched Dear ImGui (imgui.cpp).
extern bool (*GSXTranslateFn)(const char** text, const char** text_end);

namespace sx {

std::string ExeDir();  // ui/App.cpp

namespace {

namespace fs = std::filesystem;

std::string gLang = "fr";
// Dictionary: French -> translation; keys with numbers replaced by {n} are patterns.
std::unordered_map<std::string, std::string> gDict;
std::unordered_map<std::string_view, const std::string*> gView;  // lookup without allocation
// Texts built at run time (patterns, multi-line): kept alive for the frame and reused.
std::unordered_map<std::string, std::string> gDynamic;
std::unordered_set<std::string> gMiss;
// Patterns with holes: "%s" in the key ("Photo : %s"), and the keys ending with a colon ("Exporté :"),
// followed at run time by a name or a path. Literal parts in order; the holes are copied to the translation.
struct Wildcard {
    std::vector<std::string> parts;  // literal text around the holes (parts.size() = holes + 1)
    const std::string* translation;  // with "%s" for each hole, in the same order
    size_t literal = 0;
};
std::vector<Wildcard> gWild;
std::vector<std::string> gColonForms;  // translations of the colon keys, with a "%s" appended

std::vector<std::string> LangDirs() {
    return {ExeDir() + "/lang", ExeDir() + "/../../library/lang"};
}

bool IsNumChar(char c) { return c >= '0' && c <= '9'; }

// Digit runs (with decimals) -> {n}; the numbers are returned in order.
std::string PatternOf(std::string_view s, std::vector<std::string>& nums) {
    std::string out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size();) {
        if (IsNumChar(s[i])) {
            size_t j = i;
            while (j < s.size() && (IsNumChar(s[j]) || ((s[j] == '.' || s[j] == ',') && j + 1 < s.size() && IsNumChar(s[j + 1])))) ++j;
            nums.emplace_back(s.substr(i, j - i));
            out += "{n}";
            i = j;
        } else {
            out += s[i++];
        }
    }
    return out;
}

const std::string* Exact(std::string_view s);

// The text (numbers already as {n}) against the patterns with holes: the translation with the holes filled.
bool MatchWildcards(const std::string& text, std::string& out) {
    for (const Wildcard& w : gWild) {
        if (w.literal > text.size()) continue;
        const std::string& first = w.parts.front();
        if (text.compare(0, first.size(), first) != 0) continue;
        std::vector<std::string> holes;
        size_t pos = first.size();
        bool ok = true;
        for (size_t i = 1; i < w.parts.size() && ok; ++i) {
            const std::string& lit = w.parts[i];
            size_t at;
            if (i + 1 == w.parts.size()) {
                // Last part: anchored at the end.
                if (text.size() < pos + lit.size() || text.compare(text.size() - lit.size(), lit.size(), lit) != 0) { ok = false; break; }
                at = text.size() - lit.size();
            } else {
                at = lit.empty() ? pos : text.find(lit, pos);
                if (at == std::string::npos) { ok = false; break; }
            }
            if (at <= pos) { ok = false; break; }  // a hole is never empty
            holes.push_back(text.substr(pos, at - pos));
            pos = at + lit.size();
        }
        if (!ok) continue;
        out.clear();
        size_t k = 0;
        const std::string& t = *w.translation;
        for (size_t i = 0; i < t.size();) {
            if (t.compare(i, 2, "%s") == 0 && k < holes.size()) {
                // The hole itself when it is a known text ("Préréglage : Choisir...").
                const std::string* ht = Exact(holes[k]);
                out += ht ? *ht : holes[k];
                ++k;
                i += 2;
            } else {
                out += t[i++];
            }
        }
        return true;
    }
    return false;
}

const std::string* Exact(std::string_view s) {
    auto it = gView.find(s);
    return it == gView.end() ? nullptr : it->second;
}

// Translation of one text, or nullptr when there is none.
const std::string* Translate(std::string_view s) {
    if (const std::string* t = Exact(s)) return t;
    std::string key(s);
    if (auto it = gDynamic.find(key); it != gDynamic.end()) return &it->second;
    if (gMiss.count(key)) return nullptr;
    std::string result;
    bool found = false;
    // Surrounding spaces (labels padded by the code): translated inside.
    size_t a = 0, b = s.size();
    while (a < b && s[a] == ' ') ++a;
    while (b > a && s[b - 1] == ' ') --b;
    if ((a > 0 || b < s.size()) && b > a) {
        if (const std::string* t = Translate(s.substr(a, b - a))) {
            result = std::string(s.substr(0, a)) + *t + std::string(s.substr(b));
            found = true;
        }
    }
    if (!found && s.find('\n') != std::string_view::npos) {
        // Line by line.
        size_t start = 0;
        bool any = false;
        while (start <= s.size()) {
            size_t e = s.find('\n', start);
            if (e == std::string_view::npos) e = s.size();
            std::string_view line = s.substr(start, e - start);
            const std::string* t = line.empty() ? nullptr : Translate(line);
            result += t ? *t : std::string(line);
            any |= t != nullptr;
            if (e == s.size()) break;
            result += '\n';
            start = e + 1;
        }
        found = any;
    }
    if (!found) {
        // Numbers as {n}, then the exact pattern or a pattern with holes; the numbers put back in order.
        std::vector<std::string> nums;
        const std::string pat = PatternOf(s, nums);
        std::string filled;
        const std::string* t = nums.empty() ? nullptr : Exact(pat);
        if (!t && MatchWildcards(pat, filled)) t = &filled;
        if (t) {
            size_t k = 0;
            result.clear();
            for (size_t i = 0; i < t->size();) {
                if (t->compare(i, 3, "{n}") == 0 && k < nums.size()) {
                    result += nums[k++];
                    i += 3;
                } else {
                    result += (*t)[i++];
                }
            }
            found = true;
        }
    }
    if (gDynamic.size() > 20000) gDynamic.clear();
    if (gMiss.size() > 50000) gMiss.clear();
    if (!found) {
        gMiss.insert(std::move(key));
        return nullptr;
    }
    return &(gDynamic[std::move(key)] = std::move(result));
}

bool Hook(const char** text, const char** end) {
    if (gView.empty()) return false;
    const size_t n = (size_t)(*end - *text);
    if (n < 2 || n > 4000) return false;
    const std::string* t = Translate(std::string_view(*text, n));
    if (!t) return false;
    *text = t->data();
    *end = t->data() + t->size();
    return true;
}

}  // namespace

std::vector<Language> AvailableLanguages() {
    std::vector<Language> out = {{"fr", "Français"}};
    std::unordered_set<std::string> seen = {"fr"};
    for (auto& d : LangDirs()) {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(Utf8Path(d), ec)) {
            if (e.path().extension() != ".json") continue;
            const std::string code = Narrow(e.path().stem().wstring());
            if (!seen.insert(code).second) continue;
            std::string name = code == "en" ? "English" : code;
            out.push_back({code, name});
        }
    }
    return out;
}

const std::string& CurrentLanguage() { return gLang; }

bool SetLanguage(const std::string& code) {
    gDict.clear();
    gView.clear();
    gDynamic.clear();
    gMiss.clear();
    gWild.clear();
    gColonForms.clear();
    GSXTranslateFn = &Hook;
    gLang = "fr";
    if (code == "fr" || code.empty()) return true;
    for (auto& d : LangDirs()) {
        std::string text;
        Json j;
        if (!ReadTextFile(d + "/" + code + ".json", text) || !Json::Parse(text, j) || !j.IsObject()) continue;
        for (auto& [k, v] : j.Items())
            if (v.GetType() == Json::Type::String && !v.S("").empty() && v.S("") != k) {
                // printf escapes of the source: "%%" is drawn as "%".
                auto unescape = [](std::string x) {
                    for (size_t at; (at = x.find("%%")) != std::string::npos;) x.replace(at, 2, "%");
                    return x;
                };
                gDict[unescape(k)] = unescape(v.S(""));
            }
        for (auto& [k, v] : gDict) gView[std::string_view(k)] = &v;
        // Patterns with holes: "%s" keys, and "... :" keys (a name follows at run time).
        gColonForms.reserve(gDict.size());
        auto addWild = [](const std::string& key, const std::string* tr) {
            Wildcard w;
            w.translation = tr;
            size_t start = 0;
            for (size_t at; (at = key.find("%s", start)) != std::string::npos; start = at + 2) w.parts.push_back(key.substr(start, at - start));
            w.parts.push_back(key.substr(start));
            for (auto& part : w.parts) w.literal += part.size();
            if (w.parts.size() > 1 && w.literal >= 3) gWild.push_back(std::move(w));
        };
        for (auto& [k, v] : gDict) {
            if (k.find("%s") != std::string::npos && v.find("%s") != std::string::npos) addWild(k, &v);
            else if (k.size() >= 4 && k.back() == ':' && v.back() == ':') {
                gColonForms.push_back(v + " %s");
                addWild(k + " %s", &gColonForms.back());
            }
        }
        // Longest literal text first: the most specific pattern wins.
        std::sort(gWild.begin(), gWild.end(), [](const Wildcard& a, const Wildcard& b) { return a.literal > b.literal; });
        gLang = code;
        return true;
    }
    return false;
}

std::string Tr(const std::string& french) {
    if (gView.empty()) return french;
    const std::string* t = Translate(french);
    return t ? *t : french;
}

const char* TrC(const char* french) {
    if (gView.empty() || !french) return french;
    const std::string* t = Translate(french);
    return t ? t->c_str() : french;
}

}  // namespace sx
