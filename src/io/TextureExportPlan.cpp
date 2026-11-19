#include "io/TextureExportPlan.h"
#include <windows.h>
#include <set>

namespace sx::texexport {
namespace {
struct WindowsFileLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return CompareStringOrdinal(a.data(), (int)a.size(), b.data(), (int)b.size(), TRUE) == CSTR_LESS_THAN;
    }
};
constexpr int dx[] = {-1,0,1,-1,1,-1,0,1}, dy[] = {-1,-1,-1,0,0,1,1,1};

bool ValidComponent(const std::wstring& s) {
    if (s.empty() || s.back() == L'.' || s.back() == L' ' || s.find_first_of(L"<>:\"|?*") != s.npos) return false;
    if (std::any_of(s.begin(), s.end(), [](wchar_t c){ return c < 32; })) return false;
    std::wstring stem = s.substr(0, s.find(L'.'));
    for (auto& c : stem) if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
    return stem != L"CON" && stem != L"NUL" && stem != L"PRN" && stem != L"AUX" &&
           !(stem.size() == 4 && (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) && stem[3] >= L'1' && stem[3] <= L'9');
}
}

std::pair<std::string, std::string> SetSuffixes(const std::string& suffix) {
    const auto dot = suffix.find('.');
    if (dot == suffix.npos) return {suffix, {}};
    return {suffix.substr(0, dot), suffix.substr(dot)};
}

std::string PlanOutputs(const std::vector<std::pair<std::string, Rect>>& parts, const std::vector<Map>& maps,
                        const std::string& pattern, const std::string& base, const std::string& ext,
                        std::vector<Output>& outputs, const std::vector<std::vector<bool>>& enabledMaps) {
    outputs.clear();
    std::set<std::wstring, WindowsFileLess> names, directories;
    std::vector<Output> result;
    for (int p = 0; p < (int)parts.size(); ++p) {
        const auto [material, udim] = SetSuffixes(parts[p].first);
        for (int m = 0; m < (int)maps.size(); ++m) {
            if (!enabledMaps.empty() && !enabledMaps[p][m]) continue;
            std::string file;
            for (size_t i = 0; i < pattern.size();) {
                if (pattern.compare(i, 5, "{nom}") == 0) { file += base + material; i += 5; }
                else if (pattern.compare(i, 7, "{carte}") == 0) { file += maps[m].suffix; i += 7; }
                else file += pattern[i++];
            }
            // Always identify the set even with a custom pattern that omits the base name.
            if (pattern.find("{nom}") == pattern.npos) file += material;
            file += udim;
            if (file.empty()) return "Nom de fichier vide.";
            file += ext;
            const auto path = Utf8Path(file).make_preferred();
            if (path.has_root_path() || std::any_of(path.begin(), path.end(), [](const auto& part){ return !ValidComponent(part.native()); }))
                return "Nom de fichier invalide : " + file;
            const auto normalized = path.lexically_normal();
            if (directories.contains(normalized.native()) || !names.insert(normalized.native()).second)
                return "Plusieurs sorties utilisent « " + file + " ». Modifiez le motif, le nom du matériau ou celui de la carte.";
            for (auto parent = normalized.parent_path(); !parent.empty(); parent = parent.parent_path()) {
                if (names.contains(parent.native()))
                    return "Un fichier et un dossier portent le même nom : " + Narrow(parent.wstring());
                directories.insert(parent.native());
            }
            result.push_back({p, m, std::move(file)});
        }
    }
    outputs = std::move(result);
    return {};
}

std::string ValidateSetNames(const std::vector<std::pair<std::string, Rect>>& parts, const std::string& base) {
    // The legacy fixed-channel exporters append the same distinct map / script suffixes to each set.
    std::vector<Output> outputs;
    return PlanOutputs(parts, {{"BaseColor", {}}}, "{nom}_{carte}", base, ".png", outputs);
}

std::vector<PaddingPixel> BuildPadding(std::vector<u8> covered, int w, int h, int radius) {
    std::vector<PaddingPixel> plan;
    if (radius <= 0 || covered.size() != (size_t)w * h) return plan;
    // 0 = already covered, 255 = unreached. Later rings use only earlier rings.
    for (auto& v : covered) v = v ? 0 : 255;
    auto visit = [&](int i, int distance) {
        if (covered[i] != 255) return;
        u8 neighbors = 0;
        const int x = i % w, y = i / w;
        for (int k = 0; k < 8; ++k) {
            const int nx = x + dx[k], ny = y + dy[k];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h && covered[ny * w + nx] < distance)
                neighbors |= (u8)(1u << k);
        }
        if (neighbors) {
            plan.push_back({i, neighbors});
            covered[i] = (u8)distance;
        }
    };
    for (int i = 0; i < w * h; ++i) visit(i, 1);
    size_t start = 0;
    for (int distance = 2; distance <= std::min(radius, 254) && start < plan.size(); ++distance) {
        const size_t end = plan.size();
        for (; start < end; ++start) {
            const int i = plan[start].index, x = i % w, y = i / w;
            for (int k = 0; k < 8; ++k) {
                const int nx = x + dx[k], ny = y + dy[k];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) visit(ny * w + nx, distance);
            }
        }
    }
    return plan;
}

void ApplyPadding(const std::vector<PaddingPixel>& padding, int w, std::vector<float>& rgba) {
    for (const auto& p : padding) {
        float sum[4] = {};
        int count = 0;
        for (int k = 0; k < 8; ++k) {
            if (!(p.neighbors & (1u << k))) continue;
            const float* source = &rgba[(size_t)(p.index + dy[k] * w + dx[k]) * 4];
            for (int c = 0; c < 4; ++c) sum[c] += source[c];
            ++count;
        }
        for (int c = 0; c < 4; ++c) rgba[(size_t)p.index * 4 + c] = sum[c] / count;
    }
}
}  // namespace sx::texexport
