#include "mesh/Mesh.h"
#include <charconv>
#include <map>
#include <set>
#include <sstream>

namespace sx {
bool SizeTextureTiles(MeshData& mesh, int defaultSize, const std::string& overrides, int& atlasSize, std::string& error) {
    const bool udim = !mesh.udims.empty();
    const size_t count = udim ? mesh.udims.size() : mesh.uvTiles.size();
    std::map<std::string, int> sizes;
    std::istringstream lines(overrides);
    auto trim = [](std::string text) {
        const size_t beg = text.find_first_not_of(" \t\r");
        return beg == std::string::npos ? std::string{} : text.substr(beg, text.find_last_not_of(" \t\r") - beg + 1);
    };
    for (std::string line; std::getline(lines, line);) {
        line = trim(line);
        if (line.empty()) continue;
        const size_t eq = line.rfind('=');
        const std::string key = trim(line.substr(0, eq));
        const std::string n = eq == std::string::npos ? "" : trim(line.substr(eq + 1));
        int size = 0;
        const auto [end, ec] = std::from_chars(n.data(), n.data()+n.size(), size);
        if (key.empty() || ec != std::errc{} || end != n.data()+n.size() || size < 1 || size > 4096 || (size & (size-1)) || !sizes.emplace(key, size).second) {
            error = "Résolution de tuile invalide : " + line + " (nom = puissance de deux, de 1 à 4096).";
            return false;
        }
    }
    if (!count) {
        atlasSize = defaultSize;
        if (sizes.empty()) return true;
        if (sizes.size() == 1) {
            const auto& [key, size] = *sizes.begin();
            if (key == "1001" && std::all_of(mesh.vertices.begin(), mesh.vertices.end(), [](const MeshVertex& v) {
                return v.uv.x >= 0 && v.uv.x <= 1 && v.uv.y >= 0 && v.uv.y <= 1;
            })) {
                mesh.udims = {1001};
                mesh.udimCols = 1;
                atlasSize = size;
                return true;
            }
            const size_t material = mesh.triMaterial.empty() ? 0 : mesh.triMaterial.front();
            const bool single = std::all_of(mesh.triMaterial.begin(), mesh.triMaterial.end(), [&](u16 m) { return m == material; });
            if (single && material < mesh.materials.size() && key == mesh.materials[material].name) {
                mesh.uvTiles.assign(mesh.materials.size(), {});
                mesh.uvTiles[material] = {0,0,1,1};
                atlasSize = size;
                return true;
            }
        }
        error = "Tuile ou matériau introuvable : " + sizes.begin()->first;
        return false;
    }
    std::vector<int> native(count);
    std::set<std::string> matched;
    int largest = 0, cols = udim ? mesh.udimCols : 1;
    for (size_t i = 0; i < count; ++i) {
        if (!udim && mesh.uvTiles[i].z <= mesh.uvTiles[i].x) continue;
        native[i] = defaultSize;
        auto apply = [&](const std::string& key) {
            if (auto it = sizes.find(key); it != sizes.end()) {
                native[i] = it->second;
                matched.insert(key);
            }
        };
        const std::string number = udim ? std::to_string(mesh.udims[i]) : "";
        if (udim) apply(number);
        if (!udim || !mesh.udimMaterials.empty()) {
            const size_t index = udim ? mesh.udimMaterials[i] : i;
            const auto& name = mesh.materials[index].name;
            const std::string material = name.empty() ? "Materiau" + std::to_string(index+1) : name;
            apply(material);
            if (udim) apply(material + "." + number);
        }
        largest = std::max(largest, native[i]);
        if (!udim) cols = (int)std::lround(1.0 / (mesh.uvTiles[i].z - mesh.uvTiles[i].x));
    }
    for (const auto& [key, size] : sizes)
        if (!matched.contains(key)) { error = "Tuile ou matériau introuvable : " + key; return false; }
    if (cols <= 0 || largest <= 0 || cols > 16384 / largest) {
        error = "Les tuiles dépassent la taille maximale de l’atlas (16384 px). Réduisez leur résolution native.";
        return false;
    }
    atlasSize = cols * largest;
    if (std::none_of(native.begin(), native.end(), [&](int size){ return size > 0 && size < largest; })) return true;
    // Vertices shared across tile boundaries were split by PackUdims / TileMaterialUvs.
    std::vector<int> owners(mesh.vertices.size(), -1);
    for (size_t t = 0; t < mesh.indices.size()/3; ++t) {
        const u32* face = &mesh.indices[t*3];
        const int cell = udim ? UdimCellAt(mesh, (mesh.vertices[face[0]].uv + mesh.vertices[face[1]].uv + mesh.vertices[face[2]].uv) / 3.0f)
                              : (int)mesh.triMaterial[t];
        if (cell < 0 || cell >= (int)count || !native[cell]) { error = "Impossible d’associer une face à sa tuile."; return false; }
        for (int k = 0; k < 3; ++k) owners[face[k]] = cell;
    }
    for (size_t i = 0; i < owners.size(); ++i) {
        if (owners[i] < 0) continue;
        const int cell = owners[i];
        const float4 r = udim ? UdimRect(mesh, cell) : mesh.uvTiles[cell];
        const float2 origin{r.x,r.y};
        mesh.vertices[i].uv = origin + (mesh.vertices[i].uv - origin) * ((float)native[cell] / largest);
    }
    if (udim) mesh.udimScale.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const float scale = (float)native[i] / largest;
        if (udim) mesh.udimScale[i] = scale;
        else if (native[i]) {
            auto& r = mesh.uvTiles[i];
            r.z = r.x + (r.z-r.x)*scale;
            r.w = r.y + (r.w-r.y)*scale;
        }
    }
    return true;
}
}  // namespace sx
